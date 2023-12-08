#include "include/download.h"


int createSocket(char *ip, int port) { // copied from given code
     int sockfd;
     struct sockaddr_in server_addr;

     bzero((char *) &server_addr, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);  /*32 bit Internet address network byte ordered*/
     server_addr.sin_port = htons(SERVER_PORT);             /*server TCP port must be network byte ordered*/

     /*open a TCP socket*/
     if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
           perror("socket()");
           exit(-1);
     }
     /*connect to the server*/
     if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
     }

     printf(" > Exiting createSocket function\n");
     return sockfd;
}

void readResponse(const int socket, char *responseCode) {
     int state = 0;
     int index = 0;
     char c;

     while (state != 3) {	
          read(socket, &c, 1);
          printf("%c", c);

          switch (state) {
               case 0:
                    if (c == ' ') {
                         if (index != 3) {
                              printf(" > Error in code\n");
                              return;
                         }
                         index = 0;
                         state = 1;
                    } else {
                         if (c == '-') {
                              state = 2;
                              index = 0;
                         } else if (isdigit(c)) {
                              responseCode[index++] = c;
                         }
                    }
                    break;
               case 1:
                    if (c == '\n') {
                         state = 3;
                    }
                    break;
               case 2:
                    if (c == responseCode[index]) {
                         index++;
                    } else if (index == 3 && c == ' ') {
                         state = 1;
                    } else if (index == 3 && c == '-') {
                         index = 0;
                    }
                    break;
          }
     }
     printf(" > Exiting readResponse function\n");
}


void getFile(const int socket, char *filename) {
     FILE *fd = fopen(filename, "wb+");

     if(fd == NULL){
          printf(" > Error getting file\n");
          exit(-1);
     }

     int bytes_num = 1;
     char buffer[MAX_SIZE];

     while(bytes_num > 0){
          bytes_num = read(socket, buffer, MAX_SIZE);
          if (bytes_num > 0 && fwrite(buffer, bytes_num, 1, fd) < 0) {
               fclose(fd);
               return -1;
          }
     }

     fclose(fd);
     printf(" > Exiting getFile\n");
}

void parseFilename(const char *path, char *filename){
     int indexFilename = 0;
     size_t pathLength = strlen(path);

     // reset the filename buffer
     memset(filename, 0, 80);

     for (size_t indexPath = 0; indexPath < pathLength; ++indexPath){
          if (path[indexPath] == '/'){
            // reset when encountering /
            indexFilename = 0;
            memset(filename, 0, 80);
          } 
          else{
            // copy characters to the filename buffer
            filename[indexFilename++] = path[indexPath];
          }
     }

     printf(" > Exiting parseFilename\n");
}

struct hostent *getHostInfo(const char host[]) {
     struct hostent *hostInfo;

     // retrieve host information
     if ((hostInfo = gethostbyname(host)) == NULL) {
          perror("gethostbyname");
          exit(EXIT_FAILURE);
     }

     printf(" > Exiting getHostInfo\n");
     return hostInfo;
}

void parseArgument(char *argument, char *user, char *pass, char *host, char *path) {
    char start[] = "ftp://";
    int index = 0;
    int i = 0;
    int state = 0;
    int length = strlen(argument);

    while (i < length) {
        switch (state) {
        case 0: // reads the ftp://
            if (argument[i] == start[i] && i < 5) {
                break;
            }
            if (i == 5 && argument[i] == start[i])
                state = 1;
            else
                printf(" > Error parsing ftp://");
            break;
        case 1: // reads the username
            if (argument[i] == ':') {
                state = 2;
                index = 0;
            } else {
                user[index] = argument[i];
                index++;
            }
            break;
        case 2: // reads the password
            if (argument[i] == '@') {
                state = 3;
                index = 0;
            } else {
                pass[index] = argument[i];
                index++;
            }
            break;
        case 3: // reads the host
            if (argument[i] == '/') {
                state = 4;
                index = 0;
            } else {
                host[index] = argument[i];
                index++;
            }
            break;
        case 4: // reads the path
            path[index] = argument[i];
            index++;
            break;
        }

        i++;
    }
    printf(" > Exiting parseArgument\n");
}