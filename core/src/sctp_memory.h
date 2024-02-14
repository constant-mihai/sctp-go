#pragma once

#define FREE(var) \
    do {\
        free((var));\
        (var) = NULL;\
    } while(0)
