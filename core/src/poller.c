#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <string.h>

#include "sctp_memory.h"
#include "poller.h"
#include "log.h"

poller_t *poller_create(int timeout) {
    poller_t *poller = calloc(1, sizeof(poller_t));
    poller->fds = calloc(128, sizeof(int));
    poller->timeout = timeout;
    poller->max_events = 10;

    poller->epoll_fd = epoll_create1(0);
    if (poller->epoll_fd == -1) {
        LOG("Failed to create poller: %s", strerror(errno));
        exit(1);
    }

    return poller;
}

// EPOLLONESHOT disarms the fd the moment it is reported, so at most one worker
// holds a given socket at a time. That is what keeps SCTP's per-stream ordering:
// level-triggered registration would re-report the fd while a worker was still
// draining it, and a second worker reading the same association concurrently can
// finish its messages in the wrong order. The owner re-arms with poller_rearm
// once it is done, so which worker serves an association still varies per event.
static int poller_ctl(poller_t *poller, poller_action_t *action, int op) {
    struct epoll_event ev;
    ev.events = EPOLLIN|EPOLLONESHOT;
    ev.data.ptr = action;
    if (epoll_ctl(poller->epoll_fd, op, action->fd, &ev)) {
        return -1;
    }

    return 0;
}

// fd is the file descriptor that we want to poller for.
int poller_add(poller_t *poller, poller_action_t *action) {
    // TODO: store fds in poller->fds
    // TODO: realloc fds if not enough room
    return poller_ctl(poller, action, EPOLL_CTL_ADD);
}

// poller_rearm re-enables reporting for an fd disarmed by EPOLLONESHOT. It must
// be called after the batch has been *processed*, not merely read: re-arming
// between the read and the processing lets the next worker overtake the current
// one, which is the reordering this design exists to prevent.
int poller_rearm(poller_t *poller, poller_action_t *action) {
    return poller_ctl(poller, action, EPOLL_CTL_MOD);
}

int poller_del(poller_t *poller, int fd) {
    // TODO: free fds from poller->fds;
    if (epoll_ctl(poller->epoll_fd, EPOLL_CTL_DEL, fd, NULL)) {
        return -1;
    }

    return 0;
}

void *poller_run(void *args) {
    poller_t *poller = (poller_t*) args;
    poller->is_running = 1;
    struct epoll_event events[poller->max_events];
    while (poller->is_running) {
        int nfds = epoll_wait(poller->epoll_fd,
                              events,
                              poller->max_events,
                              poller->timeout);
        if(nfds == -1) {
            int error = errno;
            switch(error) {
                case EINTR:
                    continue;
                    break;
                default:
                    poller->retval = error;
                    return args;
            }
        }

        if(nfds == 0) {
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            poller_action_t * action = events[i].data.ptr;
            if (action == NULL) {
                continue;
            }

            if (events[i].events & EPOLLIN) {
                action->cb(action->fd, action->args);
            }
        }
    }
    return args;
}

void poller_stop(poller_t *poller) {
    poller->is_running = 0;
}

void poller_destroy(poller_t **poller) {
    close((*poller)->epoll_fd);
    FREE((*poller)->fds);
    FREE((*poller));
}
