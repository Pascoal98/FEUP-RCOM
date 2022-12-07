#ifndef __PARCER_H__
#define __PARCER_H__

#include <string.h>
#include <stdlib.h>
#include <regex.h>

#include "ftp.h"

ftp_t *genParse(char *url);
int parsePort(char buf[]);

#endif