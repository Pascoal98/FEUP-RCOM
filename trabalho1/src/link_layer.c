// Link layer protocol implementation

#include "helper.h"

// includes
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

int fd;
Trama trama;
struct termios oldtio;
struct termios newtio;
LinkLayer linker;
unsigned char buffer[128];

// Alarm Setup
int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
}

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    // TODO
    linker = connectionParameters;

    int fd = open(linker.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(linker.serialPort);
        exit(-1);
    }
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = linker.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

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

    (void)signal(SIGALRM, alarmHandler);

    if (linker.role == LlTx)
    {
        int flag = 0;
        trama.state = S_START;
        alarmCount = 0;
        while (alarmCount < linker.nRetransmissions && !flag)
        {
            alarm(linker.timeout);
            alarmEnabled = TRUE;
            if (alarmCount > 0)
            {
                alarmCount++;
                printf("Alarm Count = %d\n", alarmCount);
            }
            int size = createSUFrame(buffer, A_SEND, C_SET);
            write(fd, buffer, size);

            while (alarmEnabled && !flag)
            {
                int bytes = read(fd, buffer, 128);

                if (bytes < 0)
                    return -1;

                for (int i = 0; i < bytes && !flag; i++)
                {
                    state_machine_handler(&trama, buffer[i]);

                    if (trama.state == S_END && trama.adr == A_SEND && trama.ctrl == C_UA)
                        flag = 1;
                }
            }
        }

        if (flag)
            printf("UA sent successfully!\n");
        else
            return -1;
        return 1;
    }
    else // receiver
    {
        int flag = 0;
        trama.state = S_START;
        while (!flag)
        {
            int bytes = read(fd, buffer, 128);

            if (bytes < 0)
                return -1;

            for (int i = 0; i < bytes && !flag; i++)
            {
                state_machine_handler(&trama, buffer[i]);
                if (trama.state == S_END && trama.adr == A_SEND && trama.ctrl == C_DISC)
                {
                    printf("Disconnected!\n");
                    return -1;
                }
                if (trama.state == S_END && trama.adr == A_SEND && trama.ctrl == C_SET)
                    flag = 1;
            }
        }

        if (flag)
            printf("SET sent successfully!\n");

        int trama_size = createSUFrame(buffer, A_SEND, C_UA);
        write(fd, buffer, trama_size);
        return 1;
    }
    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    return 1;
}
