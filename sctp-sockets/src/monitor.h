#pragma once

typedef struct {
    int epoll_fd;
    int *fds;
    // timeout in miliseconds
    int timeout;
    int max_events;

    int is_running;
    int retval;
} monitor_t;


typedef void (monitor_action_f)(void*);

typedef struct {
    monitor_action_f *cb;
    void *args;
} monitor_action_t;

monitor_t *monitor_create(int timeout);
int monitor_add(monitor_t *monitor, int fd, void *action);
int monitor_del(monitor_t *monitor, int fd);
void *monitor_run(void *args);
void monitor_stop(monitor_t *monitor);
void monitor_destroy(monitor_t **monitor);
