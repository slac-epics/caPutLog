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
 * This software is copyrighted by the BERLINER SPEICHERRING-
 * GESELLSCHAFT FUER SYNCHROTRONSTRAHLUNG M.B.H., BERLIN, GERMANY.
 * The following terms apply to all files associated with the software.
 *
 * BESSY hereby grants permission to use, copy and modify this software
 * and its documentation for non-commercial, educational or research
 * purposes, provided that existing copyright notices are retained in
 * all copies.
 *
 * The receiver of the software provides BESSY with all enhancements,
 * including complete translations, made by the receiver.
 *
 * IN NO EVENT SHALL BESSY BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE
 * OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN
 * IF BESSY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * BESSY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NON-INFRINGEMENT. THIS SOFTWARE IS PROVIDED
 * ON AN "AS IS" BASIS, AND BESSY HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS OR MODIFICATIONS.
 *
 * Copyright (c) 1996 - 1999
 *			Berliner Elektronenspeicherring-Gesellschaft
 *			      fuer Synchrotronstrahlung m.b.H.,
 *				     Berlin, Germany
 *
 **************************************************************************-*/


#ifndef __DEBUGMSG_H
#define __DEBUGMSG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define DBG_EXTERN(prefix)			\
extern void prefix##SetDebug (int);

#ifdef DEBUGMSG

#define DBG_IMPLEMENT(prefix)			\
void prefix##SetDebug (int verb)		\
{						\
   DBG_INIT;					\
   dbg_level = verb;				\
   return;					\
}

#ifdef vxWorks
				/* VxWorks version: uses semaphores,
				   checks for interrupt context */
#include <vxWorks.h>
#include <semaphore.h>
#include <fcntl.h>

				/* logLib.h *must* be declared later */
#ifdef __INClogLibh
#error debugmsg.h must be included before logLib.h
#else
#define __PROTOTYPE_5_0		/* Use logMsg (char*, ...) declaration */
#include <logLib.h>
#undef __PROTOTYPE_5_0
#endif

#define DBG_LOG logMsg

#define DBG_LOG_MUTEX_ENTRY			\
if (!dbg_int_context) sem_wait(dbg_lock)

#define DBG_LOG_MUTEX_EXIT			\
if (!dbg_int_context) sem_post(dbg_lock)

#define DBG_DECLARE				\
static sem_t* dbg_lock      = NULL;		\
static int dbg_level       = -1;		\
static int dbg_int_context = FALSE;

#define DBG_INIT				\
if (dbg_lock == NULL) dbg_lock = sem_open("dbg_lock", O_CREAT, 0, 1)

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
#define DBG_LOG_MUTEX_ENTRY
#define DBG_LOG_MUTEX_EXIT

#define DBG_DECLARE				\
static int dbg_level       = -1;

#define DBG_INIT
#define DBG_ENTER_INT_CONTEXT
#define DBG_LEAVE_INT_CONTEXT

#define DBG_REGISTER_CMD(prefix)

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
#define DBG_DECLARE
#define DBG_IMPLEMENT(prefix)					\
void prefix##SetDebug (int verb)				\
{								\
   printf("No debug message support compiled into " #prefix	\
	  " (use '-DDEBUGMSG' to enable).\n");			\
}
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

/*+**************************************************************************
 *		Command Registration
 **************************************************************************-*/

#ifdef vxWorks
#define DBG_REGISTER_CMD(prefix)								\
static const iocshArg prefix##SetDebugArg0 = { "level", iocshArgInt };				\
static const iocshArg *const prefix##SetDebugArgs[] = { & prefix##SetDebugArg0 };		\
static const iocshFuncDef prefix##SetDebugDef = {#prefix "SetDebug",0,prefix##SetDebugArgs};\
static void prefix##SetDebugCall(const iocshArgBuf *args)					\
{												\
    prefix##SetDebug(args[0].ival);								\
}												\
static void prefix##SetDebugRegisterCommand(void)						\
{												\
    static int firstTime = 1;									\
    if (firstTime) {										\
        firstTime = 0;										\
        iocshRegister(&prefix##SetDebugDef,prefix##SetDebugCall);			\
    }												\
}												\
epicsExportRegistrar(prefix##SetDebugRegisterCommand);
#endif

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __DEBUGMSG_H */
