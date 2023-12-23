#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

//Includes getip.c
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>

#include <ctype.h>

#define MAX_STRING_SIZE     128
#define SERVER_PORT 		21

// Example of input: ./run ftp://[<user>:<password>@]<host>/<url-path>
void parseInputString(char *input, char *user, char *password, char *host, char *url_path){
	
	int curr_index = 0;
	int i = 0;
	int curr_state = 0;
	int input_length = strlen(input);
    char init[] = "ftp://";

	while (i < input_length)
	{
		switch (curr_state)
		{
		case 0: //handle "ftp://"
			if (i < 5 && input[i] == init[i]) //still reading "ftp://"
				break;

			if (i == 5 && input[i] == init[i]) //finished "ftp://"
				curr_state = 1;

			else   //unexpected value
				printf("Expected 'ftp://' value\n");

			break;

		case 1: //handle user
			if (input[i] == ':'){ //finished user
				curr_state = 2;
				curr_index = 0;
			}
            else if(input[i] == '/'){ //no user, copy to host
                curr_state = 4;
		curr_index = 0;
		int j;
                for(j = 0; j < strlen(user); j++)
                    host[j] = user[j];
                memset(user, 0, MAX_STRING_SIZE);
            }
			else{ //still reading user
				user[curr_index] = input[i];
				curr_index++;
			}
			break;

		case 2: //handle password
			if (input[i] == '@'){ //finished password
				curr_state = 3;
				curr_index = 0;
			}
            else if(input[i] == '/'){ //no password, copy to host
                curr_state = 4;
		int j;		curr_index = 0;
                for(j = 0; j < strlen(user); j++)
                    host[j] = password[j];
                memset(password, 0, MAX_STRING_SIZE);
            }
			else{ // still reading password
				password[curr_index] = input[i];
				curr_index++;
			}
			break;

		case 3: //handle host
			if (input[i] == '/'){ //finished host
				curr_state = 4;
				curr_index = 0;
			}
			else{ //still reading host
				host[curr_index] = input[i];
				curr_index++;
			}
			break;
		case 4: //handle url_path (rest of the input)
			url_path[curr_index] = input[i];
			curr_index++;
			break;
		}
		i++;
	}

}

void parseFile(char *url_path, int url_path_size, char *filename){
	
	int curr_index = 0;
	int i;
	for(i = 0; i < url_path_size; i++){

		if(url_path[i] == '/'){ // current filename is a directory, reset and start again
			curr_index = 0;
			memset(filename, 0, MAX_STRING_SIZE);
		}
		else{
			filename[curr_index] = url_path[i];
			curr_index++;
		}
	}
}

// Codigo fornecido

struct hostent* getip(char* host)
{
    struct hostent * h;
	if ((h = gethostbyname(host)) == NULL) {  
        herror("gethostbyname");
        exit(1);
    }
	return h;
}

// Fim de Codigo fornecido



void readServerAws(int sockfd, char *responseCode)
{	
	usleep(100*1000);
	char input[500];
	
	int i = read(sockfd,&input,500);

	input[i] = '\0';

	printf("%s",input);
}

void readMessage(int sockfd, char* serverAnswer){
	char answer[256];
	int i = read(sockfd,answer,256);
	answer[i] = '\0';
	strcpy(serverAnswer,answer);
}


void readToFile(const char* filename, int socketFD){
	
	chdir("./downloads/");
	FILE * f = fopen(filename, "w");
	char auxBuff[1024];
	int n;

	while ((n = read(socketFD, auxBuff, sizeof(auxBuff)))) {
		if(n < 0){
			printf("Error while reading FD\n");
			break;
		}
		fwrite(auxBuff, n, 1, f);

	}

	fclose(f);
}


void sendUserPass(int sockfd, char* user, char* password){

	char serverAnswer[256];

	//User
	char * messageU = calloc(strlen(user)+6, sizeof(char));
	sprintf(messageU, "user %s\n", user);

	write(sockfd, messageU, strlen(messageU));

	//Read answer
	readMessage(sockfd, serverAnswer);
	printf("%s",serverAnswer);
	memset(serverAnswer,0,256*sizeof(char));

	//Pass
	char * messageP = calloc(strlen(password)+6, sizeof(char));
	sprintf(messageP, "pass %s\n", password);

	write(sockfd, messageP, strlen(messageP));

	//Read answer
	readMessage(sockfd, serverAnswer);
	printf("%s",serverAnswer);
	memset(serverAnswer,0,256*sizeof(char));


	free(messageU);
	free(messageP);
}


void enterPassiveMode(int sockfd, char* serverAnswer){

	//Set passive mode
	write(sockfd, "pasv\n", 5);

	//Read answer
	readMessage(sockfd, serverAnswer);
	printf("%s",serverAnswer);
	
}


void extractInfoPassive(char * input, int * port){

	char values[64];
	memset(values, 0,64*sizeof(char));

	strncpy(values, input + 27*sizeof(char), strlen(input) - 27 - 3); //copiar os 6 bytes mandados pelo passive mode

	int valSize = strlen(values);

	int commaCounter = 0;
	int curr_index = 0;

	char portV1[4];
	char portV2[4];
	int i;
	for(i = 0; i < valSize; i++){
		char c = values[i];
		if(c == ','){
			if(commaCounter == 4){
				portV1[curr_index+1] = '\0';
			}
			else if(commaCounter == 5){
				portV2[curr_index+1] = '\0';
			}
			commaCounter++;
			curr_index = 0;
		}
		else if(commaCounter == 4){
			portV1[curr_index] = c;
			curr_index++;
		}
		else if(commaCounter == 5){
			portV2[curr_index] = c;
			curr_index++;
		}
	}
	
	int port1 = atoi(portV1);
	int port2 = atoi(portV2);
	*port = 256*port1 + port2;

}

void sendRetrAndReadResponse(int sockfdA, int sockfdB, char* path, char* filename){

	//Send retr and file path
	write(sockfdA, "retr ", 5);
	write(sockfdA, path, strlen(path));
	write(sockfdA, "\n", 1);

	char ret[512];

	usleep(100*1000);

	int bytes = read(sockfdA, ret, 512);
	ret[bytes] = '\0';

	printf("%s",ret);

	readToFile(filename, sockfdB);
	
	printf("File created successfully!\n");

}



int main(int argc, char** argv){


	char* user = (char*) calloc(MAX_STRING_SIZE, sizeof(char));
    char* password = (char*) calloc(MAX_STRING_SIZE, sizeof(char));
    char* host = (char*) calloc(MAX_STRING_SIZE, sizeof(char));
    char* url_path = (char*) calloc(MAX_STRING_SIZE, sizeof(char));

    parseInputString(argv[1], user, password, host, url_path);

    char* filename = (char*) calloc(MAX_STRING_SIZE, sizeof(char));

    parseFile(url_path, strlen(url_path), filename);

    //Testing the values -> Tested and works fine
    printf("\nUser: %s\n", user);
    printf("Password: %s\n", password);
    printf("Host: %s\n", host);
    printf("URL Path: %s\n", url_path);
    printf("File name: %s\n\n", filename);

    struct hostent* h = getip(host);

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n\n",inet_ntoa(*((struct in_addr *)h->h_addr)));

	// Codigo fornecido 

	int	sockfd;
	struct sockaddr_in server_addr;
	struct sockaddr_in server_addr_B;

	//Janela A

	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    	perror("socket()");
        exit(0);
    }

	/*connect to the server*/
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("connect()");
		exit(0);
	}

	char responseCode[3];
	readServerAws(sockfd, responseCode);

	//Send the user and password
	sendUserPass(sockfd, user, password);

	//Enter passive mode
	char array[256];
	enterPassiveMode(sockfd, array);
	int port;
	extractInfoPassive(array, &port);
	
	//Janela B

	int	sockfdB;

	/*server address handling*/
	bzero((char*)&server_addr_B,sizeof(server_addr_B));
	server_addr_B.sin_family = AF_INET;
	server_addr_B.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));	/*32 bit Internet address network byte ordered*/
	server_addr_B.sin_port = htons(port);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfdB = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    	perror("socket()");
        exit(0);
    }

	/*connect to the server*/
    if(connect(sockfdB, (struct sockaddr *)&server_addr_B, sizeof(server_addr_B)) < 0){
        perror("connect()");
		exit(0);
	}

	sendRetrAndReadResponse(sockfd, sockfdB, url_path, filename);


    // Freeing variables
    free(user);
    free(password);
    free(host);
    free(url_path);

	close(sockfd);
	close(sockfdB);

    
    return 0;
}