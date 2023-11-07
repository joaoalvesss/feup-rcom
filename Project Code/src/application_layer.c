// Application layer protocol implementation

#include "application_layer.h"
#include <signal.h>
#include <stdlib.h>

struct stat file_stat;
clock_t start_t, end_t;
double total_t;

int transmitData(const char *filename) {
    start_t = clock();

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
    packet[1] = 0;
    packet[2] = fileSizeLength;
    memcpy(&packet[3], &file_stat.st_size, fileSizeLength);
    packet[3 + fileSizeLength] = 1;
    packet[4 + fileSizeLength] = filenameLength;
    memcpy(&packet[5 + fileSizeLength], filename, filenameLength);

    printf("> Sending start packet\n");
    if (llwrite(packet, packetSize) < 0)
        return -1;

    unsigned char buf[MAX_SIZE];
    unsigned bytes_to_send;
    unsigned sequenceNumber = 0;

    while ((bytes_to_send = fread(buf, sizeof(unsigned char), MAX_SIZE - 4, filePointer)) > 0) {
        printf("> Sending middle packet\n");
        unsigned char dataPacket[MAX_SIZE];
        dataPacket[0] = MIDDLE_PACKET;
        dataPacket[1] = sequenceNumber % (MAX_SIZE -1);
        dataPacket[2] = (bytes_to_send / MAX_SIZE);
        dataPacket[3] = (bytes_to_send % MAX_SIZE);
        memcpy(&dataPacket[4], buf, bytes_to_send);

        if (llwrite(dataPacket, ((bytes_to_send + 4) < MAX_SIZE) ? (bytes_to_send + 4) : MAX_SIZE) == -1) {
            break;
        }
        printf("> Another packet sent\n");
        sequenceNumber++;
    }

    fileSizeLength = sizeof(file_stat.st_size);
    filenameLength = strlen(filename);
    packetSize = 5 + fileSizeLength + filenameLength;

    packet[0] = ENDING_PACKET;
    packet[1] = 0;
    packet[2] = fileSizeLength;
    memcpy(&packet[3], &file_stat.st_size, fileSizeLength);
    packet[3 + fileSizeLength] = 1;
    packet[4 + fileSizeLength] = filenameLength;
    memcpy(&packet[5 + fileSizeLength], filename, filenameLength);

    printf("> Sending end packet\n");
    if (llwrite(packet, packetSize) < 0)
        return -1;

    // printf("Ending packet sent\n");

    fclose(filePointer);

    end_t = clock();
    total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
    printf("\nTotal time taken: %f seconds\n", total_t);
    printf("Size transferred: %d bytes\n", (int)file_stat.st_size);
    printf("Transfer Speed: %f B/s\n\n", file_stat.st_size / total_t);

    return 0;
}


int receiveData(const char *filename) {
    int size;
    static FILE *dest;
    int STOP = FALSE;

    while (!STOP) {
        unsigned char buf[MAX_SIZE];

        if ((size = llread(buf)) < 0){
            continue;
        }

        // printf("PACKET : %d\n", buf[0]);
        if(buf[0] == 2) {
            printf("\n> Receiving start packet\n");
            dest = fopen(filename, "wb");
        }

        else if(buf[0] == 1) {
            printf("\n> Receiving middle packet\n");
            static unsigned int buf_position = 0;
            if (buf[1] != buf_position){ return -1; }
                
            buf_position = (buf_position + 1) % 255;

            unsigned int data_size = buf[2] * 256 + buf[3];
            fwrite(&buf[4], sizeof(unsigned char), data_size * sizeof(unsigned char), dest);

        }

        else if (buf[0] == 3) {
            printf("\n> Receiving end packet\n");
            fclose(dest);
            STOP = TRUE;
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

