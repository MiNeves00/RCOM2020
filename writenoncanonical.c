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

    uint8_t flag = 0b01111110; //todas as flags teem este valor, slide 10
    uint8_t address = 0x03; //header do emissor
    uint8_t control = 0b00000011; //SET ,slide 7
  //  uint8_t bcc = ?????????;


    printf("New termios structure set\n");



//Adicionar caracteres

    printf("Please enter a string:\n");
    if (gets(buf) != NULL)
      printf("The string is: %s\n", buf);
    else if (ferror(stdin))
      perror("Error");

    buf[strlen(buf)] = '\0';


    res = write(fd,buf,255);
    printf("num of char: %d\n", strlen(buf));
    printf("%d bytes written\n", res);


   //Leitura da mensagem do receptor
    char buffer[255];
    printf("Waiting for confirmation of reception\n");
    while (STOP==FALSE) {     /* loop for input */

      res = read(fd,buffer,255);   /* returns after 5 chars have been input */
      buffer[res]=0;               /* so we can printf... */
      if(buffer[res-1]=='\0'){
         printf("Confirmed!\n");
         printf(":%s:%d\n", buffer, res);
        STOP=TRUE;
        break;
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
