#include "../include/download.h"

void parseURLWithoutUserInfo(char *input, struct URL *url) {
    sscanf(input, "%*[^/]//%[^/]", url->host);
    strcpy(url->user, "anonymous");
    strcpy(url->password, "password");
    sscanf(input, "%*[^/]//%*[^/]/%s", url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);
}

void parseURLWithUserInfo(char *input, struct URL *url) {
    sscanf(input, "%*[^/]//%*[^@]@%[^/]", url->host);
    sscanf(input, "%*[^/]//%[^:/]", url->user);
    sscanf(input, "%*[^/]//%*[^:]:%[^@\n$]", url->password);
    sscanf(input, "%*[^/]//%*[^/]/%s", url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);
}

int parseURL(char *input, struct URL *url) {
     regex_t barRegex, atRegex;
     regcomp(&barRegex, "/", 0);
     if (regexec(&barRegex, input, 0, NULL, 0)) return -1;

     regcomp(&atRegex, "@", 0);
     if (regexec(&atRegex, input, 0, NULL, 0) != 0) parseURLWithoutUserInfo(input, url);
     else {
          parseURLWithUserInfo(input, url);
          if (strlen(url->user) == 0)
               strcpy(url->user, "anonymous");
     }

     struct hostent *h;
     if (strlen(url->host) == 0) return -1;
     if ((h = gethostbyname(url->host)) == NULL) {
          printf(" > Invalid hostname '%s'\n", url->host);
          exit(-1);
     }
     strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

     return !(strlen(url->host) && strlen(url->user) && strlen(url->password) && strlen(url->resource) && strlen(url->file));
}

// 100% copied from given code, cannot be wrong
int createAndConnectSocket(char *ip, int port) { 
     int sockfd;
     struct sockaddr_in server_addr;

     bzero((char *) &server_addr, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_addr.s_addr = inet_addr(ip);  /*32 bit Internet address network byte ordered*/
     server_addr.sin_port = htons(port);             /*server TCP port must be network byte ordered*/

     if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
           perror("socket()");
           exit(-1);
     }

     if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
     }

     printf(" > Create socket worked fine\n");
     return sockfd;
}

int authenticateConnection(const int socket, const char *user, const char *pass){
    char userCommand[MAX_SIZE];
    char passCommand[MAX_SIZE];
    char answer[MAX_SIZE];

    snprintf(userCommand, sizeof(userCommand), "user %s\n", user);
    snprintf(passCommand, sizeof(passCommand), "pass %s\n", pass);

    write(socket, userCommand, strlen(userCommand));
    
    if (readFTPServerResponse(socket, answer) != FTP_USER_NAME_OKAY) {
        printf(" > Error finding user");
        exit(EXIT_FAILURE);
    }

    write(socket, passCommand, strlen(passCommand));
    return readFTPServerResponse(socket, answer);
}

int readFTPServerResponse(const int socket, char *buffer){
    char byte;
    int index = 0, responseCode, state = 0;

    memset(buffer, 0, MAX_SIZE);

    while (state != 3) {
        read(socket, &byte, 1);
        if (state == 0) {
               if (byte == ' ') state = 1;
               else if (byte == '-') state = 2;
               else if (byte == '\n') state = 3;
               else buffer[index++] = byte;
        }
        else if (state == 1) {
               if (byte == '\n') state = 3;
               else buffer[index++] = byte;
        }
        else if (state == 2) {
               if (byte == '\n') {
                    memset(buffer, 0, MAX_SIZE);
                    state = 0;
                    index = 0;
               } 
               else buffer[index++] = byte;
        }
    }

    sscanf(buffer, "%d", &responseCode);
    return responseCode;
}

int requestFTPResource(const int socket, char *resource){
     char answer[MAX_SIZE], fileCommand[5 + strlen(resource) + 1];
     sprintf(fileCommand, "retr %s\n", resource);
     write(socket, fileCommand, strlen(fileCommand));          
     return readFTPServerResponse(socket, answer);
}

int enterPassiveMode(const int socket, char *ip, int *port) {
    char answer[MAX_SIZE];
    int ip1, ip2, ip3, ip4, port1, port2;
    write(socket, "pasv\n", 5);
    if (readFTPServerResponse(socket, answer) != 227) return -1;

    sscanf(answer, "%*[^(](%d,%d,%d,%d,%d,%d)%*[^\n$)]", &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    *port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    return 227;
}

void enter_passive_mode(int sockfd, char* ip, int* port){
  char response[MAX_SIZE];

  if(write(sockfd, "PASV\r\n", response, 1) != 0){
    fprintf(stderr, "Error entering passive mode. Exiting...\n");
    exit(1);
  }

  int values[6];
  char* data = strchr(response, '(');
  sscanf(data, "(%d, %d, %d, %d, %d, %d)", &values[0],&values[1],&values[2],&values[3],&values[4],&values[5]);
  sprintf(ip, "%d.%d.%d.%d", values[0],values[1],values[2],values[3]);
  *port = values[4]*256+values[5];
}

int getFTPResource(const int controlSocket, const int dataSocket, char *filename) {
    FILE *fd = fopen(filename, "wb+");

    if (fd == NULL) {
        printf(" > Error opening file\n");
        exit(-1);
    }

    char buffer[MAX_SIZE];
    int bytes = 1;

    while (bytes > 0){
        bytes = read(dataSocket, buffer, MAX_SIZE);
        if (bytes > 0 && fwrite(buffer, bytes, 1, fd) < 0) {
            fclose(fd);
            return -1;
        }
    }

    fclose(fd);
    return readFTPServerResponse(controlSocket, buffer);
}

int closeFTPConnection(const int controlSocket, const int dataSocket){
    char answer[MAX_SIZE];

    write(controlSocket, "quit\n", 5);
    if (readFTPServerResponse(controlSocket, answer) != 221) 
        return -1;

    return close(controlSocket) || close(dataSocket);
}

void printURLInfo(const struct URL *url) {
     printf(" > Host: %s\n > Resource: %s\n > File: %s\n > User: %s\n > Password: %s\n > IP Address: %s\n\n",
     url->host, url->resource, url->file, url->user, url->password, url->ip);
}

void printSocketError(const char *destination, const char *ip, int port) {
     printf(" > Socket to '%s:%d' for %s failed\n", ip, port, destination);
     exit(EXIT_FAILURE);
}

void printError(const char *message) {
     printf(" > Error: %s\n", message);
     exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int port;
    char ip[MAX_SIZE];
    char answer[MAX_SIZE];
    struct URL url;
    
    memset(&url, 0, sizeof(url));

    if (argc != 2) 
        printError("Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");

    if (parseURL(argv[1], &url) != 0) 
        printError("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");
     
    printURLInfo(&url);

    int socketA = createAndConnectSocket(url.ip, SERVER_PORT);

    if (socketA < 0 || readFTPServerResponse(socketA, answer) != 220) 
        printSocketError("control connection", url.ip, SERVER_PORT);

    if (authenticateConnection(socketA, url.user, url.password) != 230) 
        printError("Authentication failed");
     
    if (enterPassiveMode(socketA, ip, &port) != 227) 
        printError("Passive mode failed");

    int socketB = createAndConnectSocket(ip, port);

    if (socketB < 0) printSocketError("data connection", ip, port);

    if (requestFTPResource(socketA, url.resource) != 150) 
        printError("Unknown resource");

    if (getFTPResource(socketA, socketB, url.file) != 226) 
        printError("Error transferring file");

    if (closeFTPConnection(socketA, socketB) != 0) 
        printError("Sockets close error");

    printf(" > File downloaded!\n");
    return 0;
}