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

volatile int STOP=FALSE;

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

    printf("New termios structure set\n");

    //Read Input SET
    while (STOP==FALSE) {     /* loop for input */

      res = read(fd,buf,255);   /* returns after 5 chars have been input */
      printf("Frame SET Received: %d , size: %d\n", 6,res);
      printf("Buf valor %d", 7);
      char flag2 = buf[0];
      char bcc = buf[1];
      char control = buf[2];
      char address = buf[3];
      char flag = buf[4];
      printf("now!");
      if(flag != flag2 || flag^address^control != bcc){
        printf("Error, bytes received don't match with BCC!");
      }
      else {
        printf("HERE!");
        //if(control == 0b00000011){
          printf("SET received!");
          STOP = TRUE;
      //  }
    //    else
      //    printf("Control camp is different than expected\n");
      }

    }

    //Send UA back
    char flagUA = 0b01111110; //todas as flags teem este valor, slide 10
    char addressUA = 0b00000001; //header do emissor, slide 10
    char controlUA = 0b00000111; //SET ,slide 7
    char bccUA = flagUA^addressUA^controlUA; //XOR dos bytes anteriores ao mesmo
    buf[0] = flagUA;
    buf[1] = bccUA;
    buf[2] = controlUA;
    buf[3] = addressUA;
    buf[4] = flagUA;

    res = write(fd,buf,255);
    printf("num of char: %d\n", strlen(buf));
    printf("%d bytes written\n", res);




    sleep(2);

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
