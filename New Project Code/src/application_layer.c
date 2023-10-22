// Application layer protocol implementation

#include "application_layer.h"
#include <signal.h>
#include <stdlib.h>

struct stat file_stat;
// clock_t start_t, end_t;
// double total_t;


int transmitData(const char *filename) {
    // start_t = clock();

    if (stat(filename, &file_stat) < 0) {
        perror("Error getting file information.");
        return -1;
    }

    FILE *filePointer = fopen(filename, "rb");

    unsigned fileSizeLength = sizeof(file_stat.st_size);
    unsigned filenameLength = strlen(filename);
    unsigned packetSize = 5 + fileSizeLength + filenameLength;

    unsigned char packet[packetSize];
    packet[0] = STARTING_PACKET;
    packet[1] = FILE_SIZE;
    packet[2] = fileSizeLength;
    memcpy(&packet[3], &file_stat.st_size, fileSizeLength);
    packet[3 + fileSizeLength] = FILE_NAME;
    packet[4 + fileSizeLength] = filenameLength;
    memcpy(&packet[5 + fileSizeLength], filename, filenameLength);

    if (llwrite(packet, packetSize) < 0)
        return -1;

    printf("Starting packet sent\n");

    unsigned char buf[MAX_SIZE];
    unsigned bytes_to_send;
    unsigned sequenceNumber = 0;

    while ((bytes_to_send = fread(buf, sizeof(unsigned char), MAX_SIZE - 4, filePointer)) > 0) {
        printf("MIDLE PACKET\n");
        unsigned char dataPacket[MAX_SIZE];
        dataPacket[0] = MIDDLE_PACKET;
        dataPacket[1] = sequenceNumber % 255;
        dataPacket[2] = (bytes_to_send / 256);
        dataPacket[3] = (bytes_to_send % 256);
        memcpy(&dataPacket[4], buf, bytes_to_send);

        llwrite(dataPacket, ((bytes_to_send + 4) < MAX_SIZE) ? (bytes_to_send + 4) : MAX_SIZE);
        printf("> Another packet send\n");
        sequenceNumber++;
    }


    // printf("Middle packets sent\n");

    fileSizeLength = sizeof(file_stat.st_size);
    filenameLength = strlen(filename);
    packetSize = 5 + fileSizeLength + filenameLength;

    packet[0] = ENDING_PACKET;
    packet[1] = FILE_SIZE;
    packet[2] = fileSizeLength;
    memcpy(&packet[3], &file_stat.st_size, fileSizeLength);
    packet[3 + fileSizeLength] = FILE_NAME;
    packet[4 + fileSizeLength] = filenameLength;
    memcpy(&packet[5 + fileSizeLength], filename, filenameLength);

    if (llwrite(packet, packetSize) < 0)
        return -1;

    // printf("Ending packet sent\n");

    fclose(filePointer);

    // end_t = clock();
    // total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
    // printf("\nTotal time taken: %f seconds\n", total_t);
    // printf("Size transferred: %d bytes\n", (int)file_stat.st_size);
    // printf("Transfer Speed: %f B/s\n\n", file_stat.st_size / total_t);

    return 0;
}


int receiveData(const char *filename) {
    int size;
    static FILE *dest;
    int STOP = FALSE;

    while (!STOP) {
        printf("\nRECEIVER1\n");
        unsigned char buf[MAX_SIZE];
        printf("\nRECEIVER2\n");

        if ((size = llread(buf)) < 0){
            printf("\nRECEIVER3\n");
            continue;
        }

        // printf("PACKET : %d\n", buf[0]);
        switch (buf[0]) {
        case STARTING_PACKET:
            dest = fopen(filename, "wb");
            break;

        case MIDDLE_PACKET: ;
            static unsigned int n = 0;
            if (buf[1] != n)
                return -1;
            n = (n + 1) % 255;

            unsigned int data_size = buf[2] * 256 + buf[3];
            fwrite(&buf[4], sizeof(unsigned char), data_size * sizeof(unsigned char), dest);

            break;

        case ENDING_PACKET:
            // printf("\nENDING PACKET\n");
            fclose(dest);
            STOP = TRUE;
            break;
        }
    }
    return 0;   
}   

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayer link_layer;

    link_layer.baudRate = baudRate;
    link_layer.nRetransmissions = nTries;
    link_layer.timeout = timeout;

    if (strcmp(role, "tx") == 0)
        link_layer.role = LlTx;
    else if (strcmp(role, "rx") == 0)
        link_layer.role = LlRx;
    else {
        fprintf(stderr, "Unknown role: %s\n", role);
        return;
    }

    sprintf(link_layer.serialPort, "%s", serialPort);

    if (llopen(link_layer) < 0) {
        exit(-1);
        return;
    }

    if (link_layer.role == LlTx) {
        transmitData(filename);
    } 
    else if (link_layer.role == LlRx) {
        receiveData(filename);
    }
    else {
        fprintf(stderr, "Unknown role: %s\n", role);
        return;
    }

    llclose(FALSE);
}


