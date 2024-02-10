#pragma once

#include <stdint.h>

typedef void*(thread_handler_f)(void*);

typedef struct {
    pthread_t id;
    thread_handler_f *handler;
    void *args;
} thread_declaration_t;


void start_thread(thread_declaration_t *thread);
void join_thread(pthread_t id);
