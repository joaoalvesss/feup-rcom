#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"
#define MAX_SIZE 1000

struct URL {
    char host[MAX_LENGTH];      // 'ftp.up.pt'
    char resource[MAX_LENGTH];  // 'parrot/misc/canary/warrant-canary-0.txt'
    char file[MAX_LENGTH];      // 'warrant-canary-0.txt'
    char user[MAX_LENGTH];      // 'username'
    char password[MAX_LENGTH];  // 'password'
    char ip[MAX_LENGTH];        // 193.137.29.15
};

int parseUrl(char *input, struct URL *url); // TODO

int createSocket(char *ip, int port); 

int authConn(const int socket, const char *user, const char *pass); // TODO

void readResponse(const int socket, char *responseCode);

int passiveMode(const int socket, char* ip, int *port); // TODO

int requestResource(const int socket, char *resource); // TODO

void getFile(const int socket_one, const int socket_two, char *filename); 

int closeConnection(const int socketA, const int socketB); // TODO