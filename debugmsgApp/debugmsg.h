/* $RCSfile: debugmsg.h,v $ */

/*+**************************************************************************
 *
 * Project:	MultiCAN  -  EPICS-CAN-Connection
 *
 * Module:	None - valid for all modules
 *
 * File:	debugmsg.h
 *
 * Description:	Header file defining macros to implement and use a common
 *              debug message system under vxWorks.
 *
 * Author(s):	Ralph Lange
 *
 * $Revision: 1.1 $
 * $Date: 1996/10/29 13:13:26 $
 *
 * $Author: bergl $
 *
 * $Log: debugmsg.h,v $
 * Revision 1.1  1996/10/29 13:13:26  bergl
 * First checkin
 *
 * Revision 1.2  1996/07/19 13:15:41  lange
 * DEBUG -> DEBUGMSG.
 *
 * Revision 1.1  1996/06/24 18:30:39  lange
 * New debug macros.
 *
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 **************************************************************************-*/


#ifndef __DEBUGMSG_H
#define __DEBUGMSG_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef DEBUGMSG

#define DBG_EXTERN(prefix)			\
extern int prefix##SetDebug (char);

#define DBG_IMPLEMENT(prefix)			\
int prefix##SetDebug (char verb)		\
{						\
   dbg_level = verb;				\
   return(0);					\
}

#ifdef vxWorks
				/* VxWorks version: uses semaphores,
				   checks for interrupt context */
#include <vxWorks.h>
#include <semaphore.h>
#include <fcntl.h>

extern void logMsg(char*, ...);

#define DBG(verb, string) {			\
   if (verb <= dbg_level) {			\
      if (!dbg_int_context) sem_wait(dbg_lock);	\
      logMsg(__FILE__ ":%d:\n", __LINE__);	\
      logMsg("%s\n", string);			\
      if (!dbg_int_context) sem_post(dbg_lock);	\
   }						\
}

#define PRF(verb, x) {				\
   if (verb <= dbg_level) {			\
      if (!dbg_int_context) sem_wait(dbg_lock);	\
      logMsg(__FILE__ ":%d:\n", __LINE__);	\
      logMsg x;					\
      if (!dbg_int_context) sem_post(dbg_lock);	\
   }						\
}

#define DBG_DECLARE				\
static sem_t* dbg_lock      = NULL;		\
static char dbg_level       = -1;		\
static char dbg_int_context = FALSE;

#define DBG_INIT				\
dbg_lock = sem_open("dbg_lock", O_CREAT, 0, 1)

#define DBG_ENTER_INT_CONTEXT			\
dbg_int_context = TRUE

#define DBG_LEAVE_INT_CONTEXT			\
dbg_int_context = FALSE

#else  /* #ifdef vxWorks */
				/* PC target version: no semaphores,
				   no interrupt checks */
#include <target.h>

#define DBG(verb, string) {			\
   if (verb <= dbg_level)			\
      printf(__FILE__ ":%d: %s\r\n",		\
	     __LINE__, string);			\
}

#define PRF(verb, x) {				\
   if (verb <= dbg_level) {			\
      printf(__FILE__ ":%d: ", __LINE__);	\
      printf x; printf("\r");			\
   }						\
}

#define DBG_DECLARE				\
static char dbg_level       = -1;

#define DBG_INIT
#define DBG_ENTER_INT_CONTEXT
#define DBG_LEAVE_INT_CONTEXT

#endif /* #ifdef vxWorks else */

#else  /* #ifdef DEBUGMSG */
				/* No debugging: macros empty */
#define DBG(verb, string)
#define PRF(verb, x)
#define DBG_EXTERN(prefix)
#define DBG_DECLARE
#define DBG_IMPLEMENT(prefix)
#define DBG_INIT
#define DBG_ENTER_INT_CONTEXT
#define DBG_LEAVE_INT_CONTEXT

#endif /* #ifdef DEBUGMSG else */


#ifdef __cplusplus
}
#endif

#endif /* #ifndef __DEBUGMSG_H */
