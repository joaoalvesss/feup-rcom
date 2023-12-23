#include "link_layer.h"

#define _POSIX_SOURCE 1

struct termios oldtio;
struct termios newtio;
int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
int current_state = START;
int fd, bytes = 0;
unsigned char readbyte;
int answer = -1;
LinkLayer link_layer;


int state_machine(unsigned char a, unsigned char c, int isData, int RR_REJ) {
    if (!RR_REJ){ answer = -1; }
        
    unsigned char byte = 0;
    int bytes = 0;
    bytes = read(fd, &byte, 1);

    if (bytes > 0) {
        if (current_state == START) {
            // printf("START\n");
            if (byte == FLAG)
                current_state = FLAG_RCV;
        } 

        else if (current_state == FLAG_RCV) {
            // printf("FLAG_RCV\n");
            if (byte == a)
                current_state = A_RCV;
            else if (byte != FLAG)
                current_state = START;

        } else if (current_state == A_RCV) {
            // printf("A_RCV\n");
            if (RR_REJ) {
                if (byte == (RR(0))) {
                    answer = RR0;
                    current_state = C_RCV;
                } else if (byte == (RR(1))) {
                    answer = RR1;
                    current_state = C_RCV;
                } else if (byte == (REJ(0))) {
                    answer = REJ0;
                    current_state = C_RCV;
                } else if (byte == (REJ(1))) {
                    answer = REJ1;
                    current_state = C_RCV;
                } else {
                    current_state = START;
                }
            } 
            else {
                if (byte == c)
                    current_state = C_RCV;
                else if (byte == FLAG)
                    current_state = FLAG_RCV;
                else
                    current_state = START;
            }
        } 

        else if (current_state == C_RCV) {
            // printf("C_RCV\n");
            if (answer == RR0)
                c = RR(0);
            else if (answer == RR1)
                c = RR(1);
            else if (answer == REJ0)
                c = REJ(0);
            else if (answer == REJ1)
                c = REJ(1);

            if (byte == (a ^ c)) {
                if (isData)
                    current_state = WAITING;
                else
                    current_state = BCC_OK;
            } 
            else if (byte == FLAG)
                current_state = FLAG_RCV;
            else
                current_state = START;
        } 

        else if (current_state == WAITING) {
            // printf("WAITING\n");
            if (byte == FLAG) {
                STOP = TRUE;
            }
        } 
        
        else if (current_state == BCC_OK) {
            // printf("BCC_OK\n");
            if (byte == FLAG) {
                // printf("STOP\n");
                STOP = TRUE;
                current_state = START;
                if (c == C_UA)
                    alarm(0);
            } 
            else {
                current_state = START;
            }
        }
        // printf("%x\n", byte);
        readbyte = byte;
        return TRUE;
    }

    return FALSE;
}


int send_buffer(unsigned char a, unsigned char c) {
    unsigned char buffer[5];
    buffer[0] = FLAG;
    buffer[1] = a;
    buffer[2] = c;
    buffer[3] = buffer[1] ^ buffer[2];
    buffer[4] = FLAG;
    return write(fd, buffer, sizeof(buffer));
}

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    link_layer = connectionParameters;
    (void)signal(SIGALRM, alarmHandler);
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0){
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    if (tcgetattr(fd, &oldtio) == -1){
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    // printf("\nhere\n");
    // unsigned char byte2 = 0;
    // read(fd, &byte2, 1);
    // printf("\nhere\n");
    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1){
        perror("tcsetattr");
        exit(-1);
    }

    sleep(1);
    STOP = FALSE;

    if (link_layer.role == LlTx) {
        while (STOP == FALSE && alarmCount < link_layer.nRetransmissions) {
            if (alarmEnabled == FALSE) {
                bytes = send_buffer(A_TRANSMITER, C_SET);
                alarm(link_layer.timeout);
                alarmEnabled = TRUE;
                current_state = START;
            }
            state_machine(A_TRANSMITER, C_UA, 0, 0);
        }

        alarm(0);

        if (alarmCount >= link_layer.nRetransmissions) {
            printf("ERROR: TIMEOUT\n");
            return -1;
        }
        printf("Connection established successfully\n");
    } 
    else if (connectionParameters.role == LlRx) {
        while (STOP == FALSE) {
            state_machine(A_TRANSMITER, C_SET, 0, 0);
        }
        bytes = send_buffer(A_TRANSMITER, C_UA);
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int stuff(unsigned char *stuffed, const unsigned char *trama, int len) {
    int size = 0;
    stuffed[size++] = trama[0];

    for (int i = 1; i < len; i++) {
        if (trama[i] == FLAG || trama[i] == ESCAPE) {
            stuffed[size++] = ESCAPE;
            stuffed[size++] = trama[i]^0x20;
        }
        else
            stuffed[size++] = trama[i];
    }

    return size;
}

int destuff(unsigned char *destuffed, const unsigned char *trama, int len) {
    int size = 0;
    destuffed[size++] = trama[0];

    for (int i = 1; i < len; i++) {
        if (trama[i] == ESCAPE) {
            destuffed[size++] = trama[i+1]^0x20;
            i++;
        }
        else
            destuffed[size++] = trama[i];
    }
    return size;
}

int llwrite(const unsigned char *buf, int bufSize) {
    int size = bufSize + 5;
    unsigned char trama[size];
    static int pacote = 0;
    int tries = 1;
    int rej = FALSE;

    // apply header
    trama[0] = FLAG;
    trama[1] = A_TRANSMITER;
    trama[2] = C_INF(pacote);
    trama[3] = BCC(A_TRANSMITER, C_INF(pacote));

    // content
    unsigned char bcc2 = buf[0];
    for (int i = 0; i < bufSize; i++) {
        trama[i + 4] = buf[i];
        if (i > 0){ bcc2 ^= buf[i]; }
    }

    // apply tail
    trama[bufSize + 4] = bcc2;
    unsigned char stuffed[size*2];
    size = stuff(stuffed, trama, size);
    stuffed[size] = FLAG;
    size++;

    STOP = FALSE;
    alarmEnabled = FALSE;
    alarmCount = 0;
    current_state = START;

    while (!STOP && alarmCount < link_layer.nRetransmissions) {
        if (!alarmEnabled) {  
            bytes = write(fd, stuffed, size);
            printf("> %d Written bytes at %d try\n", bytes, tries);

            alarm(link_layer.timeout);
            alarmEnabled = TRUE;
            current_state = START;
            tries++;
        }

        state_machine(A_TRANSMITER, 0, 0, 1);

        if ((pacote == 0 && answer == REJ1) || (pacote == 1 && answer == REJ0)) { rej = TRUE; }
        if (rej == TRUE) { alarm(0); alarmEnabled = FALSE; }
    }

    pacote = (pacote + 1) % 2;
    alarm(0);
    printf("> Packet arrived nicely\n\n");
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *buffer) {
    int readed = 0, size;
    static int packet = 0;
    STOP = FALSE;
    current_state = START;

    unsigned char stuffed[(MAX_SIZE * 2) + 7];
    unsigned char destuffed[MAX_SIZE + 7];

    while (STOP == FALSE){
        if (state_machine(A_TRANSMITER, C_INF(packet), 1, 0)) {
            stuffed[readed] = readbyte;
            readed++;
        }
    }

    size = destuff(destuffed, stuffed, readed);
    unsigned char original_bcc2 = destuffed[size - 2];
    unsigned char cmp_bcc2 = 0x00;

    for (unsigned int i = 4; i < (size-2); i++){ cmp_bcc2 ^= destuffed[i]; }

    if (destuffed[2] == C_INF(packet) && original_bcc2 == cmp_bcc2) {
        packet = (packet + 1) % 2;
        send_buffer(A_TRANSMITER, RR(packet));
        memcpy(buffer, &destuffed[4], size - 5);
        printf("> Correct packet: \n > bcc2 -> %x \n > message -> %x\n", cmp_bcc2, destuffed[2]);
        size -= 5;
        return size;
    }
    else if (cmp_bcc2 == original_bcc2) {
        send_buffer(A_TRANSMITER, RR(packet));
        printf("> Received a duplicated packet, rejection ongoing\n");
    }
    else {
        send_buffer(A_TRANSMITER, REJ(packet));
        current_state = START;
        printf("> Bcc2 didnt match, rejection ongoing\n");
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    STOP = FALSE;
    alarmEnabled = FALSE;
    alarmCount = 0;
    current_state = START;
    answer = OTHER;

    if (link_layer.role == LlTx) {
        while (STOP == FALSE && alarmCount < link_layer.nRetransmissions) {
            if (alarmEnabled == FALSE) {
                bytes = send_buffer(A_TRANSMITER, DISC);
                alarm(link_layer.timeout);
                alarmEnabled = TRUE;
            }
            state_machine(A_RECEIVER, DISC, 0, 0);
        }
        if (alarmCount == link_layer.nRetransmissions) {
            return -1;
        }
        bytes = send_buffer(A_RECEIVER, C_UA);
    } else if (link_layer.role == LlRx) {
        while (STOP == FALSE) {
            state_machine(A_TRANSMITER, DISC, 0, 0);
        }
        bytes = send_buffer(A_RECEIVER, DISC);
        STOP = FALSE;
        current_state = START;
        while (STOP == FALSE) {
            state_machine(A_RECEIVER, C_UA, 0, 0);
        }
    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("> Everything went well and the file was transfered!\n");
    close(fd);

    return 0;
}
