/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')


volatile int STOP=FALSE;

int nAlarm=0;
int fd;

void handleAlarm()                   // atende alarme
{
  char buf[255];

  if (nAlarm < 3)
  {
    char flag = 0b01111110; //todas as flags teem este valor, slide 10
    char address = 0b00000011; //header do emissor, slide 10
    char control = 0b00000011; //SET ,slide 7
    char bcc = flag^address^control; //XOR dos bytes anteriores ao mesmo
    buf[4] = flag;
    buf[3] = bcc;
    buf[2] = control;
    buf[1] = address;
    buf[0] = flag;


    printf("%s\n", "Sending SET...");
    for (size_t i = 0; i < 5; i++) {
      printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
    }

    write(fd,buf,40);

    printf("\nalarme # %d\n", nAlarm+1);
	  nAlarm++;
  }
	else
  {
    printf("\nCan't connect\n");
    exit(1);
  }

  alarm(3);
}

int readUA(int fd);

int main(int argc, char** argv)
{
    int c, res;
    struct termios oldtio,newtio;
    
    int i, sum = 0, speed = 0;

    if ( (argc < 2) ||
  	     ((strcmp("/dev/ttyS10", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS11", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS10\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */



  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    //Establish Logic connection
    printf("New termios structure set\n");

    (void) signal(SIGALRM, handleAlarm);

    handleAlarm();

    //Leitura da mensagem do receptor UA
    while (STOP==FALSE && nAlarm < 3) {     /* loop for input */
      if(readUA(fd) == 0)
        STOP=TRUE;
    }




    sleep(2);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }




    close(fd);
    return 0;
}


int readUA(int fd){
  char buf[255];
  printf("\n%s\n", "Waiting for UA...");
  int res = read(fd,buf,40);   /* returns after 5 chars have been input */

  for (size_t i = 0; i < 5; i++) {
    printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
  }
   printf("\n%s\n", "cheking for errors...");

   char flag2UA = buf[4];
   char bccUA = buf[3];
   char controlUA = buf[2];
   char addressUA = buf[1];
   char flagUA = buf[0];
  // printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(flagUA));
  // printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(addressUA));
  // printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(controlUA));
  // printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(bccUA));
  // printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(flag2UA));

  if(flagUA != flag2UA || (flagUA^addressUA^controlUA) != bccUA){
     printf("Error, bytes received don't match with BCC!\n");
     return 1;
   }
   else {
     if(controlUA == 0b00000111){
       printf("UA received!\n");
      return 0;
     }
    else
      printf("Control camp is different than expected!\n");
      return 1;
   }
}
