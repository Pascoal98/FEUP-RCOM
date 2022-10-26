#ifndef _HELPER_H_
#define _HELPER_H_

#include "macros.h"
#include "link_layer.h"

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
    unsigned char flag;
    unsigned char adr;
    unsigned char ctrl;
    unsigned char bcc;
    unsigned char bbc2;
    unsigned char *data;
    unsigned int data_size;
} Trama;

int sendFrame(int fd, unsigned char *frame, int length);

int readByte(int fd, unsigned char *byte);

unsigned char createBCC_header(unsigned char address, unsigned char control);

unsigned char createBCC_data(unsigned char *frame, int length);

int createSupervisionFrame(unsigned char *frame, unsigned char control, LinkLayerRole role);

int createInformationFrame(unsigned char *frame, unsigned char control, unsigned char *info, int infolength);

int byteStuffing(unsigned char *frame, int length);

int byteDestuffing(unsigned char *frame, int length);

#endif