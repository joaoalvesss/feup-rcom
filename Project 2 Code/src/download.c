#include "include/download.h"

int parseUrl(char *input, struct URL *url) {

     // PARTE IMPORTANTE
     printf(" > Exiting parseUrl function\n");
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

int authConn(const int socket, const char *user, const char *pass) {
     printf(" > Exiting authConn function\n");
     return 0;
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
     // talvez seja preciso adicionar return do responseCode
}

int passiveMode(const int socket, char* ip, int *port) {

     // não sei para que serve isto
     printf(" > Exiting passiveMode function\n");
     return 0;
}

int requestResource(const int socket, char *resource) {
     printf(" > Exiting requestResource function\n");
     return 0;
}

void getFile(const int socket_one, const int socket_two, char *filename) {
     FILE *fd = fopen(filename, "wb+");

     if(fd == NULL){
          printf(" > Error getting file\n");
          exit(-1);
     }

     int bytes_num = 1;
     char buffer[MAX_SIZE];

     while(bytes_num > 0){
          bytes_num = read(socket_one, buffer, MAX_SIZE);
          if (bytes_num > 0 && fwrite(buffer, bytes_num, 1, fd) < 0) {
               fclose(fd);
               return -1;
          }
     }

     
     close(fd);
     readResponse(socket_two, buffer);

     printf(" > Exiting getFile function\n");

     // talvez seja preciso adicionar algum tipo de return
     // relacionado com os sockets mas nao sei bem como
}

int closeConnection(const int socketA, const int socketB) {

     // há alguma funcao built in para isto?
     printf(" > Exiting closeConnection function\n");
     return 0;
}