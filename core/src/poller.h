#pragma once

#include <stdint.h>

typedef struct poller {
    int epoll_fd;
    int *fds;
    // timeout in miliseconds
    int timeout;
    int max_events;

    int is_running;
    int retval;
} poller_t;

// args is an integer rather than a void*: the Go side passes a
// runtime/cgo.Handle through it, which is a uintptr. Converting that back into
// an unsafe.Pointer would be flagged by go vet, and a plain Go pointer cannot
// be stored in C memory at all.
typedef void (poller_action_f)(int fd, uintptr_t args);

typedef struct poller_action {
    // fd is the fd registerd with epoll_ctl.
    int fd;
    // cb and args are opaque data which encapsulates callbacks
    // that the user of the poller wants to call when
    // the event triggers. For example, if the FD belongs
    // to an SCTP socket and the event is EPOLLIN,
    // then the user will probably want to read from the socket fd.
    poller_action_f *cb;
    uintptr_t args;
} poller_action_t;

poller_t *poller_create(int timeout);
int poller_add(poller_t *poller, poller_action_t *action);
// poller_rearm re-enables an fd that EPOLLONESHOT disarmed when it was reported.
// Until it is called the fd is never reported again, so a caller that takes an
// fd from the poller and forgets to re-arm it silences that socket for good.
int poller_rearm(poller_t *poller, poller_action_t *action);
int poller_del(poller_t *poller, int fd);
void *poller_run(void *args);
void poller_stop(poller_t *poller);
void poller_destroy(poller_t **poller);
