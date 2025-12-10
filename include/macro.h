#pragma once
#include <stdio.h>

#define CHK0(op) do { \
    if((op) != 0) { \
        perror(#op); \
        return -1; \
    } \
} while(0)

#define CHKNULL(op) do { \
    if((op) == NULL) { \
        perror(#op); \
        return -1; \
    } \
} while(0)

#define CHKERR(op) do { \
    if((op) == -1) { \
        perror(#op); \
        return EXIT_FAILURE; \
    } \
} while(0)
