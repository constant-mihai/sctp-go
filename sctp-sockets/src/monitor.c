#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <string.h>

#include "sctp_memory.h"
#include "monitor.h"
#include "log.h"

monitor_t *monitor_create(int timeout) {
    monitor_t *monitor = calloc(1, sizeof(monitor_t));
    monitor->fds = calloc(128, sizeof(int));
    monitor->timeout = timeout;
    monitor->max_events = 10;

    monitor->epoll_fd = epoll_create1(0);
    if (monitor->epoll_fd == -1) {
        LOG("Failed to create monitor: %s", strerror(errno));
        exit(1);
    }

    return monitor;
}

// fd is the file descriptor that we want to monitor for.
int monitor_add(monitor_t *monitor, int fd, void *ptr) {
    struct epoll_event ev;
    ev.events = EPOLLIN|EPOLLEXCLUSIVE;
    ev.data.fd = fd;
    ev.data.ptr = ptr;
    if (epoll_ctl(monitor->epoll_fd, EPOLL_CTL_ADD, fd, &ev)) {
        return errno;
    }

    return 0;
}

int monitor_del(monitor_t *monitor, int fd) {
    if (epoll_ctl(monitor->epoll_fd, EPOLL_CTL_DEL, fd, NULL)) {
        return errno;
    }

    return 0;
}

void *monitor_run(void *args) {
    monitor_t *monitor = (monitor_t*) args;
    monitor->is_running = 1;
    struct epoll_event events[monitor->max_events];
    while (monitor->is_running) {
        int nfds = epoll_wait(monitor->epoll_fd,
                              events,
                              monitor->max_events,
                              monitor->timeout);
        if(nfds == -1) {
            int error = errno;
            switch(error) {
                case EINTR:
                    continue;
                    break;
                default:
                    monitor->retval = error;
                    return args;
            }
        }

        if(nfds == 0) {
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            monitor_action_t * action = events[i].data.ptr;
            if (action == NULL) {
                continue;
            }

            if (events[i].events & EPOLLIN) {
                action->cb(action->args);
            }
        }
    }
    return args;
}

void monitor_stop(monitor_t *monitor) {
    monitor->is_running = 0;
}

void monitor_destroy(monitor_t **monitor) {
    close((*monitor)->epoll_fd);
    FREE((*monitor)->fds);
    FREE((*monitor));
}
