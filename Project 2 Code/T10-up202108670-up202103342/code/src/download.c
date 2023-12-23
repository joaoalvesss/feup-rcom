#include "../include/download.h"

int parseArgument(const char *input, char *user, char *pass, char *host, char *resource, char *filename, char *ip) {
    const char *start = "ftp://";
    int index = 0;
    int i = 6;

    // Extract host from the FTP URL
    while (input[i] != '/') {
        host[index++] = input[i++];
    }
    host[index] = '\0';
    i++;
    index = 0;

    // Extract resource from the FTP URL
    while (input[i] != '\0') {
        resource[index++] = input[i++];
    }
    resource[index] = '\0';

    int auth = 0;
    // Check if authentication information is present in the host
    for (int i = 0; i < strlen(host); i++) {
        if (host[i] == '@') {
            auth = 1;
        }
    }

    // Parse authentication information if present, otherwise use anonymous credentials
    if (auth) {
        index = 0;
        while (host[0] != ':') {
            user[index++] = host[0];
            for (int j = 0; j < strlen(host); j++) {
                host[j] = host[j + 1];
            }
        }
        for (int j = 0; j < strlen(host); j++) {
            host[j] = host[j + 1];
        }

        index = 0;
        while (host[0] != '@') {
            pass[index++] = host[0];
            for (int j = 0; j < strlen(host); j++) {
                host[j] = host[j + 1];
            }
        }
        for (int j = 0; j < strlen(host); j++) {
            host[j] = host[j + 1];
        }
    } else {
        strcpy(user, "anonymous");
        strcpy(pass, "anonymous");
    }

    int counter = 0;
    // Count the number of '/' characters in the resource path
    for (int i = 0; i < strlen(resource); i++) {
        if (resource[i] == '/') counter++;
    }
    int counter2 = 0;
    index = 0;

    // Extract filename from the resource path
    for (int i = 0; i < strlen(resource); i++) {
        if (counter2 == counter) {
            filename[index++] = resource[i];
        }
        if (resource[i] == '/') counter2++;
    }
    filename[index] = '\0';

    struct hostent *h;
    // Check if the host is valid and retrieve its IP address
    if (strlen(host) == 0) return -1;
    if ((h = gethostbyname(host)) == NULL) {
        printf(" > Invalid hostname '%s'\n", host);
        exit(-1);
    }
    strcpy(ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    // Return 0 on success, -1 on failure
    return (strlen(host) && strlen(user) && strlen(pass) && strlen(resource) && strlen(filename)) ? 0 : 1;
}


// 100% copied from given code, cannot be wrong
int createAndConnectSocket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Initialize the server address structure
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);  /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);             /*server TCP port must be network byte ordered*/

    // Create a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    // Connect to the specified IP address and port
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    printf(" > Create socket worked fine\n");

    return sockfd;
}

int authenticateConnection(const int socket, const char *username, const char *password){
    char uCmd[MAX_SIZE];
    char pCmd[MAX_SIZE];
    char response[MAX_SIZE];

    // Construct user and pass commands
    snprintf(uCmd, sizeof(uCmd), "user %s\n", username);
    snprintf(pCmd, sizeof(pCmd), "pass %s\n", password);

    // Send the user command to the server
    write(socket, uCmd, strlen(uCmd));
    
    // Check the server response for user command
    if (readFTPServerResponse(socket, response) != FTP_USER_NAME_OKAY) {
        printf(" > Error finding user");
        exit(EXIT_FAILURE);
    }

    // Send the password command to the server
    write(socket, pCmd, strlen(pCmd));

    // Return the server response for password command
    return readFTPServerResponse(socket, response);
}

int readFTPServerResponse(const int socket, char *buffer) {
    char byte;
    int index = 0, responseCode, state = 0;

    // Initialize the buffer with zeros
    memset(buffer, 0, MAX_SIZE);

    // Continue reading until the end of the response
    while (state != 3) {
        // Read one byte from the socket
        read(socket, &byte, 1);

        // Interpret the byte based on the current state
        if (state == 0) {
            if (byte == ' ') state = 1;
            else if (byte == '-') state = 2;
            else if (byte == '\n') state = 3;
            else buffer[index++] = byte;
        } else if (state == 1) {
            if (byte == '\n') state = 3;
            else buffer[index++] = byte;
        } else if (state == 2) {
            if (byte == '\n') {
                // Reset buffer on multi-line response
                memset(buffer, 0, MAX_SIZE);
                state = 0;
                index = 0;
            } else {
                buffer[index++] = byte;
            }
        }
    }

    // Parse the response code from the buffer
    sscanf(buffer, "%d", &responseCode);

    // Return the FTP server response code
    return responseCode;
}

int requestFTPResource(const int socket, const char *resource) {
    char answer[MAX_SIZE];
    
    // Construct the FTP retr command with the resource
    char fileCommand[MAX_SIZE];
    snprintf(fileCommand, sizeof(fileCommand), "retr %s\n", resource);
    
    // Send the command to the server
    write(socket, fileCommand, strlen(fileCommand));
    
    // Read and return the server response
    return readFTPServerResponse(socket, answer);
}


int enterPassiveMode(const int socket, char *ip, int *port) {
    char answer[MAX_SIZE];
    int ip1, ip2, ip3, ip4, port1, port2;

    // Send the "pasv" command to the server
    write(socket, "pasv\n", 5);

    // Read the server response and check if it indicates entering passive mode
    if (readFTPServerResponse(socket, answer) != FTP_ENTERING_PASSIVE_MODE)
        return -1;

    // Find the opening parenthesis in the response
    char *start = strchr(answer, '(');
    if (start == NULL)
        return -1;

    // Extract IP address and port from the response
    if (sscanf(start + 1, "%d,%d,%d,%d,%d,%d", &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6)
        return -1;

    // Format the IP address
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    // Calculate the port number
    *port = port1 * 256 + port2;

    return FTP_ENTERING_PASSIVE_MODE;
}

int getFTPResource(const int controlSocket, const int dataSocket, char *filename) {
    FILE *fd = fopen(filename, "wb+");

    if (fd == NULL) {
        printf(" > Error opening file\n");
        exit(-1);
    }

    char buffer[MAX_SIZE];
    int bytes = 1;

    // Read data from the data socket and write it to the local file
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

    // Send the quit command to the server
    write(controlSocket, "quit\n", 5);
    
    // Read the server response and check if it indicates successful service closing
    if (readFTPServerResponse(controlSocket, answer) != FTP_SERVICE_CLOSING) return -1;

    // Close both control and data sockets
    return close(controlSocket) || close(dataSocket);
}

void printURLInfo(char *user, char *password, char *host, char *resource, char *file, char *ip) {
    printf(" > Host: %s\n > Resource: %s\n > File: %s\n > User: %s\n > Password: %s\n > IP Address: %s\n\n", host, resource, file, user, password, ip);
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
    char answer[MAX_SIZE];
    char user[MAX_SIZE];
    char password[MAX_SIZE];
    char host[MAX_SIZE];
    char resource[MAX_SIZE];
    char file[MAX_SIZE];
    char ip[MAX_SIZE];
    
    // Initialize arrays
    memset(answer, 0, MAX_SIZE);
    memset(user, 0, MAX_SIZE);
    memset(password, 0, MAX_SIZE);
    memset(host, 0, MAX_SIZE);
    memset(resource, 0, MAX_SIZE);
    memset(file, 0, MAX_SIZE);
    memset(ip, 0, MAX_SIZE);

    // Check the number of command-line arguments
    if (argc != 2) 
        printError(" > Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");

    // Parse the FTP URL
    if (parseArgument(argv[1], user, password, host, resource, file, ip) != 0)
        printError(" > Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");
     
    // Print parsed FTP URL information
    printURLInfo(user, password, host, resource, file, ip);

    // Create and connect the control socket
    int controlSocket = createAndConnectSocket(ip, SERVER_PORT);

    // Check the control socket and server response
    if (controlSocket < 0 || readFTPServerResponse(controlSocket, answer) != FTP_SERVICE_READY) 
        printSocketError("control connection", ip, SERVER_PORT);

    // Authenticate the FTP connection
    if (authenticateConnection(controlSocket, user, password) != FTP_USER_LOGGED_IN) 
        printError(" > Authentication failed");
     
    // Enter passive mode and get the data port
    if (enterPassiveMode(controlSocket, ip, &port) != FTP_ENTERING_PASSIVE_MODE) 
        printError(" > Passive mode failed");

    // Create and connect the data socket
    int dataSocket = createAndConnectSocket(ip, port);

    // Check the data socket
    if (dataSocket < 0) 
        printSocketError("data connection", ip, port);

    // Request the FTP resource
    if (requestFTPResource(controlSocket, resource) != FTP_FILE_STATUS_OKAY) 
        printError(" > Unknown resource");

    // Get the FTP resource and check the data connection status
    if (getFTPResource(controlSocket, dataSocket, file) != FTP_CLOSING_DATA_CONNECTION) 
        printError(" > Error transferring file");

    // Close both control and data sockets
    if (closeFTPConnection(controlSocket, dataSocket) != 0) 
        printError(" > Sockets close error");

    // Print success message
    printf(" > File downloaded!\n");
    return 0;
}
