// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE        B38400
#define _POSIX_SOURCE   1 // POSIX compliant source
#define FALSE           0
#define TRUE            1
#define BUF_SIZE        256
#define FLAG            0x7E
#define A               0x03
#define A_UA            0x01
#define C               0x03
#define C_UA            0x07
#define BCC             A^C
#define BCC_UA          A_UA^C_UA

typedef enum state_machine {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DONE} state_machine;

volatile int STOP = FALSE;
int timeout = 3;
int maxTries = 4;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    //printf("Alarm #%d\n", alarmCount);
}

int sendFrame(unsigned char* buffer, int n, int fd){

    int bytes = write(fd, buffer, n);
    if(bytes != n) printf("Error in writting bytes\n");

    printf("%d Bytes written\n", bytes);

    alarm(3);    
    alarmEnabled = TRUE;

    unsigned char buf_r[BUF_SIZE] = {0};
    printf("Reading now\n");
    int bytes1 = read(fd, buf_r, BUF_SIZE);

    printf("bytes1=%d \n", bytes1);

    alarm(0);   
    if(bytes1 == 0) return 1;

    unsigned char buf2[BUF_SIZE] = {FLAG, A_UA, C_UA, BCC_UA, FLAG}; // compare buffer

    for(int i = 0; i < bytes1; i++){
        if(buf2[i] != buf_r[i]){
            printf("Error in recieve UA\n");
            return 1;
        }
        printf("buf = 0x%02X\n", (unsigned int)(buf_r[i] & 0xFF));  
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send
    unsigned char buf[BUF_SIZE] = {FLAG, A, C, BCC, FLAG};
   
    (void)signal(SIGALRM, alarmHandler);  
   
    while(alarmCount < 4){
        if(sendFrame(buf, BUF_SIZE, fd) != 0){
            printf("Nice Alarm %d \n", alarmCount);
        }
        else break;
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}