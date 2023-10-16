// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"


clock_t time_start, time_end;
double total_time;
struct stat stats;

int transmitting(const char* file){
    time_start = clock();

    if (stat(file, &stats) < 0)
    {
        perror("Error reading file stats");
        return -1;
    }

    FILE *fptr = fopen(file, "rb");

    unsigned L1 = sizeof(stats.st_size);
    unsigned L2 = strlen(file);
    unsigned packet_size = 5 + L1 + L2;

    unsigned char packet[packet_size];
    packet[0] = 2;
    packet[1] = 0;
    packet[2] = L1;
    memcpy(&packet[3], &stats.st_size, L1);
    packet[3 + L1] = 1;
    packet[4 + L1] = L2;
    memcpy(&packet[5 + L1], file, L2);

    if (llwrite(packet, packet_size) < 0)
        return -1;

    unsigned char buf[MAX_SIZE];
    unsigned bytes_to_send;
    unsigned sequenceNumber = 0;

    while ((bytes_to_send = fread(buf, sizeof(unsigned char), MAX_SIZE - 4, fptr)) > 0)
    {
        printf("MIDLE PACKET\n");
        unsigned char dataPacket[MAX_SIZE];
        dataPacket[0] = 1;
        dataPacket[1] = sequenceNumber % 255;
        dataPacket[2] = (bytes_to_send / 256);
        dataPacket[3] = (bytes_to_send % 256);
        memcpy(&dataPacket[4], buf, bytes_to_send);

        llwrite(dataPacket, ((bytes_to_send + 4) < MAX_SIZE) ? (bytes_to_send + 4) : MAX_SIZE);
        printf("Sent %dº data package\n", sequenceNumber);
        sequenceNumber++;
    }
    L1 = sizeof(stats.st_size);
    L2 = strlen(file);
    packet_size = 5 + L1 + L2;

    packet[0] = 3;
    packet[1] = 0;
    packet[2] = L1;
    memcpy(&packet[3], &stats.st_size, L1);
    packet[3 + L1] = 1;
    packet[4 + L1] = L2;
    memcpy(&packet[5 + L1], file, L2);

    if (llwrite(packet, packet_size) < 0)
        return -1;

    fclose(fptr);

    time_end = clock();
    total_time = (double)(time_end - time_start) / CLOCKS_PER_SEC;

    printf("\nTotal time taken: %f seconds\n", total_time);
    printf("Size transfered: %d bytes\n", (int) stats.st_size);
    printf("Transfer Speed: %f B/s\n\n", stats.st_size/total_time);
    return 0;
}

int receiving(const char* file){
    static FILE *dest;
    while(1)
    {
        unsigned char buf[MAX_SIZE];
        if ((llread(buf)) < 0) { continue; } 
        if (buf[0] == 1) { dest = fopen(file, "wb"); }
        else if (buf[0] == 2) {
            if (buf[1] != 0) { return -1; }
            unsigned int data_size = buf[2] * 256 + buf[3];
            fwrite(&buf[4], sizeof(unsigned char), data_size * sizeof(unsigned char), dest);
        }
        else if (buf[0] == 3) {
            fclose(dest);
            break;
        }
    }
    return 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer link_layer;
    link_layer.baudRate = baudRate;
    link_layer.nRetransmissions = nTries;
    link_layer.timeout = timeout;

    if (strcmp(role, "tx")) link_layer.role = LlTx;
    else if (strcmp(role, "rx")) link_layer.role = LlRx;
    if (llopen(link_layer) < 0){
        printf("Error in llopen()");
        exit(-1);
        return;
    } 

    if(strcmp(role, "tx")) {
        if (transmitting(filename)) {
            printf("Error in transmitting()");
            exit(-1);
            return;
        }
    }    
    else if (strcmp(role, "rx")) {
        if (receiving(filename)) {
            printf("Error in receiving()");
            exit(-1);
            return;
        }
    }    
    if (llclose(0)) {
        printf("Error in llclose()");
        exit(-1);
        return;
    }
}
