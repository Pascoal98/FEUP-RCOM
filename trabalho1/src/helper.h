#ifndef _HELPER_H_
#define _HELPER_H_



int sendFrame(int fd, unsigned char *frame, int length);

int readByte(int fd, unsigned char* byte);

unsigned char createBCC_header(unsigned char address, unsigned char control);

unsigned char createBCC_data(unsigned char *frame, int length);

int createSupervisionFrame(unsigned char *frame, unsigned char control, LinkLayerRole role);

int createInformationFrame(unsigned char *frame, unsigned char control, unsigned char *info, int infolength);

int byteStuffing(unsigned char *frame, int length);

int byteDestuffing(unsigned char *frame, int length);