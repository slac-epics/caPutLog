/* initHooks.c	ioc initialization hooks */ 
/* share/src/db @(#)initHooks.c	1.5     7/11/94 */
/*
 *      Author:		Marty Kraimer
 *      Date:		06-01-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  09-05-92	rcz	initial version
 * .02  09-10-92	rcz	changed return from void to long
 * .03  09-10-92	rcz	changed completely
 * .04  09-10-92	rcz	bug - moved call to setMasterTimeToSelf later
 *
 */


#include	<vxWorks.h>
#include        <stdio.h>
#include	<initHooks.h>
#include        <errMdef.h>

#ifdef MCAN_ON
#include        <multican.h>
#include        <gps.h>
#endif

#ifdef mv162
#include	<devLib.h>
#endif

/*
 * INITHOOKS
 *
 * called by iocInit at various points during initialization
 *
 */

#ifdef MCAN_ON
extern mcan_List_Table* list_table;
#ifdef ATTACH
/*###BEGIN OF USERAREA-1###*/
extern int test_init1_attach(void);
/*###END OF USERAREA-1###*/
#endif
#endif

/* If this function (initHooks) is loaded, iocInit calls this function
 * at certain defined points during IOC initialization */
void initHooks(callNumber)
int	callNumber;
{
#ifdef MCAN_ON   
   int i;
#endif   

#ifdef mv162
   void* mv162_gcsr = (void*) 0xc200;
   void* dummy = NULL;
#endif

	switch (callNumber) {
	case INITHOOKatBeginning :
#ifdef mv162
	   devRegisterAddress(
	      "MV162 VME2chip GCSR",
	      atVMEA16,
	      mv162_gcsr,
	      0x100,
	      &dummy);
#endif
	    break;
	case INITHOOKafterGetResources :
	    break;
	case INITHOOKafterLogInit :
	    break;
	case INITHOOKafterCallbackInit :
	    break;
	case INITHOOKafterCaLinkInit :
	    break;
	case INITHOOKafterInitDrvSup :

#ifdef MCAN_ON
#ifdef ATTACH
/*###BEGIN OF USERAREA-2###*/
	    mcan_attach("lowcal", 1, test_init1_attach);
/*###END OF USERAREA-2###*/
#endif
#endif

	    break;
	case INITHOOKafterInitRecSup :
	break;
	case INITHOOKafterInitDevSup :
	    break;
	case INITHOOKafterTS_init :
	    break;
	case INITHOOKafterInitDatabase :
	    break;
	case INITHOOKafterFinishDevSup :
	    break;
	case INITHOOKafterScanInit :
	    break;
	case INITHOOKafterInterruptAccept :
#ifdef MCAN_ON
          /*Start reader and timer if any protocol is used by one or more elements*/
	   for (i = 0; i < MCAN_PROT; i++) {
	      if (mcan_list_table[i].done == MCAN_INIT_SUCCESS) {
		 if ((gpsStart()) == 1) {
		    errMessage(ERROR,"gpsStart failed!\n");
		    return;
		 }
#ifdef INITHOOK_D
		 printf("gpsStart done.\n");
#endif
		 break;
	      }
	   }
#endif
	    break;
	case INITHOOKafterInitialProcess :
	    break;
	case INITHOOKatEnd :
	    break;
	default:
	    break;
	}
	return;
}
