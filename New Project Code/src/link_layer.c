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
int fd, bytes, answer = -1;
unsigned char readbyte;


void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    // printf("Alarm #%d\n", alarmCount);
}

int stateMachine(unsigned char a, unsigned char c, int isData, int RR_REJ) {
    if (!RR_REJ){ answer = -1; }
        
    unsigned char byte = 0;
    bytes = read(fd, &byte, 1);

    if (bytes > 0) {
        // State: START
        printf("> Reached START\n");
        if (current_state == START) {
            if (byte == FLAG)
                current_state  = FLAG_RCV;
        }

        // State: FLAG_RCV
        if (current_state == FLAG_RCV) {
            printf("> Reached FLAG_RCV\n");
            if (byte == a)
                current_state = A_RCV;
            else if (byte != FLAG)
                current_state = START;
        }

        // State: A_RCV
        if (current_state == A_RCV) {
            printf("> Reached A_RCV\n");
            if (RR_REJ) {
                switch (byte) {
                    case RR(0):
                        answer = RR0;
                        current_state = C_RCV;
                        break;
                    case RR(1):
                        answer = RR1;
                        current_state = C_RCV;
                        break;
                    case REJ(0):
                        answer = REJ0;
                        current_state  = C_RCV;
                        break;
                    case REJ(1):
                        answer = REJ1;
                        current_state = C_RCV;
                        break;
                    default:
                        current_state = START;
                        break;
                }
            } else {
                if (byte == c)
                    current_state = C_RCV;
                else if (byte == FLAG)
                    current_state = FLAG_RCV;
                else
                    current_state = START;
            }
        }

        // State: C_RCV
        if (current_state  == C_RCV) {
            printf("> Reached C_RCV\n");
            switch (answer) {
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
            printf("%d\n", answer);
            if (byte == (a ^ c)) {
                if (isData)
                    current_state  = WAITING_DATA;
                else
                    current_state  = BCC_OK;
            } else if (byte == FLAG)
                current_state  = FLAG_RCV;
            else
                current_state  = START;
        }

        // State: WAITING_DATA
        if (current_state == WAITING_DATA) {
            printf("> Waiting for mode data\n");
            if (byte == FLAG) {
                STOP = TRUE;
            }
        }

        // State: BCC_OK
        if (current_state == BCC_OK) {
            printf("> Reached BCC_OK\n");
            if (byte == FLAG) {
                printf("> DONE\n\n");
                STOP = TRUE;
                current_state = START;
                if (c == C_UA)
                    alarm(0);
            } else
                current_state = START;
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
    printf("> Started openning connection\n\n");
    (void)signal(SIGALRM, alarmHandler);

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);


    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    bytes = 0;
    LinkLayerRole role = connectionParameters.role;
    int retransmissions = connectionParameters.nRetransmissions;
    int timeout = connectionParameters.timeout;

    sleep(1);

    STOP = FALSE;

    if (role == LlTx) {
        while (!STOP && alarmCount < retransmissions) {
            if (alarmEnabled == FALSE) {
                bytes = sendBuffer(A_TRANSMITER, C_SET);
                printf("> %d bytes written\n", bytes);
                alarm(timeout);
                alarmEnabled = TRUE;
                current_state = START;
            }
            stateMachine(A_TRANSMITER, C_UA, 0, 0);
        }
        alarm(0);
        if (alarmCount >= retransmissions) {
            printf("> Maximum number of tries reached, error\n\n");
            return -1;
        }
        printf("> llopen worked fine\n\n");
    } 
    else if (role == LlRx) {
        while (!STOP) {
            stateMachine(A_TRANSMITER, C_SET, 0, 0);
        }
        bytes = sendBuffer(A_TRANSMITER, C_UA);
        printf("> Answer to llopen; %d bytes written\n\n", bytes);
    } 
    else {
        printf("> Unknown error in role, llopen\n\n");
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////

int stuffing(const unsigned char *trama, unsigned char *trama_stuffed, int size)
{
    printf("> Entered stuffing mechanism\n\n");

    int aux = 0;
    trama_stuffed[aux++] = trama[0];

    for(int i = 1; i < size; i++){
        if((trama[i] == FLAG) || (trama[i] == ESCAPE)){
            trama_stuffed[aux++] = ESCAPE;
            trama_stuffed[aux++] = trama[i] ^ 0x20;
            // printf("%x\n", trama_stuffed[aux-1]);
        }
        else{
            trama_stuffed[aux++] = trama[i];
            // printf("%x\n", trama_stuffed[aux-1]);
        }
    }

    printf("> Exiting stuffing mechanism\n\n");
    return aux; // return do novo tamanho da trama após ser stuffed ou não
}

int destuffing(const unsigned char *trama, unsigned char *trama_destuffed, unsigned char *size)
{
    printf("> Entered destuffing mechanism\n\n");

    int aux = 0;
    trama_destuffed[aux++] = trama[0];

    int cmp = (int) *size;
    for(int i = 1; i < cmp; i++){
        if(trama[i] == ESCAPE){
            trama_destuffed[aux++] = trama[i+1] ^ 0x20;
            // printf("%x\n", trama_stuffed[aux-1]);
        }
        else{
            trama_destuffed[aux++] = trama[i];
            // printf("%x\n", trama_stuffed[aux-1]);
        }
    }

    printf("> Exiting destuffing mechanism\n\n");
    return aux; // return do novo tamanho da trama após ser destuffed ou não (tamanho original da trama)
}

int llwrite(const unsigned char *buf, int bufSize)
{
    int last_rejected = FALSE;
    int maxTries = link_layer.nRetransmissions;
    int timeout = link_layer.timeout;
    unsigned char trama_size = bufSize + 5; // flag, A, C, bcc1 e bcc2
    static int packet = 0;
    unsigned char trama[trama_size];

    // Construir a trama
    trama[2] = C_INFO(packet); // C
    trama[1] = A_TRANSMITER; // A
    trama[0] = FLAG; // FLAG
    trama[3] = BCC(trama[1], trama[2]); // BCC1

    // BCC2 
    unsigned char bcc2 = buf[0];
    for(int i = 0; i < bufSize; i++){
        trama[i + 4] = buf[i]; // copiar do buffer para a trama que vamos enviar
        if(i != 0){
            bcc2 = bcc2 ^ buf[i];
        }
    }
    trama[bufSize + 4] = bcc2;

    // Mecanismo de stuffing 
    unsigned char trama_stuffed[trama_size * 2]; // Dobro do tamanho para garantir que cabe 
    trama_size = stuffing(trama, trama_size, &trama_stuffed);
    trama_stuffed[trama_size] = FLAG;
    trama_size++;

    // Desligar o alarme e dar reset
    STOP = FALSE;
    current_state = START;
    alarmEnabled = FALSE;
    alarmCount = 0;

    while(STOP == FALSE && alarmCount < maxTries){
        if(alarmEnabled == FALSE){
            // Fazer write e ativar o alarme
            bytes = write(fd, trama_stuffed, trama_size);
            printf("> %d bytes written\n", bytes);
            alarm(timeout);
            alarmEnabled = TRUE;
        }

        // Alterar estado state machine
        stateMachine(A_TRANSMITER, NULL, 0, 1);

        // Verificar resposta da state machine
        if ((packet == 0 && answer == REJ1) || (packet == 1 && answer == REJ0)){
            last_rejected = TRUE;
        }

        if(last_rejected == TRUE){
            alarmEnabled = FALSE;
            alarm(0);
        }
    }

    packet = (packet + 1) % 2;
    printf("> Writting went well\n\n");
    alarm(0);
    alarmEnabled = FALSE;
    
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *buffer)
{
    int bytesread = 0;
    static int packet = 0;
    unsigned char trama_stuffed[MAX_SIZE * 2 + 7];
    unsigned char trama_unstuffed[MAX_SIZE + 7];
    STOP = FALSE;
    current_state = START;

    while (STOP == FALSE){
        if (stateMachine(A_TRANSMITER, C_INFO(packet), 1, 0)){
            trama_stuffed[bytesread] = readbyte;
            bytesread++;
        }
    }

    int trama_size = destuffing(trama_stuffed, bytesread, trama_unstuffed);

    unsigned char received = trama_unstuffed[trama_size - 2]; // BCC2 recebido

    // Calcular BCC2 esperado
    if (trama_size < 0){ printf("> Error in trama size: %d\n", trama_size); }

    unsigned char expected = 0x00;
    for (unsigned int i = 4; i < trama_size - 2; i++) { // Começa no 4 para avançar flag, a, c e bcc1, acaba -2 para ignorar o proprio bcc2 e flag
        expected ^= buffer[i];
    }

    printf("> Received BCC2: %x\n", received);
    printf("> Expected BCC2: %x\n", expected);

    if ((received == expected) && (trama_unstuffed[2] == C_INFO(packet))){
        packet = (packet + 1) % 2;
        sendBuffer(A_TRANSMITER, RR(packet));
        memcpy(buffer, &trama_unstuffed[4], trama_size - 5);
        printf("STATUS 1 ALL OK: %x , %x\nSENDING RESPONSE\n", received, trama_unstuffed[2]);
        return trama_size - 5;
    }
    else if (received == expected){
        sendBuffer(A_TRANSMITER, RR(packet));
        tcflush(fd, TCIFLUSH);
        printf("Duplicate packet!\n");
    }
    else {
        sendBuffer(A_TRANSMITER, REJ(packet));
        tcflush(fd, TCIFLUSH);
        printf("Error in BCC2, sent REJ\n");
    }
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
                bytes = sendBuffer(A_TRANSMITER, DISC);
                printf("Number of written bytes: %d \n\n", bytes);
                printf("DISC was sent in order to terminate\n\n");
                alarm(timeout);
                alarmEnabled = TRUE;
            }
            stateMachine(A_RECEIVER, DISC, 0, 0);
        }

        if (alarmCount == maxTries) {
            return -1;
        }

        printf("DISC was received\n\n");
        bytes = sendBuffer(A_RECEIVER, C_UA);
        printf("Sending UA now\n\n");
        printf("Number of written bytes: %d \n\n", bytes);
    } 
    else if (role == LlRx) {
        while (!STOP) {
            stateMachine(A_RECEIVER, DISC, 0, 0);
        }

        printf("DISC was received\n\n");
        sendBuffer(A_RECEIVER, C_UA);
        printf("Sending UA now\n\n");

        current_state = START;
        STOP = FALSE;

        while (!STOP) {
            stateMachine(A_RECEIVER, C_UA, 0, 0);
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

