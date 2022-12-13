#include "parser.h"

ftp_t *genParse(char *url)
{
    ftp_t *parse = newParse();
    if (parse == NULL)
        return NULL;

    parse->net = newNet();
    parse->net->port = DEFAULT_PORT;

    int size = 15;
    regmatch_t groupArray[size];

    regex_t regexCompiled;

    char *reg = "ftp://((.+):(.+)@)?((.[^/|:]+)(:[0-9]+)?/((.+/)?(.+)))"; //"ftp://((.+):(.+)@)?((.+)(:[0-9]+)?/((.+/)?(.+)))";

    regcomp(&regexCompiled, reg, REG_EXTENDED);
    regexec(&regexCompiled, url, size, groupArray, 0);

    if (groupArray[2].rm_so > 0 && groupArray[3].rm_so > 0)
    {
        strncpy(parse->user, &url[groupArray[2].rm_so], groupArray[2].rm_eo - groupArray[2].rm_so);
        strncpy(parse->pass, &url[groupArray[3].rm_so], groupArray[3].rm_eo - groupArray[3].rm_so);
    }
    else
    {
        strcpy(parse->user, ANON_USER);
        strcpy(parse->pass, ANON_PASS);
    }
    strncpy(parse->host, &url[groupArray[5].rm_so], groupArray[5].rm_eo - groupArray[5].rm_so);
    strncpy(parse->path, &url[groupArray[7].rm_so], groupArray[7].rm_eo - groupArray[7].rm_so);
    strncpy(parse->filename, &url[groupArray[9].rm_so], groupArray[9].rm_eo - groupArray[9].rm_so);

    if (groupArray[6].rm_so > 0)
    {
        char port[8] = {'\0'};
        strncpy(port, &url[groupArray[6].rm_so + 1], groupArray[6].rm_eo - groupArray[6].rm_so - 1);
        parse->net->port = atoi(port);
    }

    puts(parse->user);
    puts(parse->pass);
    puts(parse->host);
    puts(parse->path);
    puts(parse->filename);
    printf("%d\n", parse->net->port);

    return parse;
}

int parsePort(char buf[])
{
    int msb = 0;
    int lsb = 0;

    int size = 4;
    regmatch_t groupArray[size];
    regex_t regexCompiled;

    char *reg = "\\([0-9]{1,3},[0-9]{1,3},[0-9]{1,3},[0-9]{1,3},([0-9]{1,3}),([0-9]{1,3})\\)";

    regcomp(&regexCompiled, reg, REG_EXTENDED);
    regexec(&regexCompiled, buf, size, groupArray, 0);

    char port_msb[5] = {'\0'};
    strncpy(port_msb, &buf[groupArray[1].rm_so], groupArray[1].rm_eo - groupArray[1].rm_so);
    msb = atoi(port_msb);

    char port_lsb[5] = {'\0'};
    strncpy(port_lsb, &buf[groupArray[2].rm_so], groupArray[2].rm_eo - groupArray[2].rm_so);
    lsb = atoi(port_lsb);

    // printf("port: %d %d\n", msb, lsb);
    return msb * 256 + lsb;
}