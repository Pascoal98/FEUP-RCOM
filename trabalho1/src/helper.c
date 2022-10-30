
#include "helper.h"

///////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////

int createInfoFrame(unsigned char *buffer, const unsigned char *data, unsigned int data_size, unsigned char address, unsigned char control)
{
    buffer[0] = FLAG;
    buffer[1] = address;
    buffer[2] = control;
    buffer[3] = createBCC_header(address, control);

    int added_length = 0;
    unsigned char bcc = 0;

    for (int i = 0; i < data_size; i++)
        added_length += byteStuffing(data + i, 1, buffer + added_length + 4, &bcc);

    added_length += byteStuffing(&bcc, 1, buffer + added_length + 4, NULL);
    buffer[added_length + 4] = FLAG;

    printf("Added length : %d\n", added_length);
    return added_length + 5;
}

int createSUFrame(unsigned char *buffer, unsigned char address, unsigned control)
{

    buffer[0] = FLAG;
    buffer[1] = address;
    buffer[2] = control;
    buffer[3] = createBCC_header(address, control);
    buffer[4] = FLAG;

    return 5;
}

unsigned char createBCC_header(unsigned char address, unsigned char control)
{
    return address ^ control;
}

// bcc2
unsigned char createBCC2(unsigned char *frame, int length)
{

    unsigned char bcc = frame[0];

    for (int i = 1; i < length; i++)
    {
        bcc ^= frame[i];
    }

    return bcc;
}

int byteStuffing(const unsigned char *frame, int sizeBuffer, unsigned char *data, unsigned char *bcc)
{
    int size = 0;
    for (int i = 0; i < sizeBuffer; i++)
    {
        if (bcc != NULL)
            *bcc ^= frame[i];

        if (frame[i] == ESC_BYTE)
        {
            data[size++] = ESC_BYTE;
            data[size++] = BYTE_STUFFING_ESCAPE;
            break;
        }
        if (frame[i] == FLAG)
        {
            data[size++] = ESC_BYTE;
            data[size++] = BYTE_STUFFING_FLAG;
            break;
        }
        data[size++] = frame[i];
    }
    return size;
}

///////////////////////////////////////////
// STATE MACHINE
///////////////////////////////////////////

void state_machine_handler(Trama *trama, unsigned char byte)
{
    switch (trama->state)
    {
    case S_START:
        printf("S_Start\n");
        if (byte == FLAG)
            trama->state = S_FLAG;
        break;

    case S_FLAG:
        printf("S_FLAG\n");
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
        printf("S_adr\n");
        if (byte == C_SET || byte == C_DISC || byte == C_UA || byte == C_RR_(0) || byte == C_RR_(1) || byte == C_RJ_(0) || byte == C_RJ_(1))
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
        printf("S_ctrl\n");
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
        printf("S_bcc1\n");
        if (byte == FLAG)
        {
            if (trama->ctrl == C_DATA_(0) || trama->ctrl == C_DATA_(1))
            {
                trama->state = S_FLAG;
                break;
            }
            trama->state = S_END;
            break;
        }
        if ((trama->ctrl == C_DATA_(0) || trama->ctrl == C_DATA_(1)) && trama->data != NULL)
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
        printf("S_bcc2\n");
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
        printf("S_data\n");
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
        printf("S_esc\n");
        if (byte == BYTE_STUFFING_ESCAPE)
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
        if (byte == BYTE_STUFFING_FLAG)
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
        if (byte == FLAG)
        {
            trama->state = S_REJ;
            break;
        }
        trama->state = S_START;
        break;
    case S_END:
        printf("S_end\n");
        trama->state = S_START;
        break;
    case S_REJ:
        printf("S_REJ\n");
        break;
    }
}