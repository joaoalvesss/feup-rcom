#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
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

/***** MAIN FUNCTIONS *****/ 

/**
 * @brief Reads and parses the FTP server response from the given socket.
 *
 * This function reads the response from the server character by character, 
 * interpreting the FTP protocol's response codes. The response code is 
 * extracted from the server's message and returned as an integer.
 *
 * @param socket The socket connected to the FTP server.
 * @param buffer A character buffer to store the server's response.
 * @return The FTP server response code.
 */
int readFTPServerResponse(const int socket, char *buffer);

/**
 * @brief Requests a resource from an FTP server.
 *
 * This function constructs an FTP "retr" command with the specified resource,
 * sends the command to the server, and reads the server's response.
 *
 * @param socket The control socket connected to the FTP server.
 * @param resource A pointer to a char array containing the resource to request.
 * @return Returns the FTP server response code.
 *         Exits the program with an error message on failure.
 */
int requestFTPResource(const int socket, const char *resource);

/**
 * @brief Authenticates the connection with an FTP server.
 *
 * This function sends the user and password commands to the server,
 * reads the server's responses, and returns the final authentication status.
 *
 * @param socket The control socket connected to the FTP server.
 * @param username A pointer to a char array containing the FTP username.
 * @param password A pointer to a char array containing the FTP password.
 * @return Returns the FTP server response code for successful authentication.
 *         Exits the program with an error message on failure.
 */
int authenticateConnection(const int socket, const char *username, const char *password);

/**
 * @brief Creates and connects a socket to the specified IP address and port.
 *
 * This function creates a TCP socket, sets up the server address structure, and
 * connects to the specified IP address and port.
 *
 * @param ip A pointer to a char array containing the IP address to connect to.
 * @param port The port number to connect to.
 * @return Returns the socket file descriptor on success.
 *         Exits the program with an error message on failure.
 */
int createAndConnectSocket(char *ip, int port);

/**
 * @brief Retrieves a file from the FTP server and saves it to a local file.
 *
 * This function reads data from the data socket and writes it to a local file specified by the filename.
 *
 * @param controlSocket The control socket for FTP communication.
 * @param dataSocket The data socket for transferring file data.
 * @param filename The name of the local file to save the retrieved data.
 * @return Returns the FTP server response code after completing the file transfer.
 */
int getFTPResource(const int controlSocket, const int dataSocket, char *filename);

/**
 * @brief Closes the FTP connection by sending a quit command to the server.
 *
 * This function sends a quit command to the FTP server, reads the server response,
 * and closes both the control and data sockets.
 *
 * @param controlSocket The control socket for FTP communication.
 * @param dataSocket The data socket for transferring file data.
 * @return Returns 0 on success, -1 on failure.
 */
int closeFTPConnection(const int controlSocket, const int dataSocket);

/**
 * Parses an FTP URL and extracts relevant information such as
 * user, password, host, resource, filename, and IP address.
 *
 * @param input The FTP URL to be parsed.
 * @param user Output parameter for the username.
 * @param pass Output parameter for the password.
 * @param host Output parameter for the host.
 * @param resource Output parameter for the resource path.
 * @param filename Output parameter for the filename.
 * @param ip Output parameter for the IP address.
 * @return Returns 0 on success, -1 on failure.
 */
int parseArgument(const char *argument, char *user, char *pass, char *host, char *path, char *filename, char *ip);

/**
 * @brief Enters passive mode for FTP data transfer.
 *
 * This function sends a "pasv" command to the FTP server, reads the server response,
 * and extracts the IP address and port for passive mode data transfer.
 *
 * @param socket The control socket for FTP communication.
 * @param ip A pointer to a char array to store the extracted IP address.
 * @param port A pointer to an integer to store the extracted port number.
 * @return Returns FTP_ENTERING_PASSIVE_MODE on success, -1 on failure.
 */
int enterPassiveMode(const int socket, char *ip, int *port);

/***** AUXILIARY FUNCTIONS *****/ 

/**
 * @brief Prints URL information, including user, password, host, resource, file, and IP address.
 *
 * This function takes the components of a URL and prints them to the console.
 *
 * @param user The username for authentication.
 * @param password The password for authentication.
 * @param host The host address of the URL.
 * @param resource The resource part of the URL.
 * @param file The filename part of the URL.
 * @param ip The IP address associated with the host.
 */
void printURLInfo(char *user, char *password, char *host, char *resource, char *file, char *ip);

/**
 * @brief Prints an error message related to socket connection failure.
 *
 * This function prints an error message when a socket connection to a destination fails.
 *
 * @param destination The destination information.
 * @param ip The IP address of the destination.
 * @param port The port number of the destination.
 */
void printSocketError(const char *destination, const char *ip, int port);

/**
 * @brief Prints a general error message and exits the program.
 *
 * This function prints a general error message to the console and exits the program with a failure status.
 *
 * @param message The error message to be printed.
 */
void printError(const char *message);


