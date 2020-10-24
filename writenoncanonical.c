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

int nAlarm = 0;
int fd;

int setConnection();
void sendSetWithAlarm();
int readUA();

int disconnect();
int readDisc();
int sendDisconnectWithAlarm();
int sendUA();

int main(int argc, char **argv)
{
  int c, res;
  struct termios oldtio, newtio;

  int i, sum = 0, speed = 0;

  if ((argc < 2) ||
      ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS10\n");
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



  //Establish Logic connection
  printf("New termios structure set\n");
  setConnection();
  printf("\nConnection SET!\n");

  char data[255] = {'s','d','b','k','9','4','g'};
      int size = (sizeof(data)/sizeof(data[0])) + 6; ///DEVIA IMPRIMIR 7+8=13
    printf("\n%s %d", "Size: ", size);
  transferData(data);

  disconnect();





  printf("Closing\n");
  // sleep(1);
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}





#pragma region ////////Connect

int setConnection()
{
  nAlarm = 0;
  STOP = FALSE;
  printf("\nConnecting...\n");
  (void)signal(SIGALRM, sendSetWithAlarm);

  sendSetWithAlarm();

  //Leitura da mensagem do receptor UA
  while (STOP == FALSE && nAlarm < 3)
  { /* loop for input */
    if (readUA() == 0)
      STOP = TRUE;
  }

  return 0;
}

int readUA()
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

      if (buf[0] == 0b00000001)
      { //address
        int res = read(fd, buf, 1);

        if (buf[0] == 0b00000111)
        { //control
          int res = read(fd, buf, 1);

          if (buf[0] == (0b00000001 ^ 0b00000111))
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

void sendSetWithAlarm() // atende alarme
{
  char buf[255];

  if (nAlarm < 3)
  {
    char flag = 0b01111110;         //todas as flags teem este valor, slide 10
    char address = 0b00000011;      //header do emissor, slide 10
    char control = 0b00000011;      //SET ,slide 7
    char bcc = (address ^ control); //XOR dos bytes anteriores ao mesmo
    buf[4] = flag;
    buf[3] = bcc;
    buf[2] = control;
    buf[1] = address;
    buf[0] = flag;

    printf("%s\n", "Sending SET...");
    for (size_t i = 0; i < 5; i++)
    {
      printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
    }

    write(fd, buf, 5);

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
#pragma endregion



////////Transfer Data
int transferData(char data[255]){
  nAlarm = 0;
  STOP = FALSE;
  printf("\nTransfering Data...\n");
  (void)signal(SIGALRM, sendDataWithAlarm(data));

  sendDataWithAlarm(data);

  //Leitura da mensagem do receptor UA
  while (STOP == FALSE && nAlarm < 3)
  { /* loop for input */
    //TO DO
  }

  printf("\nData Transfered with success!");
  return 0;
  
}


int sendDataWithAlarm(char data[255]){  //TO DO
  char buf[255];

  if (nAlarm < 3)
  {
    char flag = 0b01111110;         //todas as flags teem este valor, slide 10
    char address = 0b00000011;      //header do emissor, slide 10
    char control = 0b00000011;      //SET ,slide 7
    char bcc1 = (address ^ control); //XOR dos bytes anteriores ao mesmo

    buf[3] = bcc1;
    buf[2] = control;
    buf[1] = address;
    buf[0] = flag;
    int i;
    char bcc2; //XOR dos bytes de Data
    for(i = 0; i < sizeof(data)/sizeof(data[0]); i++){
      buf[4+i] = data[i];
      bcc2 ^= data[i];
    }
    buf[4+i] = bcc2;
    buf[4+i+1] = flag;


    printf("%s\n", "Sending Data...");
    int size = (sizeof(data)/sizeof(data[0])) + 6; //ESTA A MANDAR SEMPRE 8 BYTES DE DADOS INDEPENDENTEMENTE
    printf("\n%s %d", "Size: ", size);
    for (size_t i = 0; i < size; i++)
    {
      printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
    }

    write(fd, buf, size);

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


#pragma region ////////Disconnect


int disconnect()
{
  //Send Disc
  nAlarm = 0;
  STOP = FALSE;
  printf("\nDisconnecting...\n");
  (void)signal(SIGALRM, sendDisconnectWithAlarm);

  sendDisconnectWithAlarm();

  //Leitura da mensagem do receptor Disc
  while (STOP == FALSE && nAlarm < 3)
  { /* loop for input */
    if (readDisc() == 0)
      STOP = TRUE;
  }

  //Send UA
  sendUA();
  printf("\nDisconnected sucessfully!\n");

  return 0;
}

int sendDisconnectWithAlarm()
{
  char buf[255];

  if (nAlarm < 3)
  {
    char flag = 0b01111110;         //todas as flags teem este valor, slide 10
    char address = 0b00000011;      //header do emissor, slide 10
    char control = 0b00001011;      //DISC ,slide 7
    char bcc = (address ^ control); //XOR dos bytes anteriores ao mesmo
    buf[4] = flag;
    buf[3] = bcc;
    buf[2] = control;
    buf[1] = address;
    buf[0] = flag;

    printf("%s\n", "Sending DISC...");
    for (size_t i = 0; i < 5; i++)
    {
      printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
    }

    write(fd, buf, 5);

    printf("\nalarme # %d\n", nAlarm + 1);
    nAlarm++;
  }
  else
  {
    printf("\nCan't disconnect\n");
    exit(1);
  }

  alarm(3);
}

int readDisc()
{
  char buf[1];
  printf("\n%s\n", "Waiting for Disc...");
  int stop = 0;
  while (stop == 0)
  { //state machine
    int res = read(fd, buf, 1);

    if (buf[0] == 0b01111110)
    { //flag
      int res = read(fd, buf, 1);

      if (buf[0] == 0b00000001)
      { //address
        int res = read(fd, buf, 1);

        if (buf[0] == 0b00001011)
        { //control
          int res = read(fd, buf, 1);

          if (buf[0] == (0b00000001 ^ 0b00001011))
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

  printf("Disc received sucessfuly!\n");
  return 0;
}

int sendUA()
{
  //Send UA back
  char buf[255];
  int res;
  printf("\n%s\n", "Sending UA back...");
  char flagUA = 0b01111110;             //todas as flags teem este valor, slide 10
  char addressUA = 0b00000011;          //header do emissor, slide 10
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
#pragma endregion