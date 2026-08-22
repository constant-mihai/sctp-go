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
    // stderr, not stdout: under `go test` stdout is a pipe and therefore
    // block-buffered, so log lines would be swallowed. stderr is unbuffered.
    fprintf(stderr, "%s:%d ", file, line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

