#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG(fmt, ...) sctp_log(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

static void
sctp_log(const char* file, int line, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("%s:%d ", file, line);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}

