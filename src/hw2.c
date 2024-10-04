#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "hw2.h"

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

unsigned int* create_completion(unsigned int packets[], const char *memory) {   
    /* --- CALCULATE THE AMOUNT OF INT ELEMENTS (THAT I NEED TO ALLOCATE SPACE FOR) ---*/
    bool crosses_boundary = false;
    unsigned int packets_iterator = 0;
    unsigned int total_elements = 0;

    while (true) {
        if ((packets[packets_iterator] >> 10) & 0x3FFFFF != 0x0) {
            break;
        } else {
            unsigned int packet_address = packets[packets_iterator + 2] & 0xFFFFFFFC;
            unsigned int packet_length = packets[packets_iterator] & 0x3FF;
            unsigned int boundary = (0x4000 - (packet_address % 0x4000)) + packet_address;
            if (packet_address + (packet_length * 4) >= boundary) {
                total_elements += (6 + packet_length);
                crosses_boundary = true;
            } else {
                total_elements += (3 + packet_length);
                crosses_boundary = false;
            }
            packets_iterator += 3;
        }
    }
    
    /* --- USE `TOTAL_ELEMENTS` TO ALLOCATE THE APPROPRIATE AMOUNT OF SPACE --- */
    unsigned int * completions = malloc(total_elements * sizeof(unsigned int));
    for (int i = 0; i < total_elements; i++) {completions[i] = 0;}

    /* --- ITERATE THROUGH THE PACKETS IN `PACKETS[]` --- */
    unsigned int r_start = 0;
    unsigned int c_start = 0;

    while (true) {
        /* --- IF THE PACKET IS NOT A READ TLP, BREAK --- */
        if (((packets[r_start] >> 10) & 0x3FFFFF) != 0x0) {
            break;
        /* --- IF THE PACKET IS A READ TLP, PROCESS --- */
        } else {
            /* --- EXTRACT INFORMATION FROM THE CURRENT PACKET IN QUESTION --- */
            unsigned int r_address = packets[r_start + 2] & 0xFFFFFFFC;
            unsigned int r_length = packets[r_start] & 0x3FF;
            unsigned int requester_id = (packets[r_start + 1] >> 16) & 0xFFFF;
            unsigned int tag = (packets[r_start + 1] >> 8) & 0xFF;
            unsigned int last_be = (packets[r_start + 1] >> 4) & 0xF;
            unsigned int first_be = packets[r_start +1] & 0xF;

            /* Generate variables for use in Completion packets */
            unsigned int c_length = r_length;
            unsigned int byte_count = (r_length * 4) /* - disabled_bytes */ ;
            unsigned int lower_address = r_address & 0x7F;
            unsigned int c_data_idx = c_start + 3;

            /* --- MAKE A COMPLETION PACKET --- */
            completions[c_start] |= 0x4A << 24; /* Type */
            completions[c_start] |= (r_length); /* Length */
            completions[c_start + 2] |= (requester_id) << 16; /* Requestor ID */
            completions[c_start + 1] |= 0xdc << 16; /* Completer ID */
            completions[c_start + 2] |= (tag) << 8; /* Tag */
            completions[c_start + 1] |= (r_length * 4); /* Byte Count*/
            completions[c_start + 2] |= lower_address; /* Lower Address */

            /* --- FOR EACH INT, READ 4 DATA VALUES INTO IT --- */
            unsigned int mem_address = r_address;
            for (int i = 0; i < r_length; i++) {
                completions[c_data_idx] = 0;
                for (int j = 0; j < 4; j++) {
                    completions[c_data_idx] |= ((unsigned char) memory[mem_address]) << (j * 8);
                    mem_address++;;
                }
                c_data_idx++;
            }
            /* --- ASSIGN THE NEW C_START (START OF THE NEXT COMPLETION PACKET) --- */
            c_start = c_data_idx;
            /* --- GO TO THE NEXT READ PACKET --- */
            r_start += 3;
        }
    }

	return completions;
}
