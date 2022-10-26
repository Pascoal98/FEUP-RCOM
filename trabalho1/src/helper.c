
#include "helper.h"

///////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////

// ^ = XOR

// bbc1
/*
@param address address byte
@param control control byte


*/

int sendFrame(int fd, unsigned char *frame, int length)
{
    int n;
    if (n = write(fd, &frame, length) <= 0)
        return -1; // ERROR

    return n;
}

// read a byte from a fd
int readByte(int fd, unsigned char *byte)
{
    if (read(fd, byte, sizeof(unsigned char)) <= 0)
        return -1; // ERROR

    return 0;
}

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

    unsigned char bcc2 = createBCC_data(info, infolength);

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

/*

int readSupervisionFrame(unsigned char* frame, )
{

    Trama super;

    super.flag = frame[0]

    super.adr = frame[1];

    super.ctrl = frame[2];

    super.bcc = frame[3];





}




int readInformationFrame(unsigned char* frame,)
{

    Trama info;

    info.flag = frame[0];

    info.adr = frame[1];

    info.ctrl = frame[2];

    unsigned char bbc1 = frame[3];

    int ln = sizeof(frame) / sizeof(frame[0]);

    for (int i = 4; i < (ln); i++)
    {
        if (frame[i] == info.flag)
        {
            info.bcc2 = frame[i - 1];
            info.data_size = (i - 2) - 3;

            memcpy(info.data, &frame[4], info.data_size*sizeof(*frame)); //not entirely sure

        }
    }



    // ^ go from bbc1 to bbc2


}

*/
