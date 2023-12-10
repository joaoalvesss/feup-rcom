#include "../include/download.h"

void parseURLWithoutUserInfo(char *input, struct URL *url) {
    sscanf(input, HOST_REGEX, url->host);
    strcpy(url->user, "anonymous");
    strcpy(url->password, "password");
    sscanf(input, RESOURCE_REGEX, url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);
}

void parseURLWithUserInfo(char *input, struct URL *url) {
    sscanf(input, HOST_AT_REGEX, url->host);
    sscanf(input, USER_REGEX, url->user);
    sscanf(input, PASS_REGEX, url->password);
    sscanf(input, RESOURCE_REGEX, url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);
}

int parse(char *input, struct URL *url) {
     regex_t barRegex, atRegex;
     regcomp(&barRegex, "/", 0);
     if (regexec(&barRegex, input, 0, NULL, 0)) return -1;

     regcomp(&atRegex, "@", 0);
     if (regexec(&atRegex, input, 0, NULL, 0) != 0) {
          parseURLWithoutUserInfo(input, url);
     } 
     else {
          parseURLWithUserInfo(input, url);
          if (strlen(url->user) == 0) {
               strcpy(url->user, "anonymous");
          }
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
int createSocket(char *ip, int port) { 
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

int authConn(const int socket, const char *user, const char *pass) {
    char userCommand[MAX_SIZE];
    char passCommand[MAX_SIZE];
    char answer[MAX_SIZE];

    snprintf(userCommand, sizeof(userCommand), "user %s\n", user);
    snprintf(passCommand, sizeof(passCommand), "pass %s\n", pass);

    write(socket, userCommand, strlen(userCommand));
    
    if (readResponse(socket, answer) != FTP_USER_NAME_OKAY) {
        printf(" > Error finding user");
        exit(EXIT_FAILURE);
    }

    write(socket, passCommand, strlen(passCommand));
    return readResponse(socket, answer);
}

int passiveMode(const int socket, char *ip, int *port) {
    char answer[MAX_SIZE];
    int ip1, ip2, ip3, ip4, port1, port2;

    write(socket, "pasv\n", 5);
    if (readResponse(socket, answer) != FTP_ENTERING_PASSIVE_MODE) {
        return -1;
    }

    sscanf(answer, PASSIVE_REGEX, &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    *port = port1 * 256 + port2;
    snprintf(ip, MAX_SIZE, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    return FTP_ENTERING_PASSIVE_MODE;
}

int readResponse(const int socket, char *buffer) {
    char byte;
    int index = 0, responseCode, state = 0;

    memset(buffer, 0, MAX_SIZE);

    while (state != 3) {
        read(socket, &byte, 1);
        switch (state) {
               case 0:
                    if (byte == ' ') state = 1;
                    else if (byte == '-') state = 2;
                    else if (byte == '\n') state = 3;
                    else buffer[index++] = byte;
                    break;
               case 1:
                    if (byte == '\n') state = 3;
                    else buffer[index++] = byte;
                    break;
               case 2:
                    if (byte == '\n') {
                         memset(buffer, 0, MAX_SIZE);
                         state = 0;
                         index = 0;
                    } 
                    else
                         buffer[index++] = byte;
               break;
               case 3:
                    break;
               default:
                    break;
        }
    }

    sscanf(buffer, RESPCODE_REGEX, &responseCode);
    return responseCode;
}



int requestResource(const int socket, char *resource) {
    char fileCommand[5 + strlen(resource) + 1], answer[MAX_SIZE];

    sprintf(fileCommand, "retr %s\n", resource);

    write(socket, fileCommand, strlen(fileCommand));

    return readResponse(socket, answer);
}

int getResource(const int socketA, const int socketB, char *filename) {
    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) {
        printf(" > Error opening or creating file '%s'\n", filename);
        exit(-1);
    }

    char buffer[MAX_SIZE];
    int bytes;

    do {
        bytes = read(socketB, buffer, MAX_SIZE);
        if (bytes > 0 && fwrite(buffer, bytes, 1, fd) < 0) {
            fclose(fd);
            return -1;
        }
    } while (bytes);

    fclose(fd);

    return readResponse(socketA, buffer);
}

int closeConnection(const int socketA, const int socketB) {
    char answer[MAX_SIZE];

    write(socketA, "quit\n", 5);
    if (readResponse(socketA, answer) != FTP_SERVICE_CLOSING) {
        return -1;
    }

    return close(socketA) || close(socketB);
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

     if (parse(argv[1], &url) != 0)
          printError("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");
     
     printURLInfo(&url);

     int socketA = createSocket(url.ip, SERVER_PORT);
     if (socketA < 0 || readResponse(socketA, answer) != FTP_SERVICE_READY)
          printSocketError("control connection", url.ip, SERVER_PORT);

     
     if (authConn(socketA, url.user, url.password) != FTP_USER_LOGGED_IN)
          printError("Authentication failed");
     
     if (passiveMode(socketA, ip, &port) != FTP_ENTERING_PASSIVE_MODE)
          printError("Passive mode failed");

     int socketB = createSocket(ip, port);
     if (socketB < 0)
          printSocketError("data connection", ip, port);

     if (requestResource(socketA, url.resource) != FTP_FILE_STATUS_OKAY)
          printError("Unknown resource");

     if (getResource(socketA, socketB, url.file) != FTP_CLOSING_DATA_CONNECTION) 
          printError("Error transferring file");

     if (closeConnection(socketA, socketB) != 0) 
          printError("Sockets close error");

     printf(" > File downloaded!\n");
     return 0;
}
