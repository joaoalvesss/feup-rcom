#include "../include/download.h"

/**
 * The struct hostent (host entry) with its terms documented

    struct hostent {
        char *h_name;    // Official name of the host.
        char **h_aliases;    // A NULL-terminated array of alternate names for the host.
        int h_addrtype;    // The type of address being returned; usually AF_INET.
        int h_length;    // The length of the address in bytes.
        char **h_addr_list;    // A zero-terminated array of network addresses for the host.
        // Host addresses are in Network Byte Order.
    };

    #define h_addr h_addr_list[0]	The first address in h_addr_list.
*/

int main(int argc, char **argv){
     return 0;
}

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
          }
     }

     fclose(fd);
     printf(" > Exiting getFile\n");
}

void parseFilename(char *path, char *filename){
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


int getServerPortFromResponse(int socketfd) {
     int state = 0;
     int index = 0;
     char firstByte[4] = {0};  // Initialize with zeros
     char secondByte[4] = {0}; // Initialize with zeros
     char current;

     while (state != 7) {
          read(socketfd, &current, 1);
          printf("%c", current);

          switch (state) {
          case 0:
               // Waits for 3 digit number followed by ' '
               if (current == ' ') {
                    if (index != 3) {
                         printf(" > Error receiving response code\n");
                         return -1;
                    }
                    index = 0;
                    state = 1;
               } else {
                    index++;
               }
               break;
          case 5:
               // Reads until the first comma, extracting the first byte of the port
               if (current == ',') {
                    index = 0;
                    state++;
               } else {
                    firstByte[index++] = current;
               }
               break;
          case 6:
               // Reads until the closing parenthesis, extracting the second byte of the port
               if (current == ')') {
                    state++;
               } else {
                    secondByte[index++] = current;
               }
               break;
          default:
               // Reads until the first comma
               if (current == ',') {
                    state++;
               }
               break;
          }
     }

     // Converts the extracted bytes to integers and returns the calculated port
     int firstByteInt = atoi(firstByte);
     int secondByteInt = atoi(secondByte);
     return (firstByteInt * 256 + secondByteInt);
}


int sendCommandInterpretResponse(int socketfd, char cmd[], char commandContent[], char* filename, int socketfdClient) {
     char responseCode[3];
     int action = 0;

     // sends the command
     write(socketfd, cmd, strlen(cmd));
     write(socketfd, commandContent, strlen(commandContent));
     write(socketfd, "\n", 1);

     while (1) {
          // reads response
          readResponse(socketfd, responseCode);
          action = responseCode[0] - '0';

          switch (action) {
          case 1:
               // wait for response
               if (strcmp(cmd, "retr ") == 0) {
                    getFile(socketfdClient, filename);
                    break;
               }
               readResponse(socketfd, responseCode);
               break;
          case 2:
               // command accepted
               return 0;
          case 3:
               // needs more info
               return 1;
          case 4:
               // try again
               write(socketfd, cmd, strlen(cmd));
               write(socketfd, commandContent, strlen(commandContent));
               write(socketfd, "\r\n", 2);
               break;
          case 5:
               // command rejected, exit
               printf(" > Command rejected\n");
               close(socketfd);
               exit(-1);
          }
     }
}