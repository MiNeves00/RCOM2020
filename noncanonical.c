/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)     \
  (byte & 0x80 ? '1' : '0'),     \
      (byte & 0x40 ? '1' : '0'), \
      (byte & 0x20 ? '1' : '0'), \
      (byte & 0x10 ? '1' : '0'), \
      (byte & 0x08 ? '1' : '0'), \
      (byte & 0x04 ? '1' : '0'), \
      (byte & 0x02 ? '1' : '0'), \
      (byte & 0x01 ? '1' : '0')

volatile int STOP = FALSE;

int setProtocol(int fd);
int readSet(int fd);
int sendUA(int fd);

int disconnectProtocol(int fd);
int readDisc(int fd);
int sendDiscWithAlarm(int fd);
int readUA(int fd);

int nAlarm = 0;

int main(int argc, char **argv)
{
  int fd, c, res;
  struct termios oldtio, newtio;
  char buf[255];

  if ((argc < 2) ||
      ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS11\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(fd, &oldtio) == -1)
  { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 5;  /* blocking read until 5 chars received */

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  printf("%s\n", "New Termios Structure set");

  setProtocol(fd);

  disconnectProtocol(fd);

  //sleep(2);

  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}

///////SET

int setProtocol(int fd)
{
  //Read Input SET
  while (STOP == FALSE)
  { /* loop for input */
    if (readSet(fd) == 0)
      STOP = TRUE;
  }

  sendUA(fd);

  return 0;
}

int sendUA(int fd)
{
  //Send UA back
  char buf[255];
  int res;
  printf("\n%s\n", "Sending UA back...");
  char flagUA = 0b01111110;             //todas as flags teem este valor, slide 10
  char addressUA = 0b00000001;          //header do emissor, slide 10
  char controlUA = 0b00000111;          //UA ,slide 7
  char bccUA = (addressUA ^ controlUA); //XOR dos bytes anteriores ao mesmo
  buf[4] = flagUA;
  buf[3] = bccUA;
  buf[2] = controlUA;
  buf[1] = addressUA;
  buf[0] = flagUA;
  for (size_t i = 0; i < 5; i++)
  {
    printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
  }

  res = write(fd, buf, 5);
  printf("%s\n", "\nUA sent!");
  return 0;
}

int readSet(int fd)
{
  char buf[1];
  printf("\n%s\n", "Waiting for SET...");
  int stop = 0;
  while (stop == 0)
  { //state machine
    int res = read(fd, buf, 1);

    if (buf[0] == 0b01111110)
    { //flag
      int res = read(fd, buf, 1);

      if (buf[0] == 0b00000011)
      { //address
        int res = read(fd, buf, 1);

        if (buf[0] == 0b00000011)
        { //control
          int res = read(fd, buf, 1);

          if (buf[0] == (0b00000011 ^ 0b00000011))
          { //bcc
            int res = read(fd, buf, 1);

            if (buf[0] == 0b01111110)
            { //final flag
              stop = 1;
            }
            else
              printf("Not the correct final flag\n");
          }
          else
            printf("Not the correct bcc\n");
        }
        else
          printf("Not the correct control\n");
      }
      else
        printf("Not the correct address\n");
    }
  }

  printf("SET received sucessfuly!\n");
  return 0;
}

///////DISC

int disconnectProtocol(int fd)
{
  //Read Disc
  STOP = FALSE;
  while (STOP == FALSE)
  { /* loop for input */
    if (readDisc(fd) == 0)
      STOP = TRUE;
  }

  //Send Disc
  nAlarm = 0;
  STOP = FALSE;
  printf("\nDisconecting...\n");
  (void)signal(SIGALRM, sendDiscWithAlarm);

  sendDiscWithAlarm(fd);

  //Read UA
  while (STOP == FALSE && nAlarm < 3)
  { /* loop for input */
    if (readUA(fd) == 0)
      STOP = TRUE;
  }
  printf("\nDisconnected Sucessfully!\n");

  return 0;
}

int readDisc(int fd)
{
  char buf[1];
  printf("\n%s\n", "Waiting for DISC...");
  int stop = 0;
  while (stop == 0)
  { //state machine
    int res = read(fd, buf, 1);

    if (buf[0] == 0b01111110)
    { //flag
      int res = read(fd, buf, 1);

      if (buf[0] == 0b00000011)
      { //address
        int res = read(fd, buf, 1);

        if (buf[0] == 0b00001011)
        { //control
          int res = read(fd, buf, 1);

          if (buf[0] == (0b00000011 ^ 0b00001011))
          { //bcc
            int res = read(fd, buf, 1);

            if (buf[0] == 0b01111110)
            { //final flag
              stop = 1;
            }
            else
              printf("Not the correct final flag\n");
          }
          else
            printf("Not the correct bcc\n");
        }
        else
          printf("Not the correct control\n");
      }
      else
        printf("Not the correct address\n");
    }
  }

  printf("DISC received sucessfuly!\n");
  return 0;
}

int sendDiscWithAlarm(int fd)
{

  char buf[255];

  if (nAlarm < 3)
  {
    //Send Disc back
    int res;
    printf("\n%s\n", "Sending DISC back...");
    char flag = 0b01111110;         //todas as flags teem este valor, slide 10
    char address = 0b00000001;      //header do emissor, slide 10
    char control = 0b00001011;      //UA ,slide 7
    char bcc = (address ^ control); //XOR dos bytes anteriores ao mesmo
    buf[4] = flag;
    buf[3] = bcc;
    buf[2] = control;
    buf[1] = address;
    buf[0] = flag;
    for (size_t i = 0; i < 5; i++)
    {
      printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
    }

    res = write(fd, buf, 5);
    printf("%s\n", "\nDISC sent!");

    printf("\nalarme # %d\n", nAlarm + 1);
    nAlarm++;
  }
  else
  {
    printf("\nCan't connect\n");
    exit(1);
  }

  alarm(3);
}

int readUA(int fd)
{
  char buf[1];
  printf("\n%s\n", "Waiting for UA...");
  int stop = 0;
  while (stop == 0)
  { //state machine
    int res = read(fd, buf, 1);

    if (buf[0] == 0b01111110)
    { //flag
      int res = read(fd, buf, 1);

      if (buf[0] == 0b00000011)
      { //address
        int res = read(fd, buf, 1);

        if (buf[0] == 0b00000111)
        { //control
          int res = read(fd, buf, 1);

          if (buf[0] == (0b00000011 ^ 0b00000111))
          { //bcc
            int res = read(fd, buf, 1);

            if (buf[0] == 0b01111110)
            { //final flag
              stop = 1;
            }
            else
              printf("Not the correct final flag\n");
          }
          else
            printf("Not the correct bcc\n");
        }
        else
          printf("Not the correct control\n");
      }
      else
        printf("Not the correct address\n");
    }
  }

  printf("UA received sucessfuly!\n");
  return 0;
}