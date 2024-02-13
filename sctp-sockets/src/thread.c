#include <pthread.h>
#include <assert.h>

#include <log.h>
#include <thread.h>

void start_thread(thread_declaration_t *thread) {
    int status;
    assert(thread && thread->handler && "thread initialization failed");
    status = pthread_create(&thread->id,
                            NULL,
                            thread->handler,
                            (void*)thread->args);
    if (status != 0) { 
        LOG("error starting thread: %d", strerror(status));
        exit(1);
    }
}

void join_thread(pthread_t id) {
    void *thread_result;
    int status;
    status = pthread_join(id, &thread_result);
    if (status != 0) {
        LOG("error joining thread: %d", strerror(status));
        exit(1);
    }
}

