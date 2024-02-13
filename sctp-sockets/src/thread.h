#pragma once

#include <pthread.h>
#include <stdint.h>

#include "sctp.h"

typedef void*(thread_handler_f)(void*);

typedef struct {
    pthread_t id;
    thread_handler_f *handler;
    void *args;
} thread_declaration_t;

typedef struct {
    // Arguments needed by every sctp thread initialization.
    const char *host;
    uint16_t port;
    sctp_options_container_t socket_options_container;
    int retval;

    // Extra arguments, allows for different tests to register different extra args.
    void *extra;
} sctp_thread_args_t;

void start_thread(thread_declaration_t *thread);
void join_thread(pthread_t id);
