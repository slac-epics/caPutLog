/******************************************************************************
 *
 * $RCSfile: devGpib.h,v $ (This file is best viewed with a tabwidth of 4)
 *
 ******************************************************************************
 *
 * Author: Benjamin Franksen
 *
 *		Header file to be included in gpib device support modules
 *
 * $Revision: 1.1 $
 *
 * $Date: 1997/05/22 13:17:38 $
 *
 * $Author: franksen $
 *
 * $Log: devGpib.h,v $
 * Revision 1.1  1997/05/22 13:17:38  franksen
 * Added file devGpib.h
 *
 * Revision 1.2  1997/04/23 17:36:18  franksen
 * Added cvs keywords to all source files
 *
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 ******************************************************************************
 *
 * Notes:
 *
 *	This file is intended to be included by GPIB device support modules.
 *	If one or more of the names 
 *
 *		DSET_AI,	DSET_AO,	DSET_LI,	DSET_LO,	
 *		DSET_BI,	DSET_BO,	DSET_MBBO,	DSET_MBBI,	
 *		DSET_SI,	DSET_SO,
 *
 *	are defined before this header is included, the corresponding record
 *	types may be used in the parameter table. No further initialization
 *	is then necessary.
 *	It is not necessary to include the standard VxWorks/EPICS headers, because
 *	this is done here also.
 *
 */
#ifndef INCdevGpibh
#define INCdevGpibh

#include <vxWorks.h>
#include <taskLib.h>
#include <rngLib.h>
#include <types.h>
#include <stdioLib.h>
#include <string.h>

#include <alarm.h>
#include <cvtTable.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <devSup.h>
#include <recSup.h>
#include <drvSup.h>
#include <link.h>
#include <module_types.h>
#include <dbCommon.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>

#include <drvGpibInterface.h>
#include <devCommonGpib.h>

/* forward declaration: */
static struct devGpibParmBlock devSupParms;

/* BUG -- this should be in devCommonGpib.h: */
long devGpibLib_initEv();
long devGpibLib_readEv();
int  devGpibLib_evGpibWork();
int  devGpibLib_evGpibSrq();
int  devGpibLib_evGpibFinish();

/******************************************************************************
 *
 * Define all the DSETs.
 *
 * Note that the dset names must be provided via the #define lines at the top of
 * the file that includes this one. If one is not defined, the init_ and
 * report_ functions are set to NULL in order to generate an error when used.
 *
 * Other than for the debugging flag(s), these DSETs are the only items that
 * will appear in the global name space within the IOC.
 *
 * The last 3 items in the DSET structure are used to point to the parm 
 * structure, the work functions used for each record type, and the srq 
 * handler for each record type.
 *
 ******************************************************************************/

#ifdef DSET_AI
static long report_ai();
static long init_ai(int parm);
#ifdef IOINT_AI
static IOSCANPVT ioscanpvt_ai;
static long get_ioint_info_ai(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt);
#else
#define get_ioint_info_ai NULL
#endif
gDset DSET_AI = {6, {report_ai, init_ai, devGpibLib_initAi, get_ioint_info_ai, 
	devGpibLib_readAi, NULL, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_aiGpibWork, (DRVSUPFUN)devGpibLib_aiGpibSrq}};
static long init_ai(int parm)
{
#ifdef IOINT_AI
	scanIoInit(&ioscanpvt_ai);
#endif
	return(devGpibLib_initDevSup(parm, &DSET_AI));
}
static long report_ai()
{
	return(devGpibLib_report(&DSET_AI));
}
#ifdef IOINT_AI
static long get_ioint_info_ai(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt)
{
	*ppvt = ioscanpvt_ai;
	return OK;
}
#endif
#endif


#ifdef DSET_AO
static long report_ao();
static long init_ao(int parm);
gDset DSET_AO = {6, {report_ao, init_ao, devGpibLib_initAo, NULL, 
	devGpibLib_writeAo, NULL, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_aoGpibWork, NULL}};
static long init_ao(int parm)
{
	return(devGpibLib_initDevSup(parm, &DSET_AO));
}
static long report_ao()
{
	return(devGpibLib_report(&DSET_AO));
}
#endif


#ifdef DSET_LI
static long report_li();
static long init_li(int parm);
#ifdef IOINT_LI
static IOSCANPVT ioscanpvt_li;
static long get_ioint_info_li(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt);
#else
#define get_ioint_info_li NULL
#endif
gDset DSET_LI	 = {5, {report_li, init_li, devGpibLib_initLi, get_ioint_info_li, 
	devGpibLib_readLi, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_liGpibWork, (DRVSUPFUN)devGpibLib_liGpibSrq}};
static long init_li(int parm)
{
#ifdef IOINT_LI
	scanIoInit(&ioscanpvt_li);
#endif
	return(devGpibLib_initDevSup(parm, &DSET_LI));
}
static long report_li()
{
	return(devGpibLib_report(&DSET_LI));
}
#ifdef IOINT_LI
static long get_ioint_info_li(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt)
{
	*ppvt = ioscanpvt_li;
	return OK;
}
#endif
#endif


#ifdef DSET_LO
static long report_lo();
static long init_lo(int parm);
gDset DSET_LO	 = {5, {report_lo, init_lo, devGpibLib_initLo, NULL, 
	devGpibLib_writeLo, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_loGpibWork, NULL}};
static long init_lo(int parm)
{
	return(devGpibLib_initDevSup(parm, &DSET_LO));
}
static long report_lo()
{
	return(devGpibLib_report(&DSET_LO));
}
#endif

#ifdef DSET_BI
static long report_bi();
static long init_bi(int parm);
#ifdef IOINT_BI
static IOSCANPVT ioscanpvt_bi;
static long get_ioint_info_bi(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt);
#else
#define get_ioint_info_bi NULL
#endif
gDset DSET_BI	 = {5, {report_bi, init_bi, devGpibLib_initBi, get_ioint_info_bi, 
	devGpibLib_readBi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_biGpibWork, (DRVSUPFUN)devGpibLib_biGpibSrq}};
static long init_bi(int parm)
{
#ifdef IOINT_BI
	scanIoInit(&ioscanpvt_bi);
#endif
	return(devGpibLib_initDevSup(parm, &DSET_BI));
}
static long report_bi()
{
	return(devGpibLib_report(&DSET_BI));
}
#ifdef IOINT_BI
static long get_ioint_info_bi(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt)
{
	*ppvt = ioscanpvt_bi;
	return OK;
}
#endif
#endif

#ifdef DSET_BO
static long report_bo();
static long init_bo(int parm);
gDset DSET_BO	 = {5, {report_bo, init_bo, devGpibLib_initBo, NULL, 
	devGpibLib_writeBo, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_boGpibWork, NULL}};
static long init_bo(int parm)
{
	return(devGpibLib_initDevSup(parm, &DSET_BO));
}
static long report_bo()
{
	return(devGpibLib_report(&DSET_BO));
}
#endif

#ifdef DSET_MBBI
static long report_mbbi();
static long init_mbbi(int parm);
#ifdef IOINT_MBBI
static IOSCANPVT ioscanpvt_mbbi;
static long get_ioint_info_mbbi(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt);
#else
#define get_ioint_info_mbbi NULL
#endif
gDset DSET_MBBI = {5, {report_mbbi, init_mbbi, devGpibLib_initMbbi, get_ioint_info_mbbi, 
	devGpibLib_readMbbi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_mbbiGpibWork, (DRVSUPFUN)devGpibLib_mbbiGpibSrq}};
static long init_mbbi(int parm)
{
#ifdef IOINT_MBBI
	scanIoInit(&ioscanpvt_mbbi);
#endif
	return(devGpibLib_initDevSup(parm, &DSET_MBBI));
}
static long report_mbbi()
{
	return(devGpibLib_report(&DSET_MBBI));
}
#ifdef IOINT_MBBI
static long get_ioint_info_mbbi(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt)
{
	*ppvt = ioscanpvt_mbbi;
	return OK;
}
#endif
#endif

#ifdef DSET_MBBO
static long report_mbbo();
static long init_mbbo(int parm);
gDset DSET_MBBO = {5, {report_mbbo, init_mbbo, devGpibLib_initMbbo, NULL, 
	devGpibLib_writeMbbo, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)devGpibLib_mbboGpibWork, NULL}};
static long init_mbbo(int parm)
{
	return(devGpibLib_initDevSup(parm, &DSET_MBBO));
}
static long report_mbbo()
{
	return(devGpibLib_report(&DSET_MBBO));
}
#endif

#ifdef DSET_SI
static long report_si();
static long init_si(int parm);
#ifdef IOINT_SI
static IOSCANPVT ioscanpvt_si;
static long get_ioint_info_si(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt);
#else
#define get_ioint_info_si NULL
#endif
gDset DSET_SI	= {5, {report_si, init_si, devGpibLib_initSi, get_ioint_info_si, 
	devGpibLib_readSi, (DRVSUPFUN)&devSupParms,
	(DRVSUPFUN)&devGpibLib_stringinGpibWork, (DRVSUPFUN)devGpibLib_stringinGpibSrq}};
static long init_si(int parm)
{
#ifdef IOINT_SI
	scanIoInit(&ioscanpvt_si);
#endif
	return(devGpibLib_initDevSup(parm, &DSET_SI));
}
static long report_si()
{
	return(devGpibLib_report(&DSET_SI));
}
#ifdef IOINT_SI
static long get_ioint_info_si(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt)
{
	*ppvt = ioscanpvt_si;
	return OK;
}
#endif
#endif

#ifdef DSET_SO
static long report_so();
static long init_so(int parm);
gDset DSET_SO	= {5, {report_so, init_so, devGpibLib_initSo, NULL, 
	devGpibLib_writeSo, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_stringoutGpibWork, NULL}};
static long init_so(int parm)
{
	return(devGpibLib_initDevSup(parm, &DSET_SO));
}
static long report_so()
{
	return(devGpibLib_report(&DSET_SO));
}
#endif

#ifdef DSET_EV
static long report_ev();
static long init_ev(int parm);
#ifdef IOINT_EV
static IOSCANPVT ioscanpvt_ev;
static long get_ioint_info_ev(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt);
#else
#define get_ioint_info_ev NULL
#endif
gDset DSET_EV	 = {5, {report_ev, init_ev, devGpibLib_initEv, get_ioint_info_ev, 
	devGpibLib_readEv, (DRVSUPFUN)&devSupParms, 
	(DRVSUPFUN)devGpibLib_evGpibWork, (DRVSUPFUN)devGpibLib_evGpibSrq}};
static long init_ev(int parm)
{
#ifdef IOINT_EV
	scanIoInit(&ioscanpvt_ev);
#endif
	return(devGpibLib_initDevSup(parm, &DSET_EV));
}
static long report_ev()
{
	return(devGpibLib_report(&DSET_EV));
}
#ifdef IOINT_EV
static long get_ioint_info_ev(int cmd, struct dbCommon *pr, IOSCANPVT *ppvt)
{
	*ppvt = ioscanpvt_ev;
	return OK;
}
#endif
#endif

#endif
