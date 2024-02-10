#include <signal.h>
#include <assert.h>

#include <src/log.h>
#include <sctp_test.h>
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
// Hold some kind of record for the memory allocated here.
test_case_t *_create_test(test_case_f *cb, const char* name) {
    test_case_t *test = calloc(1, sizeof(test_case_t));
    // TODO: not sure how the function definiton looks like, postpone.
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
    LOG("main");

    install_signal_handlers();

    int exit_code = 0;

    test_suite_t *suite = test_create_suite(128);
    test_register(suite, _create_test(sctp_test_send, "sctp test send"));
    test_register(suite, _create_test(sctp_test_events, "sctp test events"));
    test_run_suite(suite);

    test_destroy_suite(&suite);
    LOG("terminating");
    fflush(stdout);
    return exit_code;
}
