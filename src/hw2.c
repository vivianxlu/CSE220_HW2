#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "hw2.h"

void print_packet(unsigned int packet[])
{
    /* 
    int packet_type = (packet[0] >> 30) & 0x3;
    unsigned int address = (packet[2]) & 0xFFFFFFFF;
    int length = (packet[0]) & 0x3FF;
    int requestor_id = (packet[1] >> 16) & 0xFFFF;
    int tag = (packet[1] >> 8) & 0xFF;
    int last_be = (packet[1] >> 4) & 0xF;
    int first_be = (packet[1]) & 0xF;
    */
    printf("Packet Type: %s\n", ((packet[0] >> 30) & 0x3) == 1 ? "Write" : "Read");
    printf("Address: %u\n", packet[2] & 0xFFFFFFFF);
    printf("Length: %u\n", packet[0] & 0x3FF);
    printf("Requester ID: %u\n", (packet[1] >> 16) & 0xFFFF);
    printf("Tag: %u\n", (packet[1] >> 8) & 0xFF);
    printf("Last BE: %u\n", (packet[1] >> 4) & 0xF);
    printf("1st BE: %u\n", packet[1] & 0xF);
    
    if (((packet[0] >> 30) & 0x3) == 1) { /* Check if the Packet Type = "Write"*/
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
            // Extract address and length
            unsigned int address = packets[packetStart + 2] & 0xFFFFFFFF;
            unsigned int length = packets[packetStart] & 0x3FF;
            // Extract byte enable values
            unsigned int last_be = (packets[packetStart + 1] >> 4) & 0xF;
            unsigned int first_be = packets[packetStart + 1] & 0xF;

            // Check if the address is greater than 1MB
            if (address > 0x100000) {
                // Update packetStart to move to the next packet
                packetStart += 4 + length;
                // Update packetsIndex to reflect the new packetStart
                payloadIndex = packetStart + 3;
            } 
            // If the address is not greater than 1MB, handle the packet accordingly
            else {
                // Iterate through every data payload
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

unsigned int* create_completion(unsigned int packets[], const char *memory)
{
    /* Extract information from packets[] */
    // unsigned int r_type = (packets[0] >> 10 & 0x3FFFFF);
    unsigned int r_address = packets[2] & 0xfffffffc;
    unsigned int r_length = packets[0] & 0x3FF;
    unsigned int requester_id = (packets[1] >> 16) & 0xFFFF;
    unsigned int tag = (packets[1] >> 8) & 0xFF;
    unsigned int last_be = (packets[1] >> 4) & 0xF;
    unsigned int first_be = packets[1] & 0xF;

    /* Check the number of disabled bytes, so I can have a proper ByteCount */
    int disabled_bytes = 0;
    for (int i = 0; i < 4; i++) {
        if (!(first_be & (1 << i))) {
            disabled_bytes++;
        }
        if (!(last_be & (1 << i))) {
            disabled_bytes++;
        }
    }

    /* Create an array for Completion packets (Initialize with the size of first packet) */
    unsigned int *completions = malloc((3 + r_length) * sizeof(unsigned int));

    /* Generate variables for use in Completion packets */
    unsigned int c_length = r_length;
    unsigned int byte_count = (r_length * 4) - disabled_bytes;
    unsigned int lower_address = r_address & 0x7F;

    int c_start = 0;
    int c_data_start = c_start + 3;

        completions[0] = 0;
        completions[1] = 0;
        completions[2] = 0;
        /* Make one completion packet */
        completions[c_start] |= 0x4A << 24; /* Type */
        completions[c_start] |= (r_length); /* Length */
        completions[c_start + 2] |= (requester_id) << 16; /* Requestor ID */
        completions[c_start + 1] |= 0xdc << 16; /* Completer ID */
        completions[c_start + 2] |= (tag) << 8; /* Tag */
        completions[c_start + 1] |= (r_length * 4); /* Byte Count*/
        completions[c_start + 2] |= lower_address; /* Lower Address */

        /* ----- Extract data from memory ----- */
        unsigned int mem_address = r_address;
        /* For-Loop || Loop through the Length */
        for (int i = 0; i < r_length; i++) {
            /* 4 Times per Length */
            completions[c_data_start] = 0;
            printf("%d:\n", i);
            for (int j = 0; j < 4; j++) {
                /* Assign the initial address to the memory address */
                completions[c_data_start] |=  ((unsigned char) memory[mem_address]) << (j * 8);
                printf("value: %d\n", memory[mem_address]);
                mem_address++;
                
                printf("payload: %x\n", completions[c_data_start]);
            }
            c_data_start++;
        }
        
	return completions;
}
