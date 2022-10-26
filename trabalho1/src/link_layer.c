// Link layer protocol implementation

#include "helper.h"

// includes
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

// Alarm Setup
int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    // printf("Alarm #%d\n", alarmCount);
}

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

void state_machine_handler(Trama *trama, unsigned char byte)
{
    switch (trama->state)
    {
    case S_START:
        if (byte == FLAG)
            trama->state = S_FLAG;
        break;

    case S_FLAG:
        if (byte == A_SEND || byte == A_RESPONSE)
        {
            trama->state = S_ADR;
            trama->adr = byte;
            break;
        }
        if (byte == FLAG)
            break;
        trama->state = S_START;
        break;

    case S_ADR:
        if (byte == C_SET || byte == C_DISC || byte == C_UA || byte == C_RR_0 || byte == C_RR_1 || byte == C_RJ_0 || byte == C_RJ_1)
        {
            trama->state = S_CTRL;
            trama->ctrl = byte;
            trama->bcc = createBCC_header(trama->adr, trama->ctrl);
            break;
        }
        if (byte == FLAG)
        {
            trama->state = S_FLAG;
            break;
        }
        trama->state = S_START;
        break;

    case S_CTRL:
        if (byte == trama->bcc)
        {
            trama->state = S_BCC1;
            break;
        }
        if (byte == FLAG)
        {
            trama->state = S_FLAG;
            break;
        }
        trama->state = S_START;
        break;

    case S_BCC1:
        if (byte == FLAG)
        {
            if (trama->ctrl == C_DATA_0 || trama->ctrl == C_DATA_1)
            {
                trama->state = S_FLAG;
                break;
            }
            trama->state = S_END;
            break;
        }
        if ((trama->ctrl == C_DATA_0 || trama->ctrl == C_DATA_1) && trama->data != NULL)
        {
            trama->data_size = 0;
            if (byte == ESC_BYTE)
            {
                trama->state = S_ESC;
                break;
            }
            trama->data[trama->data_size++] = byte;
            trama->bcc = byte;
            trama->state = S_DATA;
            break;
        }
        trama->state = S_START;
        break;

    case S_BCC2:
        if (byte == 0)
        {
            trama->data[trama->data_size++] = trama->bcc;
            trama->bcc = 0;
        }
        if (byte == FLAG)
        {
            trama->state = S_FLAG;
            break;
        }
        trama->data[trama->data_size++] = trama->bcc;
        trama->data[trama->data_size++] = byte;
        trama->bcc = byte;
        trama->state = S_DATA;
        break;
    case S_DATA:
        if (byte == trama->bcc)
        {
            trama->state = S_BCC2;
            break;
        }
        if (byte == ESC_BYTE)
        {
            trama->state = S_ESC;
            break;
        }
        if (byte == FLAG)
        {
            trama->state = S_REJ;
            break;
        }
        trama->data[trama->data_size++] = byte;
        trama->bcc = createBCC_header(trama->bcc, byte);
        break;
    case S_ESC:
        if (byte == BYTE_STUFFING_ESCAPE)
        {
            if (trama->bcc == FLAG)
            {
                trama->state = S_BCC2;
                break;
            }
            trama->bcc = createBCC_header(trama->bcc, FLAG);
            trama->data[trama->data_size++] = FLAG;
            trama->state = S_DATA;
            break;
        }
        if (byte == BYTE_STUFFING_FLAG)
        {
            if (trama->bcc == ESC_BYTE)
            {
                trama->state = S_BCC2;
                break;
            }
            trama->bcc = createBCC_header(trama->bcc, ESC_BYTE);
            trama->data[trama->data_size++] = ESC_BYTE;
            trama->state = S_DATA;
            break;
        }
        if (byte == FLAG)
        {
            trama->state = S_REJ;
            break;
        }
        trama->state = S_START;
        break;
    case S_END:
        trama->state = S_START;
        break;
    case S_REJ:
        break;
    }
}

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
    newtio.c_cc[VTIME] = 30; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;   // Blocking read until 5 chars received

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

        while (alarmCount < linker.nRetransmissions && !flag)
        {
            alarm(linker.timeout);
            alarmEnabled = TRUE;
        }
    }
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize)
{
    // TODO

    unsigned char controlByte;
    /*if (linker.sequenceNumber == 0)
        controlByte = C_DATA_0;
    else
        controlByte = C_DATA_1;
*/
    if (createInformationFrame(trama.data, controlByte, buf, bufSize) != 0)
    {

    int fullLength;
    // STUFF IT
    if ((fullLength = byteStuffing(trama.data, bufSize)) < 0)
    {
    }

    trama.data_size = fullLength;

    bool dataSent = false;

    while (!dataSent)
    {
        int written;
        if ((written = sendFrame(fd, trama.data, trama.data_size)) == -1)
        {
        }

        // TO BE FINISHED

        
    }
    
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    //receive and read stuuf (create trama)


    int fullLength;
    if (fullLength = byteDestuffing(trama.data, trama.data_size) < 0)
    {

    }

    unsigned char bcc2;

    if (bcc2 = createBCC_data(trama.data, trama.data_size))
    {

    }

    if (bcc2 != trama.bcc2)
    {
        //not good
    }

    //good

    Trama super; 
    if (createSupervisionFrame(super, control, linker.role))
    {

    }

    

    bool dataSent = false;

    while (!dataSent)
    {
        int written;
        if ((written = sendFrame(fd, ssuper.data, super.data_size)) == -1)
        {
        }

        // TO BE FINISHED

        return 0;
    }

    return 0;
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
