
#include "helper.h"

///////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////

int createDataFrama() {}

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

///////////////////////////////////////////
// STATE MACHINE
///////////////////////////////////////////

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