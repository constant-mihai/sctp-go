#pragma once

#include <stdint.h>

typedef int (test_case_f)();

// TODO: add init, tear down
typedef struct {
    test_case_f *run;
    char *name;
} test_case_t;

typedef struct {
    test_case_t *test;
    uint16_t count;
    uint16_t size;
} test_suite_t;

test_suite_t *test_create_suite(uint16_t size);
void test_register(test_suite_t *suite, test_case_t *test);
int test_run_suite(test_suite_t *suite);
void test_destroy_suite(test_suite_t**);
