// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

// Define tramas info
#define FLAG ((unsigned char) 0x7E)
#define A_SEND ((unsigned char) 0x03)
#define A_RESPONSE ((unsigned char) 0x01)
#define C_SET ((unsigned char) 0x03)
#define C_DISC ((unsigned char) 0x0B)
#define C_UA ((unsigned char) 0x07)

int get_bbc1(unsigned int A, unsigned int C){
    return A^C;
}

// Alarm Setup
int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    //printf("Alarm #%d\n", alarmCount);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char buf = 0;
    unsigned char UA[] = {FLAG,A_RESPONSE,C_UA,get_bbc1(A_RESPONSE,C_UA),FLAG};
    unsigned char SET[] = {FLAG,A_SEND,C_SET,get_bbc1(A_SEND,C_SET),FLAG};
    unsigned char SET_RECEIVED[5];

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    int ff = 0;
    for(int i=0;ff < 2;i++){
        int bytes = read(fd, &buf, 1);
        if(buf == FLAG){
            ff++;
        }
        else if(bytes < 1 && alarmEnabled == FALSE){
            alarm(1);
        }
        SET_RECEIVED[i] = buf;
    }

    printf("Got Set!\n");

    /*while (alarmCount < 1)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(1); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
    }*/

    char c;
    printf("Click anywhere to send UA"); scanf("%c",&c);
    int bytes = write(fd, UA, 5);
    printf("%d bytes written\n", bytes);

    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}