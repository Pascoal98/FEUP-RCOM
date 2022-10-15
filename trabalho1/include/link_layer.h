// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

//includes
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdlib.h>

//tramas info
#define FLAG ((unsigned char) 0x7E)
#define A_SEND ((unsigned char) 0x03)
#define A_RESPONSE ((unsigned char) 0x01)
#define C_SET ((unsigned char) 0x03)
#define C_DISC ((unsigned char) 0x0B)
#define C_UA ((unsigned char) 0x07)
#define C_RR_0 0x05
#define C_RR_1 0x85 
#define C_RJ_0 0x01
#define C_RJ_1 0x81

#define ESC_BYTE 0x7D
#define BYTE_STUFFING_ESCAPE 0x5D
#define BYTE_STUFFING_FLAG 0x5E

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

enum state_t {S_START, S_FLAG, S_ADR, S_CTRL, S_BCC1, S_BCC2, S_END, S_REJ} state;

int fd;
struct termios oldtio;
// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

#endif // _LINK_LAYER_H_
