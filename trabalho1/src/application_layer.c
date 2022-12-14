// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "helper.h"
#include "macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

unsigned char app[BUF_SIZE];

/**
 * @brief Application Layer
 *
 * @param serialPort
 * @param role
 * @param baudRate
 * @param nTries
 * @param timeout
 * @param filename
 */
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

        fseek(fp, 0L, SEEK_END);
        long int len = ftell(fp);
        printf("Total size of the file = %lu bytes\n", len);
        fseek(fp, 0, SEEK_SET);

        app[0] = CTRL_START;
        app[1] = T_SIZE;
        app[2] = sizeof(long);
        printf("------------Link Layer Write\n");

        *((long *)(app + 3)) = len;

        int flag = 0;

        if (llwrite(app, 5) == -1)
        {
            printf("Error llwrite\n");
            flag = 1;
        }

        unsigned long bytesRead = 0;

        for (unsigned char i = 0; bytesRead < len && flag == 0; i++)
        {
            unsigned long fileBytes = fread(app + 4, 1, (len - bytesRead < BUF_SIZE ? len - bytesRead : BUF_SIZE), fp);

            if (fileBytes != (len - bytesRead < BUF_SIZE ? len - bytesRead : BUF_SIZE))
            {
                flag = 1;
                break;
            }

            app[0] = CTRL_DATA;
            app[1] = i;
            app[2] = fileBytes >> 8;
            app[3] = fileBytes % 256;

            if (llwrite(app, fileBytes + 4) == -1)
            {
                printf("Error llwrite\n"); // debugging
                flag = 1;
                break;
            }

            printf("File sent %i!\n", i);
            bytesRead += fileBytes;
        }

        if (!flag)
        {
            app[0] = CTRL_END;
            if (llwrite(app, 1) == -1)
            {
                printf("Error llwrite\n"); // debugging
            }
            else
            {
                printf("Done!\n"); // debugging
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
            fprintf(stderr, "Couldn't open link layer\n"); // debugging
            exit(1);
        }

        printf("Link Layer open\n");

        unsigned int len = 0, fileReceived = 0;

        printf("------------Link Layer Read\n");

        int bytesRead = llread(app);
        printf("bytes read %d \n", bytesRead); // debugging

        unsigned char type, length, *value;

        if (app[0] == CTRL_START)
        {
            int jump = 1;
            while (jump < bytesRead)
            {
                jump += next_tlv(app + jump, &type, &length, &value);

                if (type == T_SIZE)
                {
                    len = *((unsigned int *)value);
                    printf("File Size: %d\n", len); // debugging
                }
            }

            FILE *fp = fopen(filename, "w");

            if (fp == NULL)
            {
                perror("Error creating the file.\n"); // debugging
                exit(1);
            }

            printf("File has been created!\n"); // debugging

            int reachedEND = 1;
            unsigned char lastNumber = 0;

            while (fileReceived < len)
            {
                int numberBytes = llread(app);

                if (numberBytes < 1)
                {
                    if (numberBytes == -1)
                    {
                        printf("Error llread\n"); // debugging
                        break;
                    }
                    else
                    {
                        printf("Error package too small\n"); // debugging
                    }
                }

                if (app[0] == CTRL_END)
                {
                    printf("Disconnected!\n");
                    reachedEND = 0;
                    break;
                }

                if (app[0] == CTRL_DATA)
                {

                    if (app[1] != lastNumber)
                    {
                        printf("Error!\n"); // debugging
                    }
                    else
                    {
                        unsigned int size = app[3] + app[2] * 256;
                        fwrite(app + 4, 1, size, fp);
                        fileReceived += size;
                        lastNumber++;
                    }
                }
            }
            if (reachedEND)
            {
                int numberBytes = llread(app);

                if (numberBytes <= 0)
                {
                    printf("Error with llread\n"); // debugging
                }

                if (app[0] == CTRL_END)
                {
                    printf("Disconnecting, done!\n");
                }
                else
                {
                    printf("Error received wrong package!\n"); // debugging
                    for (unsigned int i = 0; i < 10; i++)
                        printf("%i ", app[i]);
                    printf("\n");
                }
            }
            fclose(fp);
        }
        else
        {
            printf("Didn't start with start package\n"); // debugging
            for (unsigned int i = 0; i < 10; i++)
                printf("%i ", app[i]);
            printf("\n");
        }
    }

    printf("------------Link Layer Close\n");

    llclose(1);
}
