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

void store_values(unsigned int packets[], char *memory)
{
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
            printf("Address: %u, Length: %u, Last BE: %u, First BE: %u ----------\n", address, length, last_be, first_be);

            // Check if the address is greater than 1MB
            if (address > 0x100000) {
                printf("Address Size is above 1MB\n");
                // Update packetStart to move to the next packet
                printf("Previous value of packetStart: %d\n", packetStart);
                packetStart += 4 + length;
                printf("Current value of packetStart: %d\n", packetStart);
                
                // Update packetsIndex to reflect the new packetStart
                printf("Previous value of packetsIndex: %d\n", payloadIndex);
                payloadIndex = packetStart + 3;
                printf("Current value of packetsIndex: %d\n", payloadIndex);

            } 
            // If the address is not greater than 1MB, handle the packet accordingly
            else {
                printf("Packet type: Hex %X\n", packets[packetStart]);
                printf("PacketStart: %d\n", packetStart);
                printf("--------- Start Iteration through PAYLOAD --------\n");
                printf("Payload Index: %d || Payload Value: %d (Hex: %X)\n", payloadIndex, packets[payloadIndex], packets[payloadIndex]);
                // Iterate through every data payload
                for (int i = 0; i < length; i++) {
                    if (i == 0) {
                        for (int j = 0; j < 4; j++) {
                            printf("Value of bit %d in the FIRST_BE is: %d\n", j, first_be & (1 << j));
                            if (first_be & (1 << j)) {
                                printf("Bit Manipulated Payload Value: %d (Hex: %X)\n", (packets[payloadIndex] >> (j * 8)) & 0xFF, (packets[payloadIndex] >> (j * 8)) & 0xFF);
                                unsigned int byte_value = (packets[payloadIndex] >> (j * 8)) & 0xFF;
                                printf("BYTE_VALUE: %d\n", byte_value);
                                printf("Writing to memory address: %u\n", address);
                                memory[address] = byte_value;
                                printf("MEMORY[ADDRESS]: Hex %X\n", memory[address], memory[address]);
                                address++;
                            } else {
                                printf("Writing to memory address: %u\n", address);
                                memory[address] = 0x00;
                                printf("MEMORY[ADDRESS]: Hex %X)\n", memory[address], memory[address]);
                                address++;
                            }
                        }
                        payloadIndex++;
                        printf("Payload index: %d\n", payloadIndex);
                    } else if (i == length - 1) {
                        for (int j = 0; j < 4; j++) {
                            if (last_be & (1 << j)) {
                                unsigned int byte_value = (packets[payloadIndex] >> (j * 8)) & 0xFF;
                                memory[address] = byte_value;
                                address++;
                            } else {
                                memory[address] = 0x00;
                                address++;
                            }
                        }
                        payloadIndex++;
                        printf("PAYLOADINDEX after storing the last byte: %d\n", payloadIndex);
                    } else {
                        for (int j = 0; j < 4; j++) {
                            int byte_value = (packets[payloadIndex] >> (j * 8) & 0xFF);
                            memory[address] = byte_value;
                            address++;
                        }
                        payloadIndex++;
                        printf("Payload Index: %d\n", payloadIndex);
                    }
                } 
                
                packetStart += 3 + length;
                payloadIndex = packetStart + 3;
                printf("End loop\n");
                printf("New PacketStart: %d, New PayloadIndex: %d\n", packetStart, payloadIndex);
            }
        }
    }
        
}

unsigned int* create_completion(unsigned int packets[], const char *memory)
{
    (void)packets;
    (void)memory;
	return NULL;
}
