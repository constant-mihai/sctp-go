#include <sys/timerfd.h>

#include <src/poller.h>
#include <src/thread.h>
#include <src/log.h>

#include "poller_test.h"

void _poller_stop(int fd, void *args) {
    poller_t *poller = args;
    LOG("received event for fd: %d", fd);
    LOG("stopping monior with epoll_fd: %d", poller->epoll_fd);
    poller_stop(poller);
}

int poller_test() {
    poller_t *poller = poller_create(100 /*timeout in ms*/);
    thread_declaration_t thread = {
        .handler = poller_run,
        .args = poller,
    };

    start_thread(&thread);

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == -1) {
        LOG("error creating timer: %s", strerror(errno));
        return -1;
    }

    struct itimerspec timeout;
    timeout.it_value.tv_sec = 1;
    timeout.it_value.tv_nsec = 0;
    timeout.it_interval.tv_sec = 0;
    timeout.it_interval.tv_nsec = 0;

    if (timerfd_settime(timer_fd, 0, &timeout, NULL) == -1) {
        LOG("error setting the timer: %s", strerror(errno));
        return -1;
    }

    poller_action_t action = {
        .fd = timer_fd,
        .cb = _poller_stop,
        .args = poller,
    };

    int ret = poller_add(poller, &action);
    if (ret < 0) {
        LOG("error: %s", strerror(errno));
        return -1;
    }

    join_thread(thread.id);
    poller_destroy(&poller);

    return 0;
}
