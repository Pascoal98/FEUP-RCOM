// Link layer protocol implementation

#include "helper.h"

// includes
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
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
unsigned char *bigBuffer;
int bigBufferSize = 0;
int dataFlag = 0;
int timeoutStats = 0;
int retransmitionsStats = 0;

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
    printf("LLOPEN\n");
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
            printf("UA received successfully!\n");
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
            printf("SET received successfully!\n");

        int tramaSize = createSUFrame(buffer, A_SEND, C_UA);
        write(fd, buffer, tramaSize);
        printf("trama size: %d\n", tramaSize);
        return 1;
    }
    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    printf("LLWRITE\n");
    if (bigBufferSize < bufSize * 2 + 10)
    {
        if (bigBufferSize == 0)
            bigBuffer = malloc(bufSize * 2 + 10);
        else
            bigBuffer = realloc(bigBuffer, bufSize * 2 + 10);
    }

    int tramaSize = createInfoFrame(bigBuffer, buf, bufSize, A_SEND, C_DATA_(dataFlag));

    printf("Trama size %d\n", tramaSize);
    printf("\n-------------------------------------\n");
    for (unsigned int i = 0; i < tramaSize;)
    {
        int bytes = write(fd, bigBuffer + i, tramaSize - i);
        if (bytes == -1)
            return -1;
        i += bytes;
    }
    printf("\n-------------------------------------\n");
    int gotPacket = 0;
    int sendAgain = 0;
    int retransmitions = 0;

    trama.data = NULL;
    alarmEnabled = TRUE;
    alarm(linker.timeout);
    if (!gotPacket)
    {
        if (!alarmEnabled)
        {
            sendAgain = 1;
            alarmEnabled = TRUE;
            alarm(linker.timeout);
        }
        if (sendAgain)
        {
            if (retransmitions > 0)
                timeoutStats++;
            if (retransmitions == linker.nRetransmissions)
                return -1;

            for (unsigned int i = 0; i < tramaSize;) // sendAgain package
            {
                int bytesWritten = write(fd, bigBuffer + i, tramaSize - i);
                if (bytesWritten == -1)
                    return -1;
                i += bytesWritten;
            }
            printf("Send again\n");
            sendAgain = 0;
            retransmitions++;
        }
        printf("%d %d sendAgain retransmission\n", sendAgain, retransmitions);

        int bytesRead = read(fd, buffer, 128);

        printf("bytes read %d\n", bytesRead);
        if (bytesRead < 0)
            return -1;

        for (int i = 0; i < bytesRead && !gotPacket && !alarmEnabled; i++)
        {

            state_machine_handler(&trama, buffer[i]);

            printf("%d\n", trama.state);
            if (trama.state == S_END)
            {
                if (trama.adr == A_SEND && (trama.ctrl == C_RR_(0) || trama.ctrl == C_RR_(1)))
                {
                    gotPacket = 1;
                    if (trama.ctrl == C_RR_(dataFlag))
                        printf("Next packet please\n");
                    sendAgain = 0;
                }
                if (trama.adr == A_SEND && trama.ctrl == C_RJ_(dataFlag))
                {
                    retransmitions = 0;
                    retransmitionsStats++;
                }
            }
        }
    }
    dataFlag = dataFlag ? 0 : 1;
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    printf("LLREAD\n");
    int frameSize;

    if (bigBufferSize < 128)
    {
        if (bigBufferSize == 0)
            bigBuffer = malloc(128);
        else
            bigBuffer = realloc(bigBuffer, 128);
    }

    int gotPacket = 0;
    trama.data = packet;

    while (!gotPacket)
    {
        printf("Before read1 LLREAD\n");
        int bytesRead = read(fd, bigBuffer, 128);
        printf("After read1 LLREAD\n");
        if (bytesRead < 0)
            return -1;

        for (int i = 0; i < bytesRead && !gotPacket; i++)
        {
            state_machine_handler(&trama, bigBuffer[i]);
            if (trama.state == S_REJ && trama.adr == A_SEND)
            {
                frameSize = createSUFrame(buffer, A_SEND, (trama.ctrl == C_DATA_(0) ? C_RJ_(0) : C_RJ_(1)));

                write(fd, buffer, frameSize);
                printf("RJ SENT\n");
            }

            if (trama.state == S_END && trama.adr == A_SEND && trama.ctrl == C_SET)
            {
                frameSize = createSUFrame(buffer, A_SEND, C_UA);
                write(fd, buffer, frameSize);
                printf("UA SENT\n");
            }

            if (trama.state == S_END && trama.adr == A_SEND)
            {
                if (trama.ctrl == C_DATA_(dataFlag))
                {

                    dataFlag = dataFlag ? 0 : 1;

                    frameSize = createSUFrame(buffer, A_SEND, C_RR_(dataFlag));
                    write(fd, buffer, frameSize);
                    return trama.data_size;
                }
                else
                {
                    frameSize = createSUFrame(buffer, A_SEND, C_RR_(dataFlag));
                    write(fd, buffer, frameSize);
                }
            }

            if (trama.ctrl == C_DISC)
            {
                gotPacket = 1;
                frameSize = createSUFrame(buffer, A_SEND, (trama.ctrl == C_DATA_(0) ? C_RJ_(0) : C_RJ_(1)));
                write(fd, buffer, frameSize);
                printf("DISCONNECTED\n");
                return -1;
                break;
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    printf("LLCLOSE\n");
    (void)signal(SIGALRM, alarmHandler);

    if (bigBufferSize > 0)
        free(bigBuffer);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    printf("STATISTICS : \n");
    printf("TIME OUT : %d\n", timeoutStats);
    printf("RETRANSMITIONS : %d\n", retransmitionsStats);

    return 1;
}
