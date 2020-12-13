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

int parseArguments(char *arg, char *user, char *pass, char *host, char *path);
void serverConnect(int *fd, char* serverAdress, struct sockaddr_in server_addr);
void parseNameOfFile(char *path, char *nameFile);

//READ RESPONSE
void getResponseCode(int sockfd, char *responseCode);
void threeDigitNumResponse(int *index, char ch, int *state, char *responseCode);
void multipleLinesResponse(int *index, char ch, int *state, char *responseCode);
void oneLineResponse(char ch, int *state);

//COMMAND
void sendCommand(int sockfd, char cmd[], char commandContent[]);
void waitNextResponse(int sockfd, char cmd[], int sockfdClient, char* nameOfFile, char responseCode);
int analyseResponse(int sockfd, char cmd[], char commandContent[], char* nameOfFile, int sockfdClient);

int getServerDataConnectionPort(int sockfd);

int main(int argc, char** argv){


	int sockfd;
	int sockfdClient =-1;
	struct sockaddr_in server_addr;
	struct sockaddr_in server_addr_client;
	char responseCode[3];
	struct hostent *h;

	char username[STRING_MAX_LENGTH];

	memset(username, 0, STRING_MAX_LENGTH);

	char password[STRING_MAX_LENGTH];
	memset(password, 0, STRING_MAX_LENGTH);

	char host[STRING_MAX_LENGTH];
	memset(host, 0, STRING_MAX_LENGTH);

	char path[STRING_MAX_LENGTH];
	memset(path, 0, STRING_MAX_LENGTH);
	if(parseArguments(argv[1], username, password, host, path))
		exit(1);

	char filename[STRING_MAX_LENGTH];
	parseNameOfFile(path, filename);

	//gets ip address depending on host
	if ((h = gethostbyname(host)) == NULL)
	{
		herror("gethostbyname");
		exit(1);
	}
    char* ipAdd = inet_ntoa(*((struct in_addr *)h->h_addr));
	
	printf("Finished Parsing of Arguments\n");
	printf(" -> Username : %s\n", username);
	printf(" -> Password : %s\n", password);
	printf(" -> Host : %s\n", host);
	printf(" -> Path :%s\n", path);
	printf(" -> Filename : %s\n", filename);
    printf(" -> IP Address : %s\n\n", ipAdd);

	serverConnect(&sockfd, ipAdd, server_addr);


	getResponseCode(sockfd, responseCode); 
	if (responseCode[0] == '2')
	{										 
		printf(" > Connected Succesfully\n"); 
	}	

	printf(" > Sending User\n");
	sendCommand(sockfd, "user ", username);

	if (analyseResponse(sockfd, "user ", username, filename, sockfdClient))
	{
		printf(" > Sending Password\n");
		sendCommand(sockfd, "pass ", password);
		analyseResponse(sockfd, "pass ", password, filename, sockfdClient);
	}

	write(sockfd, "pasv\n", 5);
	int serverPort = getServerDataConnectionPort(sockfd);

	close(sockfd);
	exit(0);
}


void parseNameOfFile(char *path, char *nameFile){

	int indexNameFile = 0;
	memset(nameFile, 0, STRING_MAX_LENGTH);
	for(int indexPath = 0; indexPath< strlen(path); indexPath++){

		if(path[indexPath]!='/'){
			nameFile[indexNameFile] = path[indexPath];
			indexNameFile++;
		}
		else{
			indexNameFile = 0;
			memset(nameFile, 0, STRING_MAX_LENGTH);
		}
	}
}

// ftp://rcom:rcom@netlab1.fe.up.pt/files/pic1.jpg
int parseArguments(char *arg, char *user, char *pass, char *host, char *path)
{
	char initial[] = "ftp://";
	int index = 0;
	int pos = 0;
	int state = 0;
	int argLength = strlen(arg);
	while (pos < argLength)
	{
		switch (state)
		{
		case 0: //ftp://
			if (pos == 5 && arg[pos] == initial[pos])
				state = 1;
			else if (arg[pos] == initial[pos] && pos < 5)
			{
				break;
			} else{
				printf(" > Error parsing ftp://");
				return 1;
			}

			break;

		case 1: //username
			if (arg[pos] != ':')
			{
				user[index] = arg[pos];
				index++;

			}
			else
			{
				state = 2;
				index = 0;
			}
			break;
		case 2:
			if (arg[pos] != '@')
			{
				pass[index] = arg[pos];
				index++;

			}
			else
			{
				state = 3;
				index = 0;
			}
			break;
		case 3:
			if (arg[pos] != '/')
			{
				host[index] = arg[pos];
				index++;

			}
			else
			{
				state = 4;
				index = 0;
			}
			break;
		case 4:
			path[index] = arg[pos];
			index++;
			break;

		default:
			break;
		}

		pos++;
	}
	return 0;
}

void serverConnect(int *fd, char* serverAdress, struct sockaddr_in server_addr){

	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(serverAdress);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((*fd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	exit(0);
    	}
	/*connect to the server*/
    if(connect(*fd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(0);
	}
}

void threeDigitNumResponse(int *index, char ch, int *state, char *responseCode){
			if (ch != ' ')
			{
				if (ch == '-')
					{
						*state = 1;
						*index=0;
					}
					else
					{
						if (isdigit(ch))
						{
							responseCode[*index] = ch;
							*index = *index + 1;
						}
					}
			}
			else
			{
				if (*index != 3)
				{
					printf(" > Error could not get response code\n");
					return;
				}
				*index = 0;
				*state = 2;

				
			}
}

void multipleLinesResponse(int *index, char ch, int *state, char *responseCode){
			if (ch != responseCode[*index])
			{
				if (*index == 3 && ch == ' ')
					*state = 1;
				
				else 
				{
				  if(*index==3 && ch=='-')
					*index=0;
				}

				
			}
			else
				*index = *index + 1;
}

void oneLineResponse(char ch, int *state){
			if (ch == '\n')
			{
				*state = 3;
			}
}

void getResponseCode(int sockfd, char *responseCode)
{
	int state = 0;
	int index = 0;
	char ch;

	while (state != 3)
	{	
		read(sockfd, &ch, 1);
		printf("%c", ch);
		switch (state)
		{
		case 0:
			threeDigitNumResponse(&index,ch,&state,responseCode);
			break;

		
		case 1:
			multipleLinesResponse(&index,ch,&state,responseCode);
			break;

		case 2:
			oneLineResponse(ch,&state);
			break;
		
		default:
			break;
		}
	}
}

void sendCommand(int sockfd, char cmd[], char commandContent[]){
	write(sockfd, cmd, strlen(cmd));
	write(sockfd, commandContent, strlen(commandContent));
	write(sockfd, "\n", 1);
}

void waitNextResponse(int sockfd, char cmd[], int sockfdClient, char* nameOfFile, char responseCode){
	if(strcmp(cmd, "retr ")==0){

			FILE *file = fopen((char *)nameOfFile, "wb+");

			char bufferSock[SOCK_BUFFER_LENGTH];
			int info;
			while ((info = read(sockfdClient, bufferSock, SOCK_BUFFER_LENGTH))>0) {
				info = fwrite(bufferSock, info, 1, file);
			}

			fclose(file);

			printf(" > File Downloaded!\n");
		return;
	}
	getResponseCode(sockfd, responseCode);
	return;
}

int analyseResponse(int sockfd, char cmd[], char commandContent[], char* nameOfFile, int sockfdClient)
{
	char responseCode[3];
	int state = 0;
	while (1)
	{
		getResponseCode(sockfd, responseCode);
		state = responseCode[0] - '0';

		switch (state)
		{
		case 1:
			waitNextResponse(sockfd, cmd, sockfdClient, nameOfFile, responseCode);
			break;
		//successfull
		case 2:
			return 0;
		//waiting for more info
		case 3:
			return 1;
		//try again
		case 4:
			sendCommand(sockfd, cmd, commandContent);
			break;
		default:
			printf(" > Error in command!\n");
			close(sockfd);
			exit(-1);
			break;
		}
	}
}

int getServerDataConnectionPort(int sockfd){ //Tal como demonstrado na experiencia ftp

	char byte1[4];
	memset(byte1, 0, 4);
	char byte2[4];
	memset(byte2, 0, 4);
	int state = 0;
	int pos = 0;
	int commaNum = 0;

	char ch;
	int loop = 1;

	while(loop){

		read(sockfd, &ch, 1);
		printf("%c", ch);

		switch (state){
			case 0:
				if (ch != ' ')
				{
					pos++;
					break;
				}

				if (pos != 3)
				{
					printf(" > Error in response code\n");
					return -1;
				}
				pos = 0;
				state = 1;
				break;

			case 1:
				if(commaNum == 4){
					state = 2;
					break;
				}
				if (ch == ',')
				{
					commaNum++;
				}
				break;

			case 2:
				if (ch != ',')
				{
					byte1[pos] = ch;
					pos++;
					break;
				}
				pos = 0;
				commaNum++;
				state = 3;
				break;

			case 3:
				if (ch == ')')
				{
					loop = 0;
					break;
				}
					byte2[pos] = ch;
					pos++;
			
				break;

		}



	}

	int actualByte1 = atoi(byte1);
	int actualByte2 = atoi(byte2);
	return (actualByte1 * 256 + actualByte2);


}

