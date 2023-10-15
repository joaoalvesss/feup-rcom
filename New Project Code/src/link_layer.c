// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

struct termios oldtio;
struct termios newtio;
int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
int current_state = START;
LinkLayer link_layer;
int fd, bytes;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int stateMachine(unsigned char a, unsigned char c, int isData, int RR_REJ) {
    if (!RR_REJ){ response = -1; }
        
    unsigned char byte = 0;
    bytes = read(fd, &byte, 1);

    if (bytes > 0) {
        // State: START
        printf("> Reached START\n");
        if (state == START) {
            if (byte == FLAG)
                state = FLAG_RCV;
        }

        // State: FLAG_RCV
        if (state == FLAG_RCV) {
            printf("> Reached FLAG_RCV\n");
            if (byte == a)
                state = A_RCV;
            else if (byte != FLAG)
                state = START;
        }

        // State: A_RCV
        if (state == A_RCV) {
            printf("> Reached A_RCV\n");
            if (RR_REJ) {
                switch (byte) {
                    case RR(0):
                        response = RR0;
                        state = C_RCV;
                        break;
                    case RR(1):
                        response = RR1;
                        state = C_RCV;
                        break;
                    case REJ(0):
                        response = REJ0;
                        state = C_RCV;
                        break;
                    case REJ(1):
                        response = REJ1;
                        state = C_RCV;
                        break;
                    default:
                        state = START;
                        break;
                }
            } else {
                if (byte == c)
                    state = C_RCV;
                else if (byte == FLAG)
                    state = FLAG_RCV;
                else
                    state = START;
            }
        }

        // State: C_RCV
        if (state == C_RCV) {
            printf("> Reached C_RCV\n");
            switch (response) {
                case RR0:
                    c = RR(0);
                    break;
                case RR1:
                    c = RR(1);
                    break;
                case REJ0:
                    c = REJ(0);
                    break;
                case REJ1:
                    c = REJ(1);
                    break;
                default:
                    break;
            }
            printf("%d\n", response);
            if (byte == (a ^ c)) {
                if (isData)
                    state = WAITING_DATA;
                else
                    state = BCC_OK;
            } else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
        }

        // State: WAITING_DATA
        if (state == WAITING_DATA) {
            printf("> Waiting for mode data\n");
            if (byte == FLAG) {
                STOP = TRUE;
                // alarm(0);
            }
        }

        // State: BCC_OK
        if (state == BCC_OK) {
            printf("> Reached BCC_OK\n");
            if (byte == FLAG) {
                printf("> DONE\n\n");
                STOP = TRUE;
                state = START;
                if (c == C_UA)
                    alarm(0);
            } else
                state = START;
        }

        // printf("%x\n", byte);
        readbyte = byte;
        return TRUE;
    }

    return FALSE;
}


int sendBuffer(unsigned char a, unsigned char c){
    unsigned char buf[5];

    buf[0] = FLAG;
    buf[1] = a;
    buf[2] = c;
    buf[3] = a ^ c;
    buf[4] = FLAG;

    return write(fd, buf, sizeof(buf));
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////

int stuffing(const unsigned char *msg, int newSize, unsigned char *stuffedMsg){
    return 0;
}

int destuffing(const unsigned char *msg, int newSize, unsigned char *destuffedMsg){
    return 0;
}

unsigned char calculateBCC2(const unsigned char *buf, int dataSize, int startingByte){
    return 0;
}

int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    STOP = FALSE;
    alarmEnabled = FALSE;
    alarmCount = 0;

    LinkLayerRole role = link_layer.role;
    int maxTries = link_layer.nRetransmissions;
    int timeout = link_layer.timeout;

    if (role == LlTx) {
        while (!STOP && alarmCount < maxTries) {
            if (!alarmEnabled) {
                bytes = sendBuffer(A_T, DISC);
                printf("Number of written bytes: %d \n\n", bytes);
                printf("DISC was sent in order to terminate\n\n");
                alarm(timeout);
                alarmEnabled = TRUE;
            }
            stateMachine(A_R, DISC, 0, 0);
        }

        if (alarmCount == maxTries) {
            return -1;
        }

        printf("DISC was received\n\n");
        bytes = sendBuffer(A_R, C_UA);
        printf("Sending UA now\n\n");
        printf("Number of written bytes: %d \n\n", bytes);
    } 
    else if (role == LlRx) {
        while (!STOP) {
            stateMachine(A_R, DISC, 0, 0);
        }

        printf("DISC was received\n\n");
        sendBuffer(A_R, C_UA);
        printf("Sending UA now\n\n");

        current_state = START;
        STOP = FALSE;

        while (!STOP) {
            stateMachine(A_R, C_UA, 0, 0);
        }

        printf("UA was received!\n\n");
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 0;
}

