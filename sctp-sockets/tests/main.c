#include <signal.h>
#include <assert.h>

#include <src/log.h>
#include <sctp_test.h>
#include <sctp_events_test.h>
#include <monitor_test.h>
#include <test.h>

int IS_PROGRAM_RUNNING;

void signal_handler(int signum) {
    LOG("Received signal: %d, stopping program", signum);
    IS_PROGRAM_RUNNING = 0;
}

void install_signal_handlers() {
    struct sigaction sa = { .sa_handler = signal_handler };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    IS_PROGRAM_RUNNING = 1;
}

// TODO: these create a memory leak, handle it later.
// TODO: for example I could store records pointing to the allocated memory.
test_case_t *_create_test(test_case_f *cb, const char* name) {
    test_case_t *test = calloc(1, sizeof(test_case_t));
    // TODO: not sure how the function definiton looks like, postpone defining it.
    // otherwise I will have to refactor a lot.
    //test->init = NULL,
    //test->teardown = NULL,
    test->run = cb,
    test->name = malloc(100*sizeof(char));
    strcpy(test->name, name);

    return test;
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    install_signal_handlers();

    int exit_code = 0;

    LOG("Creating test suites...");
    test_suite_t *suite = test_create_suite(128);
    LOG("Registering tests...");
    test_register(suite, _create_test(sctp_test_send, "sctp test send"));
    test_register(suite, _create_test(sctp_events_test, "sctp test events"));
    test_register(suite, _create_test(monitor_test, "monitor test"));
    test_run_suite(suite);

    test_destroy_suite(&suite);
    LOG("Terminating.");
    fflush(stdout);
    return exit_code;
}
