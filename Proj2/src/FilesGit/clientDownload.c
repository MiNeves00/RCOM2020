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
#define STRING_MAX_LENGTH 50
#define SOCK_BUFFER_LENGTH 1000

void readResponse(int sockfd, char *responseCode);
struct hostent *getip(char host[]);
void createFile(int fd, char* filename);
void parseArguments(char *argument, char *username, char *password, char *host, char *path);
int sendCommandInterpretResponse(int sockfd, char cmd[], char commandContent[], char* filename, int sockfdClient);
int getServerPortFromResponse(int sockfd);
void parseFilename(char *path, char *filename);


int main(int argc, char **argv)
{
	int sockfd;
	int sockfdClient =-1;
	struct sockaddr_in server_addr;
	struct sockaddr_in server_addr_client;

	struct hostent *h;

	char username[STRING_MAX_LENGTH];
	char responseCode[3];
	memset(username, 0, STRING_MAX_LENGTH);

	char password[STRING_MAX_LENGTH];
	memset(password, 0, STRING_MAX_LENGTH);

	char host[STRING_MAX_LENGTH];
	memset(host, 0, STRING_MAX_LENGTH);

	char path[STRING_MAX_LENGTH];
	memset(path, 0, STRING_MAX_LENGTH);
	parseArguments(argv[1], username, password, host, path);

	char filename[STRING_MAX_LENGTH];
	parseFilename(path, filename);

	printf(" - Username: %s\n", username);
	printf(" - Password: %s\n", password);
	printf(" - Host: %s\n", host);
	printf(" - Path :%s\n", path);
	printf(" - Filename: %s\n", filename);

	h = getip(host);
    char* ipAdd = inet_ntoa(*((struct in_addr *)h->h_addr));
	printf(" - IP Address : %s\n\n", ipAdd);

	/*server address handling*/
	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr))); /*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);											/*server TCP port must be network byte ordered */

	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket()");
		exit(0);
	}
	/*connect to the server*/
	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("connect()");
		exit(0);
	}

	readResponse(sockfd, responseCode); 
	if (responseCode[0] == '2')
	{										 
		printf(" > Connection Estabilished\n"); 
	}										 

	printf(" > Sending Username\n");
	int res = sendCommandInterpretResponse(sockfd, "user ", username, filename, sockfdClient);
	if (res == 1)
	{
		printf(" > Sending Password\n");
		res = sendCommandInterpretResponse(sockfd, "pass ", password, filename, sockfdClient);
	}

	write(sockfd, "pasv\n", 5);
	int serverPort = getServerPortFromResponse(sockfd);

	/*server address handling*/
	bzero((char *)&server_addr_client, sizeof(server_addr_client));
	server_addr_client.sin_family = AF_INET;
	server_addr_client.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr))); /*32 bit Internet address network byte ordered*/
	server_addr_client.sin_port = htons(serverPort);										   /*server TCP port must be network byte ordered */

	/*open an TCP socket*/
	if ((sockfdClient = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket()");
		exit(0);
	}
	/*connect to the server*/
	if (connect(sockfdClient, (struct sockaddr *)&server_addr_client, sizeof(server_addr_client)) < 0)
	{
		perror("connect()");
		exit(0);
	}
	printf("\n > Sending Retr\n");
	int resRetr =sendCommandInterpretResponse(sockfd, "retr ", path, filename, sockfdClient);

	if(resRetr==0){
		close(sockfdClient);
		close(sockfd);
		exit(0);
	}
	else printf(" > ERROR in RETR response\n");

	close(sockfdClient);
	close(sockfd);
	exit(1);

	
}

// ./download ftp://anonymous:1@speedtest.tele2.net/1KB.zip
void parseArguments(char *argument, char *username, char *password, char *host, char *path)
{
	char start[] = "ftp://";
	int index = 0;
	int i = 0;
	int state = 0;
	int length = strlen(argument);
	while (i < length)
	{
		switch (state)
		{
		case 0: //reads the ftp://
			if (argument[i] == start[i] && i < 5)
			{
				break;
			}
			if (i == 5 && argument[i] == start[i])
				state = 1;
			else
				printf(" > Error parsing ftp://");
			break;
		case 1: //reads the username
			if (argument[i] == ':')
			{
				state = 2;
				index = 0;
			}
			else
			{
				username[index] = argument[i];
				index++;
			}
			break;
		case 2:
			if (argument[i] == '@')
			{
				state = 3;
				index = 0;
			}
			else
			{
				password[index] = argument[i];
				index++;
			}
			break;
		case 3:
			if (argument[i] == '/')
			{
				state = 4;
				index = 0;
			}
			else
			{
				host[index] = argument[i];
				index++;
			}
			break;
		case 4:
			path[index] = argument[i];
			index++;
			break;
		}
		i++;
	}
}

void parseFilename(char *path, char *filename){
	int indexPath = 0;
	int indexFilename = 0;
	memset(filename, 0, STRING_MAX_LENGTH);

	for(;indexPath< strlen(path); indexPath++){

		if(path[indexPath]=='/'){
			indexFilename = 0;
			memset(filename, 0, STRING_MAX_LENGTH);
			
		}
		else{
			filename[indexFilename] = path[indexPath];
			indexFilename++;
		}
	}
}

//gets ip address according to the host's name
struct hostent *getip(char host[])
{
	struct hostent *h;

	if ((h = gethostbyname(host)) == NULL)
	{
		herror("gethostbyname");
		exit(1);
	}

	return h;
}

//reads response code from the server
void readResponse(int sockfd, char *responseCode)
{
	int state = 0;
	int index = 0;
	char c;

	while (state != 3)
	{	
		read(sockfd, &c, 1);
		printf("%c", c);
		switch (state)
		{
		//waits for 3 digit number followed by ' ' or '-'
		case 0:
			if (c == ' ')
			{
				if (index != 3)
				{
					printf(" > Error receiving response code\n");
					return;
				}
				index = 0;
				state = 1;
			}
			else
			{
				if (c == '-')
				{
					state = 2;
					index=0;
				}
				else
				{
					if (isdigit(c))
					{
						responseCode[index] = c;
						index++;
					}
				}
			}
			break;
		//reads until the end of the line
		case 1:
			if (c == '\n')
			{
				state = 3;
			}
			break;
		//waits for response code in multiple line responses
		case 2:
			if (c == responseCode[index])
			{
				index++;
			}
			else
			{
				if (index == 3 && c == ' ')
				{
					state = 1;
				}
				else 
				{
				  if(index==3 && c=='-'){
					index=0;
					
				}
				}
				
			}
			break;
		}
	}
}

//reads the server port when pasv is sent
int getServerPortFromResponse(int sockfd)
{
	int state = 0;
	int pos = 0;
	char byte1[4];
	memset(byte1, 0, 4);
	char byte2[4];
	memset(byte2, 0, 4);

	char ch;

	while (state != 7)
	{
		read(sockfd, &ch, 1);
		printf("%c", ch);
		switch (state)
		{
		//waits for 3 digit number followed by ' '
		case 0:
			if (ch == ' ')
			{
				if (pos != 3)
				{
					printf(" > Error receiving response code\n");
					return -1;
				}
				pos = 0;
				state = 1;
			}
			else
			{
				pos++;
			}
			break;
		case 5:
			if (ch == ',')
			{
				pos = 0;
				state++;
			}
			else
			{
				byte1[pos] = ch;
				pos++;
			}
			break;
		case 6:
			if (ch == ')')
			{
				state++;
			}
			else
			{
				byte2[pos] = ch;
				pos++;
			}
			break;
		//reads until the first comma
		default:
			if (ch == ',')
			{
				state++;
			}
			break;
		}
	}

	int firstByteInt = atoi(byte1);
	int secondByteInt = atoi(byte2);
	return (firstByteInt * 256 + secondByteInt);
}

//sends a command, reads the response from the server and interprets it
int sendCommandInterpretResponse(int sockfd, char cmd[], char commandContent[], char* filename, int sockfdClient)
{
	char responseCode[3];
	int action = 0;
	//sends the command
	write(sockfd, cmd, strlen(cmd));
	write(sockfd, commandContent, strlen(commandContent));
	write(sockfd, "\n", 1);

	while (1)
	{
		//reads the response
		readResponse(sockfd, responseCode);
		action = responseCode[0] - '0';

		switch (action)
		{
		//waits for another response
		case 1:
			if(strcmp(cmd, "retr ")==0){
				createFile(sockfdClient, filename);
				break;
			}
			readResponse(sockfd, responseCode);
			break;
		//command accepted, we can send another command
		case 2:
			return 0;
		//needs additional information
		case 3:
			return 1;
		//try again
		case 4:
			write(sockfd, cmd, strlen(cmd));
			write(sockfd, commandContent, strlen(commandContent));
			write(sockfd, "\r\n", 2);
			break;
		case 5:
			printf(" > Command wasn\'t accepted. Goodbye!\n");
			close(sockfd);
			exit(-1);
		}
	}
}

void createFile(int fd, char* filename)
{
	FILE *file = fopen((char *)filename, "wb+");

	char bufSocket[SOCK_BUFFER_LENGTH];
 	int bytes;
 	while ((bytes = read(fd, bufSocket, SOCK_BUFFER_LENGTH))>0) {
    	bytes = fwrite(bufSocket, bytes, 1, file);
    }

  	fclose(file);

	printf(" > Finished downloading file\n");
}
