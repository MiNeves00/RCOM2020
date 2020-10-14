/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
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

int readSet(int fd);

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

    if ( (argc < 2) ||
  	     ((strcmp("/dev/ttyS10", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS11", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS11\n");
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

    printf("%s\n","New Termios Structure set");

    //Read Input SET
    while (STOP==FALSE) {     /* loop for input */
      if(readSet(fd) == 0)
        STOP=TRUE;
    }

    

    //Send UA back
    printf("\n%s\n", "Sending UA back...");
    char flagUA = 0b01111110; //todas as flags teem este valor, slide 10
    char addressUA = 0b00000001; //header do emissor, slide 10
    char controlUA = 0b00000111; //UA ,slide 7
    char bccUA = (addressUA^controlUA); //XOR dos bytes anteriores ao mesmo
    buf[4] = flagUA;
    buf[3] = bccUA;
    buf[2] = controlUA;
    buf[1] = addressUA;
    buf[0] = flagUA;
    for (size_t i = 0; i < 5; i++) {
      printf(" "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
    }

    res = write(fd,buf,5);
    printf("%s\n", "\nUA sent!");


    sleep(2);

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}




int readSet(int fd){
  char buf[1];
  printf("\n%s\n", "Waiting for SET...");
  int stop = 0;
  while(stop == 0){ //state machine
   int res = read(fd,buf,1);

   if(buf[0] == 0b01111110){ //flag
      int res = read(fd,buf,1);

      if(buf[0] == 0b00000011){ //address
        int res = read(fd,buf,1);

        if(buf[0] == 0b00000011){ //control
          int res = read(fd,buf,1);
         
          if(buf[0] == (0b11111111^0b00000011)){ //bcc
            int res = read(fd,buf,1);

            if(buf[0] == 0b01111110){ //final flag
              stop = 1;
            } else
                printf("Not the correct final flag\n");

          } else
              printf("Not the correct bcc\n");
        } else
            printf("Not the correct control\n");
      } else
          printf("Not the correct address\n");
   } 

  }

  printf("SET received sucessfuly!\n");
  return 0;
}
