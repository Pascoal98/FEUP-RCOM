// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{

    LinkLayer l_layer;

    strcpy(l_layer.serialPort, serialPort);
    l_layer.baudRate = baudRate;
    l_layer.nRetransmissions = nTries;
    l_layer.timeout = timeout;

    if (strcmp(role, "tx") == 0)
    { // check if transmitter
        printf("tx \n");
        l_layer.role = LlTx;
        if (llopen(l_layer) == -1)
        {
            fprintf(stderr, "Couldn't open link layer\n");
            exit(1);
        }

        printf("Link Layer open\n");

        int fd = open(filename, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Couldn't open the file: %s\n", filename);
            exit(1);
        }
        printf("1\n");
        unsigned char buffer[MAX_PAYLOAD_SIZE];
        int bytesRead;
        do
        {
            printf("2\n");
            bytesRead = read(fd, buffer + 1, MAX_PAYLOAD_SIZE);
            if (bytesRead < 0)
            {
                fprintf(stderr, "Error reading from link layer\n");
                exit(1);
            }
            else if (bytesRead > 0)
            {
                printf("3\n");
                buffer[0] = 1;
                printf("bytes written %d \n", bytesRead);
                int bytesWrite = llwrite(buffer, bytesRead);
                if (bytesWrite < 0)
                {
                    fprintf(stderr, "Error sending data back\n");
                    break;
                }
            }
            else if (bytesRead == 0)
            {
                buffer[0] = 0;
                llwrite(buffer, 1);
                printf("Bytes read == 0\n");
                break;
            }
        } while (bytesRead > 0);

        llclose(1);
        close(fd);
    }
    else
    {
        l_layer.role = LlRx;
        printf("rx \n");
        if (llopen(l_layer) == -1)
        {
            fprintf(stderr, "Couldn't open link layer\n");
            exit(1);
        }

        printf("Link Layer open\n");

        int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd < 0)
        {
            fprintf(stderr, "Couldn't open the file: %s\n", filename);
            exit(1);
        }

        printf("1\n");
        unsigned char buffer[MAX_PAYLOAD_SIZE];
        int bytesRead, checkBytes = 0;
        do
        {
            bytesRead = llread(buffer);
            printf("2\n");
            if (bytesRead < 0)
            {
                fprintf(stderr, "Error reading from link layer\n");
                exit(1);
            }
            else if (bytesRead > 0)
            {
                if (buffer[0] == 1)
                {
                    int bytesWrite = write(fd, buffer + 1, MAX_PAYLOAD_SIZE - 1);
                    printf("3\n");
                    if (bytesWrite < 0)
                    {
                        fprintf(stderr, "Error writing to the file\n");
                        break;
                    }
                    checkBytes += bytesWrite;
                }
                else if (buffer[0] == 0)
                {
                    printf("Gif received\n");
                    break;
                }
            }
        } while (bytesRead > 0);

        llclose(1);
        close(fd);
    }
}
