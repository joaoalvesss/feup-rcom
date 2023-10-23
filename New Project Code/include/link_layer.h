#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>

typedef enum{ LlTx, LlRx } LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

#define _POSIX_SOURCE       1
#define MAX_SIZE            256

#define FALSE               0
#define TRUE                1

#define FLAG                0x7E
#define A_TRANSMITER        0x03
#define A_RECEIVER          0x01
#define C_SET               0x03
#define C_UA                0x07
#define BCC(a, c)           (a^c) 
#define DISC                0x0B
#define ESCAPE              0x7D
#define RR(n)               (0x05 | (n << 7))
#define REJ(n)              (0x01 | (n << 7))
#define C_INF(n)            (0x00 | (n << 6))
#define RR_R                0

#define START               1
#define FLAG_RCV            2
#define A_RCV               3
#define C_RCV               4
#define BCC_OK              5
#define WAITING             6

#define RR0                 0
#define RR1                 1
#define REJ0                2
#define REJ1                3
#define OTHER               4

#define MIDDLE_PACKET       1
#define STARTING_PACKET     2
#define ENDING_PACKET       3

int state_machine(unsigned char a, unsigned char c, int isData, int RR_REJ);

int send_buffer(unsigned char a, unsigned char c);

int llopen(LinkLayer connectionParameters);

int llwrite(const unsigned char *buf, int bufSize);

int llread(unsigned char * buffer);

int llclose(int showStatistics);

#endif // _LINK_LAYER_H_
