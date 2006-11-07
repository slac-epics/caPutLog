#ifndef INCDbCh
#define INCDbCh

#include <stdio.h>
#include <stdlib.h>

/*
 * Some sensible default handlers. User code can override them with whatever
 * they deem more useful.
 */
#ifdef vxWorks
/* this one suspends the calling task */
#include <taskLib.h>
#define handleFailedPrecondition taskSuspend(0)
#else
/* this one terminates the program */
#define handleFailedPrecondition exit(1)
#endif

#ifndef NDEBUG
#define assertPre(cond,call) (!(cond)) ?\
    (fprintf(stderr,\
        "ERROR IN CALL " #call\
        " AT (" __FILE__ ":%d):\n"\
        "PRECONDITION FAILED: " #cond "\n", __LINE__),\
        handleFailedPrecondition,\
        unchecked_ ## call) : unchecked_ ## call
#else
#define assertPre(cond,call) unchecked_ ## call
#endif

#endif
