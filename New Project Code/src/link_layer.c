#include "link_layer.h"

#define _POSIX_SOURCE 1

struct termios oldtio;
struct termios newtio;
int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
int state = START;
int fd, bytes = 0;
unsigned char readbyte;
int response = -1;
LinkLayer link_layer;


int stateMachine(unsigned char a, unsigned char c, int isData, int RR_REJ)
{
    if (!RR_REJ)
        response = -1;
    unsigned char byte = 0;

    int bytes = 0;

    bytes = read(fd, &byte, 1);

    if (bytes > 0)
    {
        switch (state)
        {
        case START:
            printf("START\n");
            if (byte == FLAG)
                state = FLAG_RCV;
            break;
        case FLAG_RCV:
            printf("FLAG_RCV\n");
            if (byte == a)
                state = A_RCV;
            else if (byte != FLAG)
                state = START;
            break;
        case A_RCV:
            printf("A_RCV\n");
            if (RR_REJ)
            {
                switch (byte)
                {
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
            }
            else
            {
                if (byte == c)
                    state = C_RCV;
                else if (byte == FLAG)
                    state = FLAG_RCV;
                else
                    state = START;
            }
            break;
        case C_RCV:
            printf("C_RCV\n");
            switch (response)
            {
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
            if (byte == (a ^ c))
            {
                if (isData)
                    state = WAITING_DATA;
                else
                    state = BCC_OK;
            }
            else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
            break;
        case WAITING_DATA:
            printf("WAITING_DATA\n");
            if (byte == FLAG)
            {
                STOP = TRUE;
            }
            break;
        case BCC_OK:
            printf("BCC_OK\n");
            if (byte == FLAG)
            {
                printf("STOP\n");
                STOP = TRUE;
                state = START;
                if (c == C_UA)
                    alarm(0);
            }
            else
                state = START;
            break;
        }
        printf("%x\n", byte);
        readbyte = byte;
        return TRUE;
    }

    return FALSE;
}

int sendBuffer(unsigned char a, unsigned char c) {
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
    newtio.c_cc[VTIME] = connectionParameters.timeout * 10;
    newtio.c_cc[VMIN] = 0;

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
                bytes = sendBuffer(A_T, C_SET);
                alarm(link_layer.timeout);
                alarmEnabled = TRUE;
                state = START;
            }
            stateMachine(A_T, C_UA, 0, 0);
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
            stateMachine(A_T, C_SET, 0, 0);
        }
        bytes = sendBuffer(A_T, C_UA);
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int stuffing(const unsigned char *msg, int newSize, unsigned char *stuffedMsg) {
    int size = 0;
    stuffedMsg[size++] = msg[0];

    for (int i = 1; i < newSize; i++) {
        if (msg[i] == FLAG || msg[i] == ESCAPE) {
            stuffedMsg[size++] = ESCAPE;
            stuffedMsg[size++] = msg[i] ^ 0x20;
        }
        else
            stuffedMsg[size++] = msg[i];
    }

    return size;
}

int destuffing(const unsigned char *msg, int newSize, unsigned char *destuffedMsg) {
    int size = 0;
    destuffedMsg[size++] = msg[0];

    for (int i = 1; i < newSize; i++) {
        if (msg[i] == ESCAPE) {
            destuffedMsg[size++] = msg[i + 1] ^ 0x20;
            i++;
        }
        else
            destuffedMsg[size++] = msg[i];
    }
    return size;
}

unsigned char calculateBCC2(const unsigned char *buf, int dataSize, int startingByte) {
    if (dataSize < 0) {
        printf("Error buf Size: %d\n", dataSize);
    }
    unsigned char BCC2 = 0x00;
    for (unsigned int i = startingByte; i < dataSize; i++)
        BCC2 ^= buf[i];

    return BCC2;
}

int llwrite(const unsigned char *buf, int bufSize) {
    int newSize = bufSize + 5;
    unsigned char msg[newSize];

    static int packet = 0;

    msg[0] = FLAG;
    msg[1] = A_T;
    msg[2] = C_INF(packet);
    msg[3] = BCC(A_T, C_INF(packet));

    unsigned char BCC2 = buf[0];
    for (int i = 0; i < bufSize; i++)
    {
        msg[i + 4] = buf[i];
        if (i > 0)
            BCC2 ^= buf[i];
    }

    msg[bufSize + 4] = BCC2;

    unsigned char stuffed[newSize * 2];
    newSize = stuffing(msg, newSize, stuffed);
    stuffed[newSize] = FLAG;
    newSize++;

    STOP = FALSE;
    alarmEnabled = FALSE;
    alarmCount = 0;
    state = START;

    int numtries = 0;
    
    int reject = FALSE;

    while (STOP == FALSE && alarmCount < link_layer.nRetransmissions) {
        if (alarmEnabled == FALSE) {
            numtries++;
            bytes = write(fd, stuffed, newSize);
            printf("Data Enviada. %d bytes written, %dº try...\n", bytes, numtries);
            alarm(link_layer.timeout);
            alarmEnabled = TRUE;
            state = START;
        }

        stateMachine(A_T, 0, 0, 1);

        if ((packet == 0 && response == REJ1) || (packet == 1 && response == REJ0)) {
            reject = TRUE;
        }

        if (reject == TRUE) {
            alarm(0);
            alarmEnabled = FALSE;
        }
    }
    packet = (packet + 1) % 2;
    alarm(0);
    printf("Data Accepted!\n");

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *buffer) {
    int bytesread = 0;

    static int packet = 0;

    unsigned char stuffedMsg[MAX_BUFFER_SIZE];
    unsigned char unstuffedMsg[MAX_PACKET_SIZE + 7];

    STOP = FALSE;
    state = START;

    while (STOP == FALSE){
        if (stateMachine(A_T, C_INF(packet), 1, 0)) {
            stuffedMsg[bytesread] = readbyte;
            bytesread++;
        }
    }

    printf("DATA RECEIVED\n");

    int s = destuffing(stuffedMsg, bytesread, unstuffedMsg);

    unsigned char receivedBCC2 = unstuffedMsg[s - 2];
    printf("RECEIVED BCC2: %x\n", receivedBCC2);
    unsigned char expectedBCC2 = calculateBCC2(unstuffedMsg, s - 2, 4);
    printf("EXPECTED BCC2: %x\n", expectedBCC2);

    if (receivedBCC2 == expectedBCC2 && unstuffedMsg[2] == C_INF(packet)) {
        packet = (packet + 1) % 2;
        sendBuffer(A_T, RR(packet));
        memcpy(buffer, &unstuffedMsg[4], s - 5);
        printf("STATUS 1 ALL OK: %x , %x\nSENDING RESPONSE\n", receivedBCC2, unstuffedMsg[2]);
        return s - 5;
    }
    else if (receivedBCC2 == expectedBCC2) {
        sendBuffer(A_T, RR(packet));
        tcflush(fd, TCIFLUSH);
        printf("Duplicate packet!\n");
    }
    else {
        sendBuffer(A_T, REJ(packet));
        tcflush(fd, TCIFLUSH);
        printf("Error in BCC2, sent REJ\n");
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
    state = START;
    response = OTHER;

    switch (link_layer.role) {
    case LlTx:
        while (STOP == FALSE && alarmCount < link_layer.nRetransmissions) {
            if (alarmEnabled == FALSE) {
                bytes = sendBuffer(A_T, DISC);
                alarm(link_layer.timeout);
                alarmEnabled = TRUE;
            }
            stateMachine(A_R, DISC, 0, 0);
        }
        if (alarmCount == link_layer.nRetransmissions)
            return -1;
        bytes = sendBuffer(A_R, C_UA);
        break;

    case LlRx:
        while (STOP == FALSE) {
            stateMachine(A_T, DISC, 0, 0);
        }
        bytes = sendBuffer(A_R, DISC);
        STOP = FALSE;
        state = START;
        while (STOP == FALSE) {
            stateMachine(A_R, C_UA, 0, 0);
        }
        break;
    default:
        break;
    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
