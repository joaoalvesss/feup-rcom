#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <termios.h>

#define MAX_SIZE        1000
#define SERVER_PORT     21

// https://datatracker.ietf.org/doc/html/rfc959
// PAG 39 - 42
// like hmtl response codes
// maybe need some more (?)
#define FTP_FILE_STATUS_OKAY          150  // File status okay; about to open data connection
#define FTP_SERVICE_READY             220  // Service ready for new user
#define FTP_SERVICE_CLOSING           221  // Service closing control connection
#define FTP_CLOSING_DATA_CONNECTION   226  // Closing data connection
#define FTP_ENTERING_PASSIVE_MODE     227  // Entering Passive Mode (h1,h2,h3,h4,p1,p2)
#define FTP_USER_LOGGED_IN            230  // User logged in, proceed
#define FTP_USER_NAME_OKAY            331  // User name okay, need password

struct URL {
    char host[MAX_SIZE];      
    char resource[MAX_SIZE]; 
    char file[MAX_SIZE];     
    char user[MAX_SIZE];      
    char password[MAX_SIZE]; 
    char ip[MAX_SIZE];        
};


/***** Main functions *****/ 

int readFTPServerResponse(const int socket, char *buffer);
int requestFTPResource(const int socket, char *resource);
int authenticateConnection(const int socket, const char *user, const char *pass);
int createAndConnectSocket(char *ip, int port);
int getFTPResource(const int controlSocket, const int dataSocket, char *filename);
int closeFTPConnection(const int controlSocket, const int dataSocket);
int parseURL(char *input, struct URL *url);
int enterPassiveMode(const int socket, char *ip, int *port);


/***** Auxiliary functions *****/ 

void printURLInfo(const struct URL *url);
void printSocketError(const char *destination, const char *ip, int port);
void printError(const char *message);
void parseURLWithUserInfo(char *input, struct URL *url);
void parseURLWithoutUserInfo(char *input, struct URL *url);