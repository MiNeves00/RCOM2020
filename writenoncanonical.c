/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;


int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
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

    char flag = 0b01111110; //todas as flags teem este valor, slide 10
    char address = 0b00000011; //header do emissor, slide 10
    char control = 0b00000011; //SET ,slide 7
    char bcc = flag^address^control; //XOR dos bytes anteriores ao mesmo
    buf[0] = flag;
    buf[1] = bcc;
    buf[2] = control;
    buf[3] = address;
    buf[4] = flag;

    printf("New termios structure set\n");

    res = write(fd,buf,255);
    printf("Frame SET: %s , size: %d\n", buf,res);

  printf("\n");




   //Leitura da mensagem do receptor UA
   while (STOP==FALSE) {     /* loop for input */

     res = read(fd,buf,255);   /* returns after 5 chars have been input */
     printf("\n\nFrame UA Received: %s , size: %d\n", buf,res);
     char flag2UA = buf[0];
     char bccUA = buf[1];
     char controlUA = buf[2];
     char addressUA = buf[3];
     char flagUA = buf[4];
     if(flagUA != flag2UA || flagUA^addressUA^controlUA != bccUA){
       printf("Error, bytes received don't match with BCC!");
     } else {
       if(controlUA == 0b00000111){
         printf("UA received!");
         STOP = TRUE;
       }
       else
         printf("Control camp is different than expected\n");

     }

   }



    sleep(2);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }




    close(fd);
    return 0;
}
