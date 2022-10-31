
#include "helper.h"

///////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////

/**
 * @brief Create a Information frame
 *
 * @param buffer
 * @param data
 * @param data_size
 * @param address
 * @param control
 * @return int
 */
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

/**
 * @brief creates SU frame
 *
 * @param buffer
 * @param address
 * @param control
 * @return int
 */
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

/**
 * @brief inserts noninformation bits into data to be transfered
 *
 * @param frame
 * @param sizeBuffer
 * @param data
 * @param bcc
 * @return int
 */
int byteStuffing(const unsigned char *frame, int sizeBuffer, unsigned char *data, unsigned char *bcc)
{
    int size = 0;
    for (unsigned int i = 0; i < sizeBuffer; i++)
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

/**
 * @brief state machine to handle the information of the frame and how to behave
 *
 * @param trama
 * @param byte
 */
void state_machine_handler(Trama *trama, unsigned char byte)
{
    switch (trama->state)
    {
    case S_REJ:
    case S_END:
        trama->state = S_START;
    case S_START:
        if (byte == FLAG)
            trama->state = S_FLAG;
        break;

    case S_FLAG:
        trama->data_size = 0;
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
        if (byte == C_SET || byte == C_DISC || byte == C_UA || byte == C_RR_(0) || byte == C_RR_(1) || byte == C_RJ_(0) || byte == C_RJ_(1) || byte == C_DATA_(0) || byte == C_DATA_(1))
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
                trama->bcc = 0;
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
            break;
        }
        if (byte == FLAG)
        {
            trama->state = S_END;
            break;
        }
        if (byte == ESC_BYTE)
        {
            trama->data[trama->data_size++] = trama->bcc;
            trama->bcc = 0;
            trama->state = S_ESC;
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
    }
}