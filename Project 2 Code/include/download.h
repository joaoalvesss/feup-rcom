#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <ctype.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"
#define MAX_SIZE 500

void readResponse(int socketfd, char *responseCode);

struct hostent *getHostInfo(const char host[]);

void getFile(int socket, char* filename);

void parseArgument(char *argument, char *user, char *pass, char *host, char *path);

int sendCommandInterpretResponse(int socketfd, char cmd[], char commandContent[], char* filename, int socketfdClient); // TODO

int getServerPortFromResponse(int socketfd); // TODO

void parseFilename(char *path, char *filename);
