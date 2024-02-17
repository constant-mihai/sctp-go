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

// fd is the file descriptor that we want to poller for.
int poller_add(poller_t *poller, poller_action_t *action) {
    struct epoll_event ev;
    // TODO: store fds in poller->fds
    // TODO: realloc fds if not enough room
    ev.events = EPOLLIN|EPOLLEXCLUSIVE;
    ev.data.ptr = action;
    if (epoll_ctl(poller->epoll_fd, EPOLL_CTL_ADD, action->fd, &ev)) {
        return errno;
    }

    return 0;
}

int poller_del(poller_t *poller, int fd) {
    // TODO: free fds from poller->fds;
    if (epoll_ctl(poller->epoll_fd, EPOLL_CTL_DEL, fd, NULL)) {
        return errno;
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
