#pragma once

typedef struct poller {
    int epoll_fd;
    int *fds;
    // timeout in miliseconds
    int timeout;
    int max_events;

    int is_running;
    int retval;
} poller_t;

typedef void (poller_action_f)(int fd, void *args);

typedef struct poller_action {
    // fd is the fd registerd with epoll_ctl. 
    int fd;
    // cb and args are opaque data which encapsulates callbacks
    // that the user of the poller wants to call when
    // the event triggers. For example, if the FD belongs
    // to an SCTP socket and the event is EPOLLIN,
    // then the user will probably want to read from the socket fd. 
    poller_action_f *cb;
    void *args;
} poller_action_t;

poller_t *poller_create(int timeout);
int poller_add(poller_t *poller, poller_action_t *action);
int poller_del(poller_t *poller, int fd);
void *poller_run(void *args);
void poller_stop(poller_t *poller);
void poller_destroy(poller_t **poller);
