#ifndef HOST_ASSERT_H
#define HOST_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

/* Avoid interactive Windows CRT assertion dialogs in unattended host tests. */
#define assert(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #condition); \
        fflush(stderr); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#endif
