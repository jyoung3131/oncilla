/**
 * @file debug.h
 * @author Alexander Merritt, merritt.alex@gatech.edu
 * Modified from the Shadowfax debug.h file
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define __DEBUG_ENABLED     (getenv("OCM_VERBOSE"))

/**
 * Halt immediately if expression 'expr' evaluates to true and print a message.
 * Must be used where code is NOT expected to fail. Bad examples of its use
 * include checking return codes to networking functions and the like (we cannot
 * control failure of such functions). A good example is checking for specific
 * return values, where you wrote both the function itself, and the code which
 * invokes it.
 */
#define BUG(expr)                       \
    do {                                \
        if (expr) {                     \
            __detailed_print("BUG\n");  \
            assert(0);                  \
        }                               \
    } while(0)                          \

#define ABORT2(expr)                        \
    do {                                    \
        if (expr) {                         \
            __detailed_print("ABORT\n");    \
            assert(0);                      \
        }                                   \
    } while(0)                              \

#define ABORT()     ABORT2(1)

#define __detailed_print(fmt, args...)                  \
    do {                                                \
        fprintf(stderr, "(%d:%d) %s::%s[%d]: ",     \
                getpid(),(pid_t)syscall(SYS_gettid),    \
                __FILE__, __func__, __LINE__);          \
        printf(fmt, ##args);                            \
        fflush(stderr);                                 \
    } while(0)

/* debug printing. will only print if env var OM_VERBOSE is defined */
#define printd(fmt, args...)                                            \
    do {                                                                \
        if(__DEBUG_ENABLED) {                                           \
            __detailed_print(fmt, ##args);                              \
        }                                                               \
    } while(0)

#endif /* DEBUG_H_ */
