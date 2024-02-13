#include <sys/timerfd.h>

#include <src/monitor.h>
#include <src/thread.h>
#include <src/log.h>

#include "monitor_test.h"

void _monitor_stop(void *args) {
    monitor_t *monitor = args;
    LOG("stopping monior with epoll_fd: %d", monitor->epoll_fd);
    monitor_stop(monitor);
}

int monitor_test() {
    monitor_t *monitor = monitor_create(100 /*timeout in ms*/);
    thread_declaration_t thread = {
        .handler = monitor_run,
        .args = monitor,
    };

    start_thread(&thread);

    // Create timerfd
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == -1) {
        LOG("error creating timer: %s", strerror(errno));
        return -1;
    }

    // Set timeout value
    struct itimerspec timeout;
    timeout.it_value.tv_sec = 1;
    timeout.it_value.tv_nsec = 0;
    timeout.it_interval.tv_sec = 0;
    timeout.it_interval.tv_nsec = 0;

    if (timerfd_settime(timer_fd, 0, &timeout, NULL) == -1) {
        LOG("error setting the timer: %s", strerror(errno));
        return -1;
    }

    monitor_action_t action = {
        .cb = _monitor_stop,
        .args = monitor,
    };

    int err = monitor_add(monitor, timer_fd, &action);
    if (err) {
        LOG("error: %s", strerror(err));
        return -1;
    }

    join_thread(thread.id);
    monitor_destroy(&monitor);

    return 0;
}
