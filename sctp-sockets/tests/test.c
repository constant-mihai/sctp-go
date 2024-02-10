// std
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <test.h>
#include <src/log.h>
#include <src/sctp_memory.h>

test_suite_t *test_create_suite(uint16_t size) {
    test_suite_t *suite = (test_suite_t*) calloc(1, sizeof(test_suite_t)); 
    assert(suite && "failed to allocate memory while creating test suite");
    suite->test = (test_case_t*) calloc(size, sizeof(test_case_t));
    assert(suite->test && "failed to allocate memory for test cases");
    suite->size = size;
    suite->count = 0;

    return suite;
}

void test_register(test_suite_t *suite, test_case_t *test) {
    assert(suite && test && "Suite or test are null");

    if (suite->count == suite->size) {
        suite->test = (test_case_t*) realloc(suite->test, 2*suite->size);
        memset(suite->test+suite->size, 0, suite->size);
        suite->size *= 2;
    }

    suite->test[suite->count].run = test->run;
    suite->test[suite->count].name = (char*) calloc(strlen(test->name)+1, sizeof(char*));
    strcpy(suite->test[suite->count].name, test->name);

    suite->count++;
}

int test_run_suite(test_suite_t *suite) {
    // TODO: suite init
    for (uint16_t i = 0; i < suite->count; i++) {
        // TODO test init
        test_case_t *test = suite->test+i; 
        LOG("Running: %s", test->name);
        assert(test->run() == 0 && test->name);
        // TODO test teardown 
    }
    // TODO: suite teardown 
    return 0;
}

void test_destroy_suite(test_suite_t ** suite) {
    for (uint16_t i = 0; i < (*suite)->count; i++) {
        test_case_t *test = (*suite)->test+i; 
        if (test == NULL) break;
        FREE(test->name);
    }
    FREE((*suite)->test);
    FREE(*suite);
}
