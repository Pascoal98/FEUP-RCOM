#include "ftp.h"

ftp_t *newParse()
{
    ftp_t *parse = malloc(sizeof(ftp_t));
    if (parse == NULL)
        return NULL;
    return parse;
}

void delParse(ftp_t *parse)
{
    if (parse != NULL)
    {
        if (parse->net != NULL)
            free(parse->net);
        free(parse);
    }
}

network_t *newNet()
{
    network_t *net = malloc(sizeof(network_t));
    if (net == NULL)
        return NULL;
    return net;
}

// network_t *copyNet(network_t *fromNet) {
//     network_t *net = malloc(sizeof(network_t));
//     if (net == NULL)
//         return NULL;
//     memcpy(&fromNet, &net, sizeof(fromNet));
//     return net;
// }

void delNet(network_t *net)
{
    if (net != NULL)
    {
        free(net);
    }
}

void setParseIp(ftp_t *info)
{
    struct hostent *h;

    if ((h = gethostbyname(info->host)) == NULL)
    {
        herror("gethostbyname()");
        exit(-1);
    }

    info->net->addr = inet_ntoa(*((struct in_addr *)h->h_addr));
}

void getSocketfd(network_t *net)
{
    int sockfd;
    struct sockaddr_in server_addr;

    int port = net->port;
    char *addr = net->addr;

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(addr); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);            /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }

    net->sockfd = sockfd;
}

void closeSock(network_t *net)
{
    if (close(net->sockfd) < 0)
    {
        perror("close()");
        exit(-1);
    }
    net->sockfd = 0;
}

void socketSend(network_t *net, char *buf)
{
    int sockfd = net->sockfd;
    int bytes;

    /*send a string to the server*/
    bytes = write(sockfd, buf, strlen(buf));
    bytes += write(sockfd, END_TERMINATOR, 2);
    // if (bytes > 0)  // debug
    // printf("Bytes escritos %d\n", bytes);  //debug
    // else {
    if (bytes <= 0)
    {
        perror("write()");
        exit(-1);
    }
}

void socketReadCommand(network_t *net, char *resp, int size)
{
    int bytes;

    bytes = read(net->sockfd, resp, size - 1);
    // puts(resp);
    if (bytes > 0)
    {
        if (bytes == size)
        {
            fputs("Command to big to read. Aborting.", stderr);
            exit(-1);
        }
        // printf("Bytes lidos %d\n", bytes); //debug
        resp[bytes] = '\0';
    }
    else
    {
        perror("read()");
        exit(-1);
    }
}

void readSockFileToStdout(network_t *net)
{
    int size = 100;
    char buf[100] = {'\0'};
    int bytes;
    while ((bytes = read(net->sockfd, buf, size)) > 0)
    {
        fputs(buf, stdout);
    }
}

void readSockFileToFile(network_t *net, FILE *file)
{
    char buf[READ_BUFFER];
    int bytes;
    while ((bytes = read(net->sockfd, buf, READ_BUFFER)) > 0)
    {
        if (fwrite(buf, bytes, 1, file) < 1)
        {
            perror("fwrite()");
            exit(-1);
        }
    }
}