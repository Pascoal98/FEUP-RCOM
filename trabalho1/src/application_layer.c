// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

unsigned char buffer[BUF_SIZE];

int next_tlv(unsigned char *buf, unsigned char *type, unsigned char *length, unsigned char **value)
{
    *type = buf[0];
    *length = buf[1];
    *value = buf + 2;
    return *length + 2;
}

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

        printf("------------Link Layer open\n");

        FILE *fp = fopen(filename, "r");

        if (fp == NULL)
        {
            perror("Error opening the file.\n");
            exit(1);
        }
        else
        {
            printf("File has been opened!\n");
        }

        fseek(fp, 0, SEEK_END);
        int len = ftell(fp);
        printf("Total size of the file = %d bytes\n", len);
        fseek(fp, 0, SEEK_SET);

        buffer[0] = CTRL_START;
        buffer[1] = T_SIZE;
        buffer[2] = sizeof(int);

        printf("------------Link Layer Write\n");

        *((int *)(buffer + 3)) = len;

        int flag = 0;

        if (llwrite(buffer, 10) == -1)
        {
            printf("Error llwrite\n");
            flag = 1;
        }

        unsigned long bytesRead = 0;

        for (unsigned char i = 0; bytesRead < len && !flag; i++)
        {
            unsigned long fileBytes = fread(buffer + 4, 1, (len - bytesRead < BUF_SIZE ? len - bytesRead : BUF_SIZE), fp);

            buffer[0] = CTRL_DATA;
            buffer[1] = i;
            buffer[2] = fileBytes >> 8;
            buffer[3] = fileBytes % 256;

            if (llwrite(buffer, fileBytes + 4) == -1)
            {
                printf("Error llwrite\n");
                flag = 1;
                break;
            }

            printf("File sent!\n");
            bytesRead += fileBytes;
        }

        if (!flag)
        {
            buffer[0] = CTRL_END;
            if (llwrite(buffer, 1) == -1)
            {
                printf("Error llwrite\n");
            }
            else
            {
                printf("Done!\n");
            }
        }
        fclose(fp);
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

        unsigned int len = 0, fileReceived = 0;

        printf("------------Link Layer Read\n");

        int bytesRead = llread(buffer);

        unsigned char type, length, *value;

        if (buffer[0] == CTRL_START)
        {
            int offset = 1;
            for (; offset < bytesRead;)
            {
                offset += next_tlv(buffer, &type, &length, &value);

                if (type == T_SIZE)
                {
                    len = *((unsigned int *)value);
                    printf("File Size: %d", len);
                }
            }

            FILE *fp = fopen(filename, "w");

            if (fp == NULL)
            {
                perror("Error creating the file.\n");
                exit(1);
            }
            else
            {
                printf("File has been created!\n");
            }

            unsigned char earlyStop = 0, lastNumber = 0;

            for (; fileReceived < len;)
            {
                int numberBytes = llread(buffer);

                if (numberBytes < 1)
                {
                    printf("Error with llread\n");
                    break;
                }

                if (buffer[0] == CTRL_END)
                {
                    printf("Disconnected!\n");
                    earlyStop = 1;
                    break;
                }

                if (buffer[1] == CTRL_DATA)
                {
                    if (buffer[1] != lastNumber)
                    {
                        printf("Received wrong sequence!\n");
                    }
                    else
                    {
                        unsigned int size = buffer[3] + buffer[2] * 256;

                        fwrite(buffer + 4, 1, size, fp);
                        fileReceived += size;
                    }
                }
            }
            if (!earlyStop)
            {
                int numberBytes = llread(buffer);

                if (numberBytes < 1)
                {
                    printf("Error with llread\n");
                }

                if (buffer[0] == CTRL_END)
                {
                    printf("Disconnected, done sending!\n");
                }
                else
                {
                    printf("Error received wrong package!\n");
                }
            }
            fclose(fp);
        }
        else
        {
            printf("Didn't start with start package\n");
        }
    }

    printf("------------Link Layer Close\n");

    llclose(1);
}
