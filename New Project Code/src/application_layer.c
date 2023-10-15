// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

int transmitting(const char* file){
    return 0;
}

int receiving(const char* file){
    static FILE *dest;
    while(true)
    {
        unsigned char buf[MAX_PACKET_SIZE];
        if ((llread(buf)) < 0) { continue; } 
        if (buf[0] == 1) { dest = fopen(filename, "wb"); }
        else if (buf[0] == 2) {
            if (buf[1] != 0) { return -1; }
            unsigned int data_size = buf[2] * 256 + buf[3];
            fwrite(&buf[4], sizeof(unsigned char), data_size * sizeof(unsigned char), destination);
        }
        else if (buf[0] == 3) {
            close(destination);
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
    if (llopen(layer) < 0){
        printf("Error in llopen()");
        exit(-1);
        return;
    } 

    if (strcmp(role, "tx")) 
        if (transmitting(filename)) {
            printf("Error in transmitting()");
            exit(-1);
            return;
        }
    else if (strcmp(role, "rx")) 
        if (receiving(filename)) {
            printf("Error in receiving()");
            exit(-1);
            return;
        }
    if (llclose(0)) {
        printf("Error in llclose()");
        exit(-1);
        return;
    }
}
