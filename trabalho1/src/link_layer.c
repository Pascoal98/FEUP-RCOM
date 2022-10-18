// Link layer protocol implementation

#include "link_layer.h"

// includes
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>

// tramas info
#define FLAG ((unsigned char)0x7E)
#define A_SEND ((unsigned char)0x03)
#define A_RESPONSE ((unsigned char)0x01)
#define C_SET ((unsigned char)0x03)
#define C_DISC ((unsigned char)0x0B)
#define C_UA ((unsigned char)0x07)
#define C_RR_0 0x05
#define C_RR_1 0x85
#define C_RJ_0 0x01
#define C_RJ_1 0x81
#define C_DATA_0 0x00
#define C_DATA_1 0x40

#define ESC_BYTE 0x7D
#define BYTE_STUFFING_ESCAPE 0x5D
#define BYTE_STUFFING_FLAG 0x5E

typedef struct
{
    enum state_t
    {
        S_START,
        S_FLAG,
        S_ADR,
        S_CTRL,
        S_BCC1,
        S_BCC2,
        S_DATA,
        S_ESC,
        S_END,
        S_REJ
    } state;
    unsigned char adr;
    unsigned char ctrl;
    unsigned char bcc;
    unsigned char *data;
    unsigned int data_size;
} Trama;

int fd;
Trama trama;
struct termios oldtio;
struct termios newtio;
LinkLayer linker;

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

///////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////

// ^ = XOR

// bbc1
unsigned char createBCC_header(unsigned char address, unsigned char control)
{
    return address ^ control;
}

// bcc2
unsigned char createBCC_data(unsigned char *frame, int length)
{

    unsigned char bcc = frame[0];

    for (int i = 1; i < length; i++)
    {
        bcc = bcc ^ frame[i];
    }

    return bcc;
}

int createSupervisionFrame(unsigned char *frame, unsigned char control, LinkLayerRole role)
{

    frame[0] = FLAG;

    if (role == LlTx)
    {
        if (control == C_SET || control == C_DISC)
        {
            frame[1] = A_SEND;
        }
        else if (control == C_UA || control == C_RR_0 || control == C_RJ_0 || control == C_RR_1 || control == C_RJ_1)
        {
            frame[1] = A_RESPONSE;
        }
        else
            return 1;
    }
    else if (role == LlRx)
    {
        if (control == C_SET || control == C_DISC)
        {
            frame[1] = A_RESPONSE;
        }
        else if (control == C_UA || control == C_RR_0 || control == C_RJ_0 || control == C_RR_1 || control == C_RJ_1)
        {
            frame[1] = A_SEND;
        }
        else
            return 1;
    }
    else
        return 1;

    frame[2] = control;

    frame[3] = createBCC_header(frame[1], frame[2]);

    frame[4] = FLAG;

    return 0;
}

int createInformationFrame(unsigned char *frame, unsigned char control, unsigned char *info, int infolength)
{

    frame[0] = FLAG;

    frame[1] = A_SEND;

    frame[2] = control;

    frame[3] = createBCC_header(frame[1], frame[2]);

    for (int i = 0; i < infolength; i++)
    {
        frame[i + 4] = info[i];
    }

    unsigned bcc2 = createBCC_data(info, infolength);

    frame[infolength + 4] = bcc2;

    frame[infolength + 5] = FLAG;

    return 0;
}

int byteStuffing(unsigned char *frame, int length)
{

    unsigned char aux[length + 6];

    for (int i = 0; i < length + 6; i++)
    {
        aux[i] = frame[i];
    }

    int header_jump = 4; // onde a informaçao a enviar começa

    for (int i = 4; i < (length + 6); i++)
    {

        if (aux[i] == FLAG && i != (length + 5))
        {
            frame[header_jump] = ESC_BYTE;
            frame[header_jump + 1] = BYTE_STUFFING_FLAG;
            header_jump = header_jump + 2;
        }
        else if (aux[i] == ESC_BYTE && i != (length + 5))
        {
            frame[header_jump] = ESC_BYTE;
            frame[header_jump + 1] = BYTE_STUFFING_ESCAPE;
            header_jump = header_jump + 2;
        }
        else
        {
            frame[header_jump] = aux[i];
            header_jump++;
        }
    }

    return header_jump;
}

int byteDestuffing(unsigned char *frame, int length)
{

    unsigned char aux[length + 5];

    // copiar da trama para o auxiliar
    for (int i = 0; i < (length + 5); i++)
    {
        aux[i] = frame[i];
    }

    int header_jump = 4;

    // iterates through the aux buffer, and fills the frame buffer with destuffed content
    for (int i = 4; i < (length + 5); i++)
    {

        if (aux[i] == ESC_BYTE)
        {
            if (aux[i + 1] == BYTE_STUFFING_ESCAPE)
            {
                frame[header_jump] = ESC_BYTE;
            }
            else if (aux[i + 1] == BYTE_STUFFING_FLAG)
            {
                frame[header_jump] = FLAG;
            }
            i++;
            header_jump++;
        }
        else
        {
            frame[header_jump] = aux[i];
            header_jump++;
        }
    }

    return header_jump;
}

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
        if (byte == FLAG)
        {
            trama->state = S_FLAG;
            break;
        }
        trama->state = S_START;
        break;
    case S_DATA:
        if (byte == FLAG)
        {
            trama->state = S_FLAG;
            break;
        }
        trama->state = S_START;
        break;
    case S_ESC:
        if (byte == FLAG)
        {
            trama->state = S_FLAG;
            break;
        }
        trama->state = S_START;
        break;
    case S_END:
        trama->state = S_START;
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

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

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
