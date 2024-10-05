#include "hw2.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void print_packet(unsigned int packet[]) {
    printf("Packet Type: %s\n", ((packet[0] >> 30) & 0x3) == 1 ? "Write" : "Read");
    printf("Address: %u\n", packet[2] & 0xFFFFFFFF);
    printf("Length: %u\n", packet[0] & 0x3FF);
    printf("Requester ID: %u\n", (packet[1] >> 16) & 0xFFFF);
    printf("Tag: %u\n", (packet[1] >> 8) & 0xFF);
    printf("Last BE: %u\n", (packet[1] >> 4) & 0xF);
    printf("1st BE: %u\n", packet[1] & 0xF);

    if (((packet[0] >> 30) & 0x3) == 1) {
        printf("Data: ");
        for (unsigned int i = 3; i < (packet[0] & 0x3FF) + 3; i++) {
            printf("%d ", packet[i] & 0xFFFFFFFF);
        }
        printf("\n");
    } else {
        printf("Data: \n");
    }
}

void store_values(unsigned int packets[], char *memory) {
    int packetStart = 0;
    int payloadIndex = packetStart + 3;

    while (true) {
        if (((packets[packetStart] >> 10) & 0x3FFFFF) != 0x100000) {
            break;
        } else {
            unsigned int address = packets[packetStart + 2] & 0xFFFFFFFF;
            unsigned int length = packets[packetStart] & 0x3FF;

            unsigned int last_be = (packets[packetStart + 1] >> 4) & 0xF;
            unsigned int first_be = packets[packetStart + 1] & 0xF;

            if (address > 0x100000) {
                packetStart += 4 + length;
                payloadIndex = packetStart + 3;
            } else {
                for (unsigned int i = 0; i < length; i++) {
                    if (i == 0) {
                        for (int j = 0; j < 4; j++) {
                            if (first_be & (1 << j)) {
                                unsigned int byte_value = (packets[payloadIndex] >> (j * 8)) & 0xFF;
                                memory[address] = byte_value;
                                address++;
                            } else {
                                address++;
                            }
                        }
                        payloadIndex++;
                    } else if (i == length - 1) {
                        for (int j = 0; j < 4; j++) {
                            if (last_be & (1 << j)) {
                                unsigned int byte_value = (packets[payloadIndex] >> (j * 8)) & 0xFF;
                                memory[address] = byte_value;
                                address++;
                            } else {
                                address++;
                            }
                        }
                        payloadIndex++;
                    } else {
                        for (int j = 0; j < 4; j++) {
                            int byte_value = (packets[payloadIndex] >> (j * 8) & 0xFF);
                            memory[address] = byte_value;
                            address++;
                        }
                        payloadIndex++;
                    }
                }
                packetStart += 3 + length;
                payloadIndex = packetStart + 3;
            }
        }
    }
}

unsigned int *create_completion(unsigned int packets[], const char *memory) {
    /* --- CALCULATE THE AMOUNT OF INT ELEMENTS (THAT I NEED TO ALLOCATE SPACE FOR) ---*/
    unsigned int packets_iterator = 0;  // index pointing at the start of a read packet
    unsigned int total_elements = 0;    // bytes to allocate for completions

    print_packet(packets);

    while (true) {
        if (((packets[packets_iterator] >> 10) & 0x3FFFFF) != 0x0) {
            break;
        }

        unsigned int packet_address = packets[packets_iterator + 2];
        unsigned int packet_length = packets[packets_iterator] & 0x3FF;
        unsigned int boundary = (packet_address % 0x4000 + (packet_length - 1) * 4) / 0x4000;
        total_elements += 3 + packet_length + (3 * boundary);
        packets_iterator += 3;
    }

    /* --- USE `TOTAL_ELEMENTS` TO ALLOCATE THE APPROPRIATE AMOUNT OF SPACE --- */
    unsigned int *completions = malloc((total_elements * sizeof(unsigned int)) + 100);
    for (unsigned int i = 0; i < total_elements; i++) {
        completions[i] = 0;
    }

    /* --- ITERATE THROUGH THE PACKETS IN `PACKETS[]` --- */
    unsigned int read_start = 0;              // index that point at the start of each read packet
    unsigned int completion_start_index = 0;  // index that point at the start of each completion packet

    while (true) {
        /* --- IF THE PACKET IS NOT A READ TLP, BREAK --- */
        if (((packets[read_start] >> 10) & 0x3FFFFF) != 0x0) {
            break;
        }
        /* --- EXTRACT INFORMATION FROM THE CURRENT PACKET IN QUESTION --- */
        unsigned int read_address = packets[read_start + 2];
        unsigned int read_length = packets[read_start] & 0x3FF;
        unsigned int requester_id = (packets[read_start + 1] >> 16) & 0xFFFF;
        unsigned int tag = (packets[read_start + 1] >> 8) & 0xFF;
        // unsigned int last_be = (packets[r_start + 1] >> 4) & 0xF;
        // unsigned int first_be = packets[r_start + 1] & 0xF;

        /* Generate variables for use in Completion packets */
        unsigned int memory_address = read_address;
        unsigned int completion_length = 0;
        unsigned int byte_count = read_length * 4;
        unsigned int lower_address = memory_address & 0x7F;
        unsigned int completion_payload_index = completion_start_index + 3;

        /* --- FOR EACH INT, READ 4 DATA VALUES INTO IT --- */
        for (unsigned int i = 0; i < read_length; i++) {
            for (int j = 0; j < 4; j++) {
                completions[completion_payload_index] |= ((unsigned char)memory[memory_address]) << (j * 8);
                memory_address++;
            }

            completion_length++;        /* Incrememnt the length every time one data payload is done being read */
            completion_payload_index++; /* INncrement to the next payload index to enter the next 4 data values */

            if (memory_address % 0x4000 == 0) {
                /* --- MAKE A COMPLETION PACKET --- */
                completions[completion_start_index] |= 0x4A << 24;             /* Type */
                completions[completion_start_index] |= completion_length;      /* Length */
                completions[completion_start_index + 2] |= requester_id << 16; /* Requestor ID */
                completions[completion_start_index + 1] |= (unsigned int) 220 << 16;         /* Completer ID */
                completions[completion_start_index + 2] |= tag << 8;           /* Tag */
                completions[completion_start_index + 1] |= byte_count;         /* Byte Count*/
                completions[completion_start_index + 2] |= lower_address;      /* Lower Address */

                printf("%u\n", completions[completion_start_index]);
                printf("%u\n", completions[completion_start_index + 1]);
                printf("%u\n", completions[completion_start_index + 2]);

                /* CHANGE THE VALUES OF SOME VARIABLES FOR THE SECOND PACKET */
                byte_count -= completion_length * 4;
                lower_address = memory_address & 0x7F;
                completion_length = 0;
                completion_start_index = completion_payload_index;
                completion_payload_index += 3;
            }
        }

        /* --- MAKE A COMPLETION PACKET --- */
        completions[completion_start_index] |= 0x4A << 24;             /* Type */
        completions[completion_start_index] |= completion_length;      /* Length */
        completions[completion_start_index + 2] |= requester_id << 16; /* Requestor ID */
        completions[completion_start_index + 1] |= 0xDC << 16;         /* Completer ID */
        completions[completion_start_index + 2] |= tag << 8;           /* Tag */
        completions[completion_start_index + 1] |= byte_count;         /* Byte Count*/
        completions[completion_start_index + 2] |= lower_address;      /* Lower Address */

        printf("%u\n", completions[completion_start_index]);
        printf("%u\n", completions[completion_start_index + 1]);
        printf("%u\n", completions[completion_start_index + 2]);

        /* --- ASSIGN THE NEW CCOMPLETION_START AND COMPLETION_PAYLOAD (START OF THE NEXT COMPLETION PACKET) --- */
        completion_start_index = completion_payload_index;
        completion_payload_index += 3;
        /* --- GO TO THE NEXT READ PACKET --- */
        read_start += 3;
    }
    return completions;
}
