#include <stdio.h>
#include <stdlib.h>

#include "ftp.h"
#include "parser.h"

void setLogin(ftp_t *info);
void ftpErrorResponse(char response[], char startswith[], int n, char msg[]);
void downloadFile(ftp_t *info);
network_t *handlePASVresp(network_t *net, char buf[]);

int main(int argc, char *argv[])
{

    char buf[COMM_MAX_SIZE];
    if (argc < 2)
    {
        puts("Usage: download <ftp://url>");
        exit(-1);
    }
    ftp_t *info = genParse(argv[1]);

    SET_SOCKET(info);

    // login setup
    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    ftpErrorResponse(buf, "220", 3, "Cant establish a connection.\n");
    setLogin(info);

    downloadFile(info);

    socketSend(info->net, "QUIT");
    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    ftpErrorResponse(buf, "221", 3, "Cant quit nicelly.\n");

    closeSock(info->net);
    delParse(info);
    puts("All done, leaving...");
    return 0;
}

void setLogin(ftp_t *info)
{

    char buf[COMM_MAX_SIZE];
    char user_c[5 + USER_MAX_SIZE + 1] = "USER ";
    char pass_c[5 + PASS_MAX_SIZE + 1] = "PASS ";
    strcat(user_c, info->user);
    strcat(pass_c, info->pass);
    // user_c[5 + strlen(info->user)] = '\0';
    // pass_c[5 + strlen(info->pass)] = '\0';

    socketSend(info->net, user_c);
    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    socketSend(info->net, pass_c);
    ftpErrorResponse(buf, "331", 3, "Login error.\n");
    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    // puts(buf);
    ftpErrorResponse(buf, "230", 3, "Bad login credencials.\n");
}

void ftpErrorResponse(char response[], char startswith[], int n, char msg[])
{

    int r;
    // puts("inside: \n");
    // puts(response);
    r = strncmp(startswith, response, n);
    if (r)
    {
        fputs(msg, stderr);
        exit(-1);
    }
}

void downloadFile(ftp_t *info)
{
    char buf[COMM_MAX_SIZE];

    FILE *file = fopen(info->filename, "wb");

    char bufRETR[4 + PATH_MAX_SIZE] = "RETR ";
    strcat(bufRETR, info->path);

    socketSend(info->net, "PASV");
    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    ftpErrorResponse(buf, "227", 3, "Error while entering Passive Mode.\n");
    // puts(buf);

    network_t *netpasv = handlePASVresp(info->net, buf);
    socketSend(info->net, bufRETR);
    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    ftpErrorResponse(buf, "150", 3, "File not found.\n");

    // readSockFileToStdout(netpasv);

    readSockFileToFile(netpasv, file);

    socketReadCommand(info->net, buf, COMM_MAX_SIZE);
    ftpErrorResponse(buf, "226", 3, "Error transferring.\n");
    fclose(file);
    // puts(buf);
    closeSock(netpasv);
    delNet(netpasv);
}

network_t *handlePASVresp(network_t *net, char buf[])
{
    network_t *netpasv = newNet();
    netpasv->addr = net->addr;
    int newport = parsePort(buf);
    netpasv->port = newport;
    getSocketfd(netpasv);
    return netpasv;
}