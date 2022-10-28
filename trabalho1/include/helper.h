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
    unsigned char bcc2;
    unsigned char *data;
    unsigned int data_size;
} Trama;

unsigned char createBCC_header(unsigned char address, unsigned char control);

unsigned char createBCC2(unsigned char *frame, int length);

int byteStuffing(unsigned char *frame, int length);

int byteDestuffing(unsigned char *frame, int length);

#endif