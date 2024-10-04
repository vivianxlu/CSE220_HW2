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
    } else { printf("Data: \n"); }
}

void store_values(unsigned int packets[], char *memory) {
    int packetStart = 0;
    int payloadIndex = packetStart + 3;

    while (true) {
        if (((packets[packetStart] >> 10) & 0x3FFFFF) != 0x100000) {
            return;
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
                            } else { address++; }
                        }
                        payloadIndex++;
                    } else if (i == length - 1) {
                        for (int j = 0; j < 4; j++) {
                            if (last_be & (1 << j)) {
                                unsigned int byte_value = (packets[payloadIndex] >> (j * 8)) & 0xFF;
                                memory[address] = byte_value;
                                address++;
                            } else { address++; }
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
    unsigned int packets_iterator = 0; // index pointing at the start of a read packet
    unsigned int total_elements = 0; // bytes to allocate for completions

    while (true) {
        if (((packets[packets_iterator] >> 10) & 0x3FFFFF) != 0x0) { break; }

        unsigned int packet_address = packets[packets_iterator + 2];
        unsigned int packet_length = packets[packets_iterator] & 0x3FF;
        unsigned int boundary = (packet_address % 0x4000 + (packet_length - 1) * 4) / 0x4000;
        total_elements +=  3 + packet_length + (3 * boundary);
        packets_iterator += 3;
    }

    /* --- USE `TOTAL_ELEMENTS` TO ALLOCATE THE APPROPRIATE AMOUNT OF SPACE --- */
    unsigned int *completions = malloc(total_elements * sizeof(unsigned int));
    for (unsigned int i = 0; i < total_elements; i++) { completions[i] = 0; }

    /* --- ITERATE THROUGH THE PACKETS IN `PACKETS[]` --- */
    unsigned int r_start = 0; // index that point at the start of each read packet
    unsigned int c_start = 0; // index that point at the start of each completion packet

    while (true) {
        /* --- IF THE PACKET IS NOT A READ TLP, BREAK --- */
        if (((packets[r_start] >> 10) & 0x3FFFFF) != 0x0) { break; }
        /* --- EXTRACT INFORMATION FROM THE CURRENT PACKET IN QUESTION --- */
        unsigned int r_address = packets[r_start + 2];
        unsigned int r_length = packets[r_start] & 0x3FF;
        unsigned int requester_id = (packets[r_start + 1] >> 16) & 0xFFFF;
        unsigned int tag = (packets[r_start + 1] >> 8) & 0xFF;
        // unsigned int last_be = (packets[r_start + 1] >> 4) & 0xF;
        // unsigned int first_be = packets[r_start + 1] & 0xF;

        /* Generate variables for use in Completion packets */
        unsigned int mem_address = r_address;
        unsigned int c_length = 0; // need to change
        unsigned int byte_count = r_length * 4;
        unsigned int lower_address = mem_address & 0x7F;
        unsigned int c_data_idx = c_start + 3;
        printf("Initial State of the Variables:\n mem_address = Hex %x, c_length = Hex %x, byte_count = Hex %x, lower_address = Hex %x, c_data_idx = Hex %x\n", mem_address, c_length, byte_count, lower_address, c_data_idx);

        /* --- FOR EACH INT, READ 4 DATA VALUES INTO IT --- */
        for (unsigned int i = 0; i < r_length; i++) {
            // printf("Start iteration %u of outer_loop\n", i);
            for (int j = 0; j < 4; j++) {
                // printf("Start iteration %u of inner_loop\n", j);
                completions[c_data_idx] |= ((unsigned char)memory[mem_address]) << (j * 8);
                printf("Value stored in completions: %d\n", completions[c_data_idx]);
                mem_address++;
                printf("Updated mem_address = %d\n", mem_address);
            }
            
            c_length++; /* Incrememnt the length every time one data payload is done being read */
            printf("Updated c_length = Hex %x\n", c_length);
            c_data_idx++; /* The payload index that I'm looking at rn */
            printf("Updated c_data_idx = Hex %x\n", c_data_idx);

            if (mem_address % 0x4000 == 0) {
                printf("mem_address = Hex %x, REACHED BOUNDARY\n", mem_address);
                /* --- MAKE A COMPLETION PACKET --- */
                completions[c_start] |= 0x4A << 24;               /* Type */
                // printf("Type: %u\n", (completions[c_start] >> 10) & 0x3FFFFF);
                completions[c_start] |= c_length;                 /* Length */
                // printf("Length: %u\n", completions[c_start] & 0x3FF);
                completions[c_start + 2] |= (requester_id) << 16; /* Requestor ID */
                // printf("Requester ID: %u\n", (completions[c_start + 2] >> 16) & 0xFFFF);
                completions[c_start + 1] |= 0xDC << 16;           /* Completer ID */
                // printf("Completer ID: %u\n", (completions[c_start + 1] >> 16) & 0xFFFF);
                completions[c_start + 2] |= tag << 8;           /* Tag */
                // printf("Tag: %u\n", (completions[c_start + 2] >> 8) & 0xFF);
                completions[c_start + 1] |= byte_count;       /* Byte Count*/
                // printf("Byte Count: %u\n", completions[c_start + 1] & 0xFFF);
                completions[c_start + 2] |= lower_address;        /* Lower Address */
                // printf("Lower Address: %u\n", completions[c_start + 2] & 0x7F);
                
                /* --- ASSIGN THE NEW C_START (START OF THE NEXT COMPLETION PACKET) --- */
                c_start = c_data_idx;
                /* Adjust some values for the second completion packet that will be created after this */
                lower_address = mem_address & 0x7f;
                byte_count -= (c_length * 4);
                c_length = 0;
            } 
            // printf("End iteration %u of the outer loop\n", i);        
        }
        // printf("We are outside the loop because the boundary was not crossed\n");
        /* --- MAKE A COMPLETION PACKET --- */
        completions[c_start] |= 0x4A << 24;               /* Type */
        completions[c_start] |= c_length;                 /* Length */
        completions[c_start + 2] |= requester_id << 16; /* Requestor ID */
        completions[c_start + 1] |= 0xdc << 16;           /* Completer ID */
        completions[c_start + 2] |= tag << 8;           /* Tag */
        completions[c_start + 1] |= byte_count;           /* Byte Count*/
        completions[c_start + 2] |= lower_address;        /* Lower Address */

        /* --- ASSIGN THE NEW C_START (START OF THE NEXT COMPLETION PACKET) --- */
        c_start = c_data_idx;
        /* --- GO TO THE NEXT READ PACKET --- */
        r_start += 3;
    }
    return completions;
}
