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

#define BUFFER_LIMIT (128)

int fd;
struct termios oldtio;
struct termios newtio;
LinkLayer linker;
unsigned char buffer[BUFFER_LIMIT];
unsigned char *bigBuffer;
int bigBufferSize = 0;
int dataFlag = 0;
int timeoutStats = 0;
int retransmitionsStats = 0;
int gotDISC = 0;

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

    fd = open(linker.serialPort, O_RDWR | O_NOCTTY);

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
                int bytes = read(fd, buffer, BUFFER_LIMIT);

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
            int bytes = read(fd, buffer, BUFFER_LIMIT);

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

    for (unsigned int i = 0; i < tramaSize;)
    {
        int bytes = write(fd, bigBuffer + i, tramaSize - i);
        if (bytes == -1)
            return -1;
        i += bytes;
    }

    int gotPacket = 0;
    int sendAgain = 0;
    int retransmitions = 0;

    trama.data = NULL;
    alarmEnabled = TRUE;

    alarm(linker.timeout);
    while (!gotPacket)
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

        printf("Before read in llwrite\n");
        int byteLido = read(fd, buffer, BUFFER_LIMIT);
        printf("bytes read %d\n", byteLido);
        sleep(1);

        if (byteLido < 0)
            return -1;

        for (unsigned int i = 0; i < byteLido && !gotPacket && alarmEnabled; ++i)
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
    if (dataFlag == 0)
    {
        dataFlag = 1;
    }
    else
    {
        dataFlag = 0;
    }
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    printf("LLREAD\n");

    if (bigBufferSize < BUFFER_LIMIT)
    {
        if (bigBufferSize == 0)
            bigBuffer = malloc(BUFFER_LIMIT);
        else
            bigBuffer = realloc(bigBuffer, BUFFER_LIMIT);
    }

    int receivedPacket = 0;
    trama.data = packet;

    while (!receivedPacket)
    {

        printf("Before read1 LLREAD\n");
        int bytesRead = read(fd, bigBuffer, BUFFER_LIMIT);
        printf("After read1 LLREAD %d\n", bytesRead);

        if (bytesRead < 0)
            return -1;

        for (unsigned int i = 0; i < bytesRead; ++i)
        {
            state_machine_handler(&trama, bigBuffer[i]);
            if (trama.state == S_REJ && trama.adr == A_SEND)
            {
                int frameSize = createSUFrame(buffer, A_SEND, (trama.ctrl == C_DATA_(0) ? C_RJ_(0) : C_RJ_(1)));

                write(fd, buffer, frameSize);
                printf("RJ SENT\n");
            }

            if (trama.state == S_END && trama.adr == A_SEND && trama.ctrl == C_SET)
            {
                int frameSize = createSUFrame(buffer, A_SEND, C_UA);
                write(fd, buffer, frameSize);
                printf("UA SENT\n");
            }

            if (trama.state == S_END && trama.adr == A_SEND)
            {
                if (trama.ctrl == C_DATA_(dataFlag))
                {

                    if (dataFlag == 0)
                    {
                        dataFlag = 1;
                    }
                    else
                    {
                        dataFlag = 0;
                    }

                    int frameSize = createSUFrame(buffer, A_SEND, C_RR_(dataFlag));
                    write(fd, buffer, frameSize);
                    printf("Sent RR %i\n", dataFlag);
                    return trama.data_size;
                }
                else
                {
                    int frameSize = createSUFrame(buffer, A_SEND, C_RR_(dataFlag));
                    write(fd, buffer, frameSize);
                }
            }

            if (trama.ctrl == C_DISC)
            {
                gotDISC = 1;
                int frameSize = createSUFrame(buffer, A_SEND, (trama.ctrl == C_DATA_(0) ? C_RJ_(0) : C_RJ_(1)));
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
