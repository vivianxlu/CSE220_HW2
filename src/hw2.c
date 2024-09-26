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
    printf("Length: %d\n", packet[0] & 0x3FF);
    printf("Requester ID: %u\n", (packet[1] >> 16) & 0xFFFF);
    printf("Tag: %u\n", (packet[1] >> 8) & 0xFF);
    printf("Last BE: %u\n", (packet[1] >> 4) & 0xF);
    printf("1st BE: %u\n", packet[1] & 0xF);
    
    if (((packet[0] >> 30) & 0x3) == 1) {
        printf("Data: ");
        for (unsigned int i = 3; i < (packet[0] & 0x3FF) + 3; i++) {
            printf("%d", packet[i] & 0xFFFFFFFF);
            if (i < (packet[0] & 0x3FF) + 2) {
                printf(" ");
            }
        }
        printf("\n");
    } else {
        printf("Data:\n");
    }
}

void store_values(unsigned int packets[], char *memory)
{
    (void)packets;
    (void)memory;
}

unsigned int* create_completion(unsigned int packets[], const char *memory)
{
    (void)packets;
    (void)memory;
	return NULL;
}
