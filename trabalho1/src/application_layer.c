// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{

    LinkLayer l_layer;

    strcpy(l_layer.serialPort, serialPort);
    l_layer.baudRate = baudRate;
    l_layer.nRetransmissions = nTries;
    l_layer.timeout = timeout;

    int compare = strcmp(role, "tx");

    if (strcmp(role, "tx") == 0)
    { // check if transmitter
        l_layer.role = role;
    }
    else
    { // receiver
        l_layer.role = role;
    }
}
