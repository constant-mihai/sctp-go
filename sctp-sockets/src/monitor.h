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

typedef void (monitor_action_f)(int fd, void *args);

typedef struct {
    // fd is the fd registerd with epoll_ctl. 
    int fd;
    // cb and args are opaque data which encapsulates callbacks
    // that the user of the monitor wants to call when
    // the event triggers. For example, if the FD belongs
    // to an SCTP socket and the event is EPOLLIN,
    // then the user will probably want to read from the socket fd. 
    monitor_action_f *cb;
    void *args;
} monitor_action_t;

monitor_t *monitor_create(int timeout);
int monitor_add(monitor_t *monitor, monitor_action_t *action);
int monitor_del(monitor_t *monitor, int fd);
void *monitor_run(void *args);
void monitor_stop(monitor_t *monitor);
void monitor_destroy(monitor_t **monitor);
