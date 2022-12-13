#ifndef __FTP_H__
#define __FTP_H__

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#define COMM_MAX_SIZE 1024
#define USER_MAX_SIZE 64
#define PASS_MAX_SIZE 256
#define FILENAME_MAX_SIZE 256
#define HOST_MAX_SIZE 1024
#define PATH_MAX_SIZE 1024

#define ANON_USER "anonymous"
#define ANON_PASS "anonymous@email.com"

#define END_TERMINATOR "\r\n"
#define READ_BUFFER 4096
#define DEFAULT_PORT 21
#define SET_SOCKET(info)        \
    ({                          \
        setParseIp(info);       \
        getSocketfd(info->net); \
    })

typedef struct s_network
{
    char *addr;
    int port;
    int sockfd;
} network_t;

typedef struct s_ftp
{
    char user[USER_MAX_SIZE];
    char pass[PASS_MAX_SIZE];
    char host[HOST_MAX_SIZE];
    char path[PATH_MAX_SIZE];
    char filename[FILENAME_MAX_SIZE];
    network_t *net;
} ftp_t;

ftp_t *newParse();
void delParse(ftp_t *parse);
network_t *newNet();
void delNet(network_t *net);
void setParseIp(ftp_t *info);
void getSocketfd(network_t *net);
void closeSock(network_t *net);
void socketSend(network_t *net, char *buf);
void socketReadCommand(network_t *net, char *buf, int size);
void readSockFileToStdout(network_t *net);
void readSockFileToFile(network_t *net, FILE *file);

#endif