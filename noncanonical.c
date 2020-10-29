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

#define FLAG 0x7e
#define ESC 0x7d
#define STUFF 0x20

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

char data[255];
int dataFrameNum = 0;
int duplicate = 0;
int dataProtocol(int fd);
int readData(int fd);
int sendRR(int fd);
int sendREJ(int fd);

int disconnectProtocol(int fd);
int readDisc(int fd);
int sendDiscWithAlarm(int fd);
int readUA(int fd);



int llread(int fd, char* buffer);

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
  dataProtocol(fd);
  disconnectProtocol(fd);

  //sleep(2);

  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}

#pragma region ///////SET

int setProtocol(int fd)
{
  //Read Input SET
  STOP = FALSE;
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
  int flag = 0;
  int res = 0;
  while (stop == 0)
  { //state machine
    if(flag == 0)
      res = read(fd, buf, 1);

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
    
    flag = 0;
    if(buf[0] == 0b01111110) //if its a flag
      flag = 1;
  }

  printf("SET received sucessfuly!\n");
  return 0;
}
#pragma endregion

#pragma region ///////DATA

int dataProtocol(int fd){

  //Read Data and then answer
  STOP = FALSE;
  while (STOP == FALSE)
  { /* loop for input */
    int res = readData(fd);
    if (res == -1)       // has received Disc
      STOP = TRUE;
    else if (res == 1) // bcc2 detected errors
      sendREJ(fd);
    
    else{
      sendRR(fd);

      if(duplicate == 0) //discard data if duplicate
        llread(fd, &data);
    }
    
    
  }

  return 0;
}


int readData(int fd){ //TO DO parte do Disc

  memset(data, 0, 255);
  char tmpData[255];
  char buf[1];
  printf("\n%s\n", "Waiting for Data...");
  int stop = 0;
  int flag = 0;
  int res = 0;
  int control;
  char bcc2;
  duplicate = 0;


  while (stop == 0)
  { //state machine
    if(flag == 0)
      res = read(fd, buf, 1);

    if (buf[0] == 0b01111110)
    { //flag
      int res = read(fd, buf, 1);

      if (buf[0] == 0b00000011)
      { //address
        int res = read(fd, buf, 1);

        if(dataFrameNum == 0){           //S e N(s), slide 7
          control = 0b00000000;
          if(buf[0] == 0b01000000)
            duplicate = 1;    
        }  
        else{
          control = 0b01000000;
          if(buf[0] == 0b00000000)
            duplicate = 1;
        }
        if (buf[0] == control)
        { //control
          int res = read(fd, buf, 1);

          if (buf[0] == (0b00000011 ^ control))
          { //bcc1
            
            
            //DATA
            int receiving = 0;
            int i = 0;
            int res = read(fd, buf, 1);
            bcc2 = buf[0];
            while(receiving == 0){
              int res = read(fd, buf, 1);
              if (buf[0] == 0b01111110){ //final flag
                receiving = 1;
                stop = 1;
              } else {
                tmpData[i] = bcc2;
                bcc2 = buf[0];
                printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(tmpData[i]));
              }
              i++;
            }

            char stuffflag = STUFF ^ FLAG;
            char stuffesc = STUFF ^ ESC;

            int xor = 0, n = 0, j;
            for(j = 0; j < (i-1); j++)
            {
              if (tmpData[j] == ESC)
              {
                j++;

                if(tmpData[j] == stuffflag)
                {
                  data[n] = FLAG;
                }

                else if (tmpData[j] == stuffesc)
                {
                  data[n] = ESC;
                }

              }

              else
                data[n] = tmpData[j];
    
              xor ^= data[n];

              n++;
            }
            printf("\nData after Destuffing:\n");
            for(int w = 0; w < n; w++)
              printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(data[w]));

            if(bcc2 != xor){
              printf("BCC2 shows errors in data fields!\n");
              return 1;
            }
          }
          else
            printf("Not the correct bcc1\n");
        }
        else if(duplicate == 1){
          dataFrameNum = 1-dataFrameNum; //para pedir de novo a trama seguinte visto que esta era duplicate 
          printf("Data was a Duplicate\n");
          return 0;
        }
        else
          printf("Not the correct control\n");
      }
      else
        printf("Not the correct address\n");
    }
    
    flag = 0;
    if(buf[0] == 0b01111110) //if its a flag
      flag = 1;
  }

  printf("\n**Data received sucessfuly!**\n");
  return 0;  
}


int sendRR(int fd){
  //Send RR back
  char buf[255];
  int res;
  printf("\n%s\n", "Sending RR back...");
  char flagRR = 0b01111110;             //todas as flags teem este valor, slide 10
  char addressRR = 0b00000001;          //header do emissor, slide 10
  char controlRR;
  dataFrameNum = 1-dataFrameNum;
  if(dataFrameNum == 0)                 //RR e R = dataFrameNum ,slide 7
    controlRR = 0b00000101;          
  else
    controlRR = 0b10000101;
  char bccRR = (addressRR ^ controlRR); //XOR dos bytes anteriores ao mesmo
  buf[4] = flagRR;
  buf[3] = bccRR;
  buf[2] = controlRR;
  buf[1] = addressRR;
  buf[0] = flagRR;
  for (size_t i = 0; i < 5; i++)
  {
    printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
  }

  res = write(fd, buf, 5);
  printf("\nRR with R = %d sent!\n", dataFrameNum);
  return 0;
}


int sendREJ(int fd){
  //Send REJ back
  char buf[255];
  int res;
  printf("\n%s\n", "Sending REJ back...");
  char flagREJ = 0b01111110;             //todas as flags teem este valor, slide 10
  char addressREJ = 0b00000001;          //header do emissor, slide 10
  char controlREJ;

  if(dataFrameNum == 0)                 //REJ e R = dataFrameNum ,slide 7
    controlREJ = 0b00000001;          
  else
    controlREJ = 0b10000001;
  char bccREJ = (addressREJ ^ controlREJ); //XOR dos bytes anteriores ao mesmo
  buf[4] = flagREJ;
  buf[3] = bccREJ;
  buf[2] = controlREJ;
  buf[1] = addressREJ;
  buf[0] = flagREJ;
  for (size_t i = 0; i < 5; i++)
  {
    printf(" " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
  }

  res = write(fd, buf, 5);
  printf("\nREJ with R = %d sent!\n", dataFrameNum);
  return 0;
}


#pragma endregion


#pragma region ///////DISC

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
  int flag = 0;
  int res = 0;
  while (stop == 0)
  { //state machine
    if(flag == 0)
      res = read(fd, buf, 1);

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
    
    flag = 0;
    if(buf[0] == 0b01111110) //if its a flag
      flag = 1;
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
  int flag = 0;
  int res = 0;
  while (stop == 0)
  { //state machine
    if(flag == 0)
      res = read(fd, buf, 1);

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
    flag = 0;
    if(buf[0] == 0b01111110) //if its a flag
      flag = 1;
  }

  printf("UA received sucessfuly!\n");
  return 0;
}
#pragma endregion



#pragma region //////APP

int llread(int fd, char* buffer){
  //TO DO ,guarda a data senao ela desaparece
  printf("SAVED DATA\n");
}

#pragma endregion

