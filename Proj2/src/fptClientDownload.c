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
#define SOCK_BUFFER_SIZE 1000

int main(int argc, char** argv){


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
	parseArgument(argv[1], username, password, host, path);

	char filename[STRING_MAX_LENGTH];
	parseFilename(path, filename);

    h = getip(host);
    char* ipAdd = inet_ntoa(*((struct in_addr *)h->h_addr));
	

	printf(" -> Username : %s\n", username);
	printf(" -> Password : %s\n", password);
	printf(" -> Host : %s\n", host);
	printf(" -> Path :%s\n", path);
	printf(" -> Filename : %s\n", filename);
    printf(" -> IP Address : %s\n\n", ipAdd);


	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ipAdd);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	exit(0);
    	}
	/*connect to the server*/
    	if(connect(sockfd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(0);
	}

	close(sockfd);
	exit(0);
}


