#ifndef _MACROS_H_
#define _MACROS_H_

// tramas info
#define FLAG ((unsigned char)0x7E)
#define A_SEND ((unsigned char)0x03)
#define A_RESPONSE ((unsigned char)0x01)

#define C_SET ((unsigned char)0x03)
#define C_DISC ((unsigned char)0x0B)
#define C_UA ((unsigned char)0x07)
#define C_RR_0 ((unsigned char)0x05)
#define C_RR_1 ((unsigned char)0x85)
#define C_RJ_0 ((unsigned char)0x01)
#define C_RJ_1 ((unsigned char)0x81)
#define C_DATA_0 ((unsigned char)0x00)
#define C_DATA_1 ((unsigned char)0x40)

#define ESC_BYTE ((unsigned char)0x7D)
#define BYTE_STUFFING_ESCAPE ((unsigned char)0x5D)
#define BYTE_STUFFING_FLAG ((unsigned char)0x5E)

#endif