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
 * $Revision: 1.2 $
 * $Date: 1996/10/30 10:58:49 $
 *
 * $Author: lange $
 *
 * $Log: debugmsg.h,v $
 * Revision 1.2  1996/10/30 10:58:49  lange
 * New version (version mismatch).
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
extern void prefix##SetDebug (char);

#define DBG_IMPLEMENT(prefix)			\
void prefix##SetDebug (char verb)		\
{						\
   dbg_level = verb;				\
   return;					\
}

#ifdef vxWorks
				/* VxWorks version: uses semaphores,
				   checks for interrupt context */
#include <vxWorks.h>
#include <semaphore.h>
#include <fcntl.h>

extern void logMsg(char*, ...);

#define DBG_LOG logMsg

#define DBG_LOG_MUTEX_ENTRY			\
if (!dbg_int_context) sem_wait(dbg_lock)

#define DBG_LOG_MUTEX_EXIT			\
if (!dbg_int_context) sem_post(dbg_lock)

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

				/* Mutex stuff */

#define LOCK(name) {						\
   sem_wait(&name);						\
   if (4 <= dbg_level) {					\
      DBG_LOG_MUTEX_ENTRY;					\
      DBG_LOG (__FILE__ ":%d: " #name " locked.\n", __LINE__);	\
      DBG_LOG_MUTEX_EXIT;					\
   }								\
}

#define UNLOCK(name) {							\
   sem_post(&name);							\
   if (4 <= dbg_level) {						\
      DBG_LOG_MUTEX_ENTRY;						\
      DBG_LOG (__FILE__ ":%d: " #name " unlocked.\n", __LINE__);	\
      DBG_LOG_MUTEX_EXIT;						\
   }									\
}

#else  /* #ifdef vxWorks */
				/* PC target version: no semaphores,
				   no interrupt checks */
#include <target.h>

#define DBG_LOG printf

#define DBG(verb, string) {			\
   if (verb <= dbg_level)			\
      DBG_LOG (__FILE__ ":%d: %s\r\n",		\
	     __LINE__, string);			\
}

#define PRF(verb, x) {				\
   if (verb <= dbg_level) {			\
      DBG_LOG (__FILE__ ":%d: ", __LINE__);	\
      DBG_LOG x; DBG_LOG ("\r");		\
   }						\
}

#define DBG_DECLARE				\
static char dbg_level       = -1;

#define DBG_INIT
#define DBG_ENTER_INT_CONTEXT
#define DBG_LEAVE_INT_CONTEXT

#endif /* #ifdef vxWorks else */


/*+**************************************************************************
 *		DEBUG Message
 **************************************************************************-*/

#define DBG(verb, string) {			\
   if (verb <= dbg_level) {			\
      DBG_LOG_MUTEX_ENTRY;			\
      DBG_LOG (__FILE__ ":%d:\r\n", __LINE__);	\
      DBG_LOG ("%s\r\n", string);		\
      DBG_LOG_MUTEX_EXIT;			\
   }						\
}

#define PRF(verb, x) {				\
   if (verb <= dbg_level) {			\
      DBG_LOG_MUTEX_ENTRY;			\
      DBG_LOG (__FILE__ ":%d:\r\n", __LINE__);	\
      DBG_LOG x; DBG_LOG ("\r");		\
      DBG_LOG_MUTEX_EXIT;			\
   }						\
}


/*+**************************************************************************
 *		ASSERT Structures
 **************************************************************************-*/

#define MAGIC1 unsigned long dbg_assert_magic1;
#define MAGIC2 unsigned long dbg_assert_magic2;

#define SET_MAGIC(str, type) {			\
   (str)->dbg_assert_magic1 = type##_MAGIC1;	\
   (str)->dbg_assert_magic2 = type##_MAGIC2;	\
}

#define ASSERT_STRUC(str, type) {				\
   if ((str)->dbg_assert_magic1 != type##_MAGIC1)		\
      if (1 <= dbg_level) {					\
	 DBG_LOG_MUTEX_ENTRY;					\
	 DBG_LOG (__FILE__ ":%d: PANIC: " #type			\
		" structure head corrupt.\r\n", __LINE__);	\
	 DBG_LOG_MUTEX_EXIT;					\
      };							\
   if ((str)->dbg_assert_magic2 != type##_MAGIC2)		\
      if (1 <= dbg_level) {					\
	 DBG_LOG_MUTEX_ENTRY;					\
	 DBG_LOG (__FILE__ ":%d: PANIC: " #type			\
		" structure tail corrupt.\r\n", __LINE__);	\
	 DBG_LOG_MUTEX_EXIT;					\
      };							\
}


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
#define SET_MAGIC(str, type)
#define ASSERT_STRUC(str, type)
#define MAGIC1
#define MAGIC2

#ifdef vxWorks
#define LOCK(name) sem_wait(&name)
#define UNLOCK(name) sem_post(&name)
#endif

#endif /* #ifdef DEBUGMSG else */

/*+**************************************************************************
 *		Mutual Exclusion
 **************************************************************************-*/

#ifdef vxWorks
				/* Mutual exclusion mechanism */
#include <semaphore.h>
#define DECLARE_LOCK(name) static sem_t name
#define INIT_LOCK(name) sem_init(&name, 0, 1)
#define IMPLEMENT_LOCK
#else

#include <ctools.h>
#define DECLARE_LOCK(name)
#define INIT_LOCK(name) return_0()
#define LOCK(name) ct_intr_disable()
#define UNLOCK(name) ct_intr_enable()
int return_0(void);

#endif


#ifdef __cplusplus
}
#endif

#endif /* #ifndef __DEBUGMSG_H */
