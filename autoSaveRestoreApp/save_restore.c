/* 12/12/96  tmm  v1.3 get doubles and doubles native (independent of
 *                the EPICS "precision" setting, as is BURT) and convert
 *                them using #define FLOAT_FMT, DOUBLE_FMT, below.
 * 01/03/97  tmm  v1.4 make backup save file if save succeeds
 * 06/03/97  tmm  v1.5 write doubles and doubles in g format. Fixed fdbrestore.
 * 04/26/99  tmm  v1.6 set first character of save file to '&' until we've
 *                written and flushed everything, then set it to '#'.
 * 05/11/99  tmm  v1.7 make sure backup save file is ok before writing new
 *                save file.
 * 11/23/99  tmm  v1.8 defend against condition in which ioc does not have
 *                write access to directory, but does have write access to
 *                existing autosave file.  In this case, we can't erase the
 *                file or change its length.
 * 01/05/01  tmm  v1.9 proper treatment of event_handler_args in function
 *                argument list.
 * 05/11/01  tmm  v2.0 defend against null trigger channel.
 * 12/20/01  tmm  v2.1 Test file open only if save_restore_test_fopen != 0
 * 02/22/02  tmm  v2.2 Work around for problems with USHORT fields 
 * 02/26/02  tmm  v2.3 Adding features from Frank Lenkszus' version of the code:
 *                replaced logMsg with epicsPrintf  [that was a mistake!!]
 *                added set_savefile_path(char *path)
 *			      added set_requestfile_path(char *path)
 *			      added set_saveTask_priority(int priority)
 *			      added remove_data_set(char *filename)
 *                added reload_periodic_set(char * filename, double period)
 *                added reload_triggered_set(char *filename, char *trig_channel)
 *                added reload_monitor_set(char * filename, double period)
 *                added reload_manual_set(char * filename)
 *                check for errors on backup file creation
 *                check for errors on writing save file
 *                don't overwrite backup file if previous save failed
 *                added fdbrestoreX(char *filename) to restore from backup file
 *
 *                in set_XXX_path(), allow path with or without trailing '/'
 * 03/08/02  tmm  v2.4 Use William Lupton's macLib to simplify request files.
 *                Command-line arguments redefined from double to int. (Required
 *                for PPC.)
 * 03/13/02  tmm  v2.5 Allow for multiple directories in reqFilePath.
 * 03/15/02  tmm  v2.6 check saveRestoreFilePath before using it.
 * 04/22/03  tmm  v2.7 Add add_XXXfile_path(): like set_XXXfile_path(),
 *                but allows caller to pass path as two args to be concatenated.
 * 04/23/03  tmm  v2.8 Add macro-string argument to create_xxx_set(), added
 *                argument 'verbose' to save_restoreShow();
 * 04/28/03  tmm  v2.9 Drop add_XXXfile_path, and add second argument to
 *                set_XXXfile_path();
 * 05/20/03  tmm  v2.9a Looking for problems that cause save_restore() to hang
 * 07/14/03  tmm  v3.0 In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.  No longer
 *                test fopen() before save.  This caused more problems than it
 *                solved.
 * 07/21/03  tmm  v3.1 More checks during file writing.
 * 08/13/03  tmm  v3.2 Status PV's.  Merge bug fixes from 3.13 and 3.14 versions into
 *                something that will work under 3.13  
 * 08/19/03  tmm  v3.3 Manage NFS mount.  Replace epicsPrintf with errlogPrintf.
 *                Replace logMsg with errlogPrintf except in callback routines.
 */
#define		SRVERSION "save/restore V3.3"

#ifdef vxWorks
#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<usrLib.h>
#include	<taskLib.h>
#include	<wdLib.h>
#include	<nfsDrv.h>
#include	<ioLib.h>
extern int logMsg(char *fmt, ...);
#else
#define OK 0
#define ERROR -1
#define logMsg errlogPrintf
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<ctype.h>

#include	<tickLib.h>
#include	<sys/stat.h>
#include	<time.h>

#include	<dbDefs.h>
#include	<cadef.h>
#include	<epicsPrint.h>
#include 	<tsDefs.h>
#include    <macLib.h>
#include	"save_restore.h"
#include 	"fGetDateStr.h"

#define TIME2WAIT 20		/* time to wait for semaphore sem_remove */

/*** Debugging variables, macros ***/
/* #define NODEBUG */
#ifdef NODEBUG
#define STATIC static
#define Debug(l,FMT,V) ;
#else
#define STATIC 
#define Debug(l,FMT,V) {  if (l <= save_restoreDebug) \
			{ errlogPrintf("%s(%d):",__FILE__,__LINE__); \
			  errlogPrintf(FMT,V); } }
#define DebugNV(l,FMT) {  if (l <= save_restoreDebug) \
			{ errlogPrintf("%s(%d):",__FILE__,__LINE__); \
			  errlogPrintf(FMT); } }
#endif
#define myPrintErrno(errNo) {errlogPrintf("%s(%d): [0x%x]=",__FILE__,__LINE__,errNo); printErrno(errNo);}

#define FLOAT_FMT "%.7g"
#define DOUBLE_FMT "%.14g"

/*
 * data structure definitions 
 */

STATIC struct chlist *lptr;		/* save-set listhead */
STATIC SEM_ID 		sr_mutex;	/* mut(ual) ex(clusion) for list of save sets */
STATIC SEM_ID		sem_remove;	/* delete list semaphore */

/* save_methods - used to determine when a file should be deleted */
#define PERIODIC	0x01		/* set when timer goes off */
#define TRIGGERED	0x02		/* set when trigger channel goes off */
#define	TIMER		0x04
#define	CHANGE		0x08
#define MONITORED	(TIMER|CHANGE)	/* set when timer expires and channel changes */
#define MANUAL		0x10		/* set on request */
#define	SINGLE_EVENTS	(PERIODIC|TRIGGERED|MANUAL)

struct chlist {							/* save set list element */
	struct chlist	*pnext;				/* next list */
	struct channel	*pchan_list;		/* channel list head */
	char			reqFile[80];		/* request file name */
	char 			last_save_file[80];	/* file name last used for save */
	char			save_file[80];		/* file name to use on next save */
	int				save_method;		/* bit for each save method requested */
	int				enabled_method;		/* bit for each save method enabled */
	short			save_ok;			/* !=0 -> last save ok */
	int				save_state;			/* state of this save set 	*/
	int				period;				/* time between saves (max for on change) */
	int				monitor_period;		/* time between saves (max for on change) */
	char			trigger_channel[40];	/* db channel to trigger save  */
	WDOG_ID			saveWdId;
	int				not_connected;		/* # bad channels not saved/connected */
	int				backup_sequence_num;	/* appended to backup files */
	time_t			backup_time;
	time_t			save_time;				/* status reporting variables */
	long			status;
	char			status_PV[40];
	char			state_PV[40];
	char			statusStr_PV[40];
	char			time_PV[40];
	char			timeStr[40];
	char			statusStr[40];
	chid			status_chid;
	chid			state_chid;
	chid			statusStr_chid;
	chid			time_chid;
};

struct channel {			/* database channel list element */
	struct channel *pnext;	/* next channel */
	char		name[64];	/* channel name */
	chid 		chid;		/* channel access id */
	char 		value[64];	/* value string */
	short		enum_val;	/* short value of an enumerated field */
	short		valid;		/* we think we got valid data for this channel */
};

struct pathListElement {
	struct pathListElement *pnext;
	char path[PATH_SIZE+1];
};
/*
 * module global variables
 */
STATIC short	save_restore_init = 0;
STATIC char 	*SRversion = SRVERSION;
STATIC struct pathListElement *reqFilePathList = NULL;
char			saveRestoreFilePath[PATH_SIZE] = "";	/* path to save files, also used by dbrestore.c */
STATIC int		taskPriority =  190;	/* initial task priority */
STATIC int		taskID = 0;				/* save_restore task tid */
STATIC char		remove_filename[80];	/* name of list to delete */
STATIC int		remove_dset = 0;
STATIC int		remove_status = 0;

STATIC char	status_prefix[10];

STATIC long	save_restoreStatus, save_restoreHeartbeat;
/* Make save_restoreRecentlyStr huge because sprintf may overrun (vxWorks has no snprintf) */
STATIC char	save_restoreStatusStr[40], save_restoreRecentlyStr[300];
STATIC char	save_restoreStatus_PV[40] = "", save_restoreHeartbeat_PV[40] = ""; 
STATIC char	save_restoreStatusStr_PV[40] = "", save_restoreRecentlyStr_PV[40] = "";
STATIC chid	save_restoreStatus_chid, save_restoreHeartbeat_chid,
			save_restoreStatusStr_chid, save_restoreRecentlyStr_chid;

STATIC long	reboot_restoreStatus;
STATIC char	reboot_restoreStatusStr[40];
STATIC char	reboot_restoreStatus_PV[40] = "", reboot_restoreStatusStr_PV[40] = "";
STATIC chid	reboot_restoreStatus_chid, reboot_restoreStatusStr_chid;
STATIC char	reboot_restoreTime_PV[40];
STATIC char	reboot_restoreTimeStr[40];
STATIC chid	reboot_restoreTime_chid;

STATIC int	save_restoreDebug = 0;
STATIC int	save_restoreNumSeqFiles = 3;
STATIC int	save_restoreSeqPeriodInSeconds = 60;
STATIC int	save_restoreIncompleteSetsOk = 1;		/* will save/restore incomplete sets */
STATIC int	save_restoreDatedBackupFiles = 1;
STATIC char	save_restoreNFSHostName[40] = "";
STATIC char	save_restoreNFSHostAddr[40] = "";

STATIC int	save_restoreNFSOK=0;
STATIC int	save_restoreIoErrors=0;
STATIC int	save_restoreRemountThreshold=10;

/* configuration parameters */
STATIC int	min_period	= 4;	/* save no more frequently than every 4 seconds */
STATIC int	min_delay	= 1;	/* check need to save every 1 second */
				/* worst case wait can be min_period + min_delay */

/* functions */
STATIC int mountFileSystem(void);
STATIC void dismountFileSystem(void);
STATIC int periodic_save(struct chlist	*plist);
STATIC void triggered_save(struct event_handler_args);
STATIC int on_change_timer(struct chlist *plist);
STATIC void on_change_save(struct event_handler_args);
STATIC int save_restore(void);
STATIC int connect_list(struct chlist *plist);
STATIC int enable_list(struct chlist *plist);
STATIC int get_channel_list(struct chlist *plist);
STATIC int write_it(char *filename, struct chlist *plist);
STATIC int write_save_file(struct chlist	*plist);
STATIC void do_seq(struct chlist *plist);
STATIC int create_data_set(char *filename,	int save_method, int period,
		char *trigger_channel, int mon_period, char *macrostring);
int manual_save(char *request_file);
int set_savefile_name(char *filename, char *save_filename);
int create_periodic_set(char *filename, int period, char *macrostring);
int create_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int create_monitor_set(char *filename, int period, char *macrostring);
int create_manual_set(char *filename, char *macrostring);
int fdbrestore(char *filename);
void save_restoreShow(int verbose);
int set_requestfile_path(char *path, char *pathsub);
int set_savefile_path(char *path, char *pathsub);
int set_saveTask_priority(int priority);
int remove_data_set(char *filename);
int reload_periodic_set(char *filename, int period, char *macrostring);
int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int reload_monitor_set(char * filename, int period, char *macrostring);
int reload_manual_set(char * filename, char *macrostring);
int fdbrestoreX(char *filename);

STATIC int readReqFile(const char *file, struct chlist *plist, char *macrostring);

/* user-callable function to set save_restore parameters */
void save_restoreSet_Debug(int level) {save_restoreDebug = level;}
void save_restoreSet_NumSeqFiles(int numSeqFiles) {save_restoreNumSeqFiles = numSeqFiles;}
void save_restoreSet_SeqPeriodInSeconds(int period) {save_restoreSeqPeriodInSeconds = MAX(10, period);}
void save_restoreSet_IncompleteSetsOk(int ok) {save_restoreIncompleteSetsOk = ok;}
void save_restoreSet_DatedBackupFiles(int ok) {save_restoreDatedBackupFiles = ok;}
void save_restoreSet_status_prefix(char *prefix) {strncpy(status_prefix, prefix, 9);}

void save_restoreSet_NFSHost(char *hostname, char *address) {
	if (save_restoreNFSOK) dismountFileSystem();
	strncpy(save_restoreNFSHostName, hostname, 39);
	strncpy(save_restoreNFSHostAddr, address, 39);
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && saveRestoreFilePath[0]) 
		mountFileSystem();
}

/*
 * method PERIODIC - timer has elapsed
 */
STATIC int periodic_save(struct chlist	*plist)
{
	plist->save_state |= PERIODIC;
	return(OK);
}


/*
 * method TRIGGERED - ca_monitor received for trigger PV
 */
STATIC void triggered_save(struct event_handler_args event)
{
    struct chlist *plist = (struct chlist *) event.usr;

    if (event.dbr)
	plist->save_state |= TRIGGERED;
}


/*
 * method MONITORED - timer has elapsed
 */
STATIC int on_change_timer(struct chlist *plist)
{
	if (save_restoreDebug >= 10) logMsg("on_change_timer for %s (period is %d ticks)\n",
			plist->reqFile, plist->monitor_period);
	plist->save_state |= TIMER;
	return(OK);
}


/*
 * method MONITORED - ca_monitor received for a PV
 * get all channels; write to disk; restart timer
 */
STATIC void on_change_save(struct event_handler_args event)
{
    struct chlist *plist;
	if (save_restoreDebug >= 10) {
		logMsg("on_change_save: event = 0x%x, event.usr=0x%x\n",
			event, event.usr);
	}
    plist = (struct chlist *) event.usr;

    plist->save_state |= CHANGE;
}


/*
 * manual_save - save routine
 *
 */
int manual_save(char *request_file)
{
	struct chlist	*plist;

	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while ((plist != 0) && strcmp(plist->reqFile, request_file)) {
		plist = plist->pnext;
	}
	if (plist != 0)
		plist->save_state |= MANUAL;
	else
		errlogPrintf("saveset %s not found", request_file);
	semGive(sr_mutex);
	return(OK);
}

STATIC int mountFileSystem()
{
	errlogPrintf("save_restore:mountFileSystem:entry\n");
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && saveRestoreFilePath[0]) {
		if (hostGetByName(save_restoreNFSHostName) == ERROR) {
			(void)hostAdd(save_restoreNFSHostName, save_restoreNFSHostAddr);
		}
		if (nfsMount(save_restoreNFSHostName, saveRestoreFilePath, saveRestoreFilePath)==OK) {
			errlogPrintf("save_restore:mountFileSystem:successfully mounted '%s'\n", saveRestoreFilePath);
			save_restoreNFSOK = 1;
			save_restoreIoErrors = 0;
			strncpy(save_restoreRecentlyStr, "nfsMount succeeded",39);
			return(1);
		} else {
			errlogPrintf("save_restore: Can't nfsMount file system\n");
		}
	}
	strncpy(save_restoreRecentlyStr, "nfsMount failed",39);
	return(0);
}

STATIC void dismountFileSystem()
{
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && saveRestoreFilePath[0]) {
		nfsUnmount(saveRestoreFilePath);
		errlogPrintf("save_restore:dismountFileSystem:dismounted '%s'\n", saveRestoreFilePath);
		save_restoreNFSOK = 0;
		strncpy(save_restoreRecentlyStr, "nfsUnmount",39);
	}
}

/*
 * save_restore task
 *
 * all requests are flagged in the common data store -
 * they are satisfied here - with all channel access contained within
 */
STATIC int save_restore(void)
{
	struct chlist	*plist;
	char *cp;
	static size_t	buflen = 39;
	int i, do_seq_check, just_remounted;
	long status;
	time_t currTime, last_seq_check = time(NULL), remount_check_time = time(NULL);

	if (save_restoreDebug)
			errlogPrintf("save_restore:save_restore: entry status_prefix='%s'\n", status_prefix);

	ca_task_initialize();

	if (save_restoreNFSOK == 0) mountFileSystem();

	/* Build names for save_restore general status PV's with this prefix */
	if (*status_prefix && (*save_restoreStatus_PV == '\0')) {
		strcpy(save_restoreStatus_PV, status_prefix);
		strcat(save_restoreStatus_PV, "save_restoreStatus");
		strcpy(save_restoreHeartbeat_PV, status_prefix);
		strcat(save_restoreHeartbeat_PV, "save_restoreHeartbeat");
		strcpy(save_restoreStatusStr_PV, status_prefix);
		strcat(save_restoreStatusStr_PV, "save_restoreStatusStr");
		strcpy(save_restoreRecentlyStr_PV, status_prefix);
		strcat(save_restoreRecentlyStr_PV, "save_restoreRecentlyStr");
		ca_search(save_restoreStatus_PV, &save_restoreStatus_chid);
		ca_search(save_restoreHeartbeat_PV, &save_restoreHeartbeat_chid);
		ca_search(save_restoreStatusStr_PV, &save_restoreStatusStr_chid);
		ca_search(save_restoreRecentlyStr_PV, &save_restoreRecentlyStr_chid);

		strcpy(reboot_restoreStatus_PV, status_prefix);
		strcat(reboot_restoreStatus_PV, "reboot_restoreStatus");
		strcpy(reboot_restoreStatusStr_PV, status_prefix);
		strcat(reboot_restoreStatusStr_PV, "reboot_restoreStatusStr");
		strcpy(reboot_restoreTime_PV, status_prefix);
		strcat(reboot_restoreTime_PV, "reboot_restoreTime");
		ca_search(reboot_restoreStatus_PV, &reboot_restoreStatus_chid);
		ca_search(reboot_restoreStatusStr_PV, &reboot_restoreStatusStr_chid);
		ca_search(reboot_restoreTime_PV, &reboot_restoreTime_chid);
		if (ca_pend_io(0.5)!=ECA_NORMAL) {
			errlogPrintf("save_restore: Can't connect to all status PV(s)\n");
		}
		/* Show reboot status */
		reboot_restoreStatus = SR_STATUS_OK;
		strcpy(reboot_restoreStatusStr, "Ok");
		for (i = 0; i < restoreFileList.pass0cnt; i++) {
			if (restoreFileList.pass0Status[i] < reboot_restoreStatus) {
				reboot_restoreStatus = restoreFileList.pass0Status[i];
				strncpy(reboot_restoreStatusStr, restoreFileList.pass0StatusStr[i], 39);
			}
		}
		for (i = 0; i < restoreFileList.pass1cnt; i++) {
			if (restoreFileList.pass1Status[i] < reboot_restoreStatus) {
				reboot_restoreStatus = restoreFileList.pass1Status[i];
				strncpy(reboot_restoreStatusStr, restoreFileList.pass1StatusStr[i], 39);
			}
		}
		if (ca_state(reboot_restoreStatus_chid) == cs_conn)
			ca_put(DBR_LONG, reboot_restoreStatus_chid, &reboot_restoreStatus);
		if (ca_state(reboot_restoreStatusStr_chid) == cs_conn)
			ca_put(DBR_STRING, reboot_restoreStatusStr_chid, &reboot_restoreStatusStr);
		currTime = time(NULL);
		(void)ctime_r(&currTime, reboot_restoreTimeStr, &buflen);
		if ((cp = strrchr(reboot_restoreTimeStr, (int)':'))) cp[3] = 0;
		if (ca_state(reboot_restoreTime_chid) == cs_conn)
			ca_put(DBR_STRING, reboot_restoreTime_chid, &reboot_restoreTimeStr);
	}

	while(1) {

		save_restoreStatus = SR_STATUS_OK;
		strcpy(save_restoreStatusStr, "Ok");
		save_restoreSeqPeriodInSeconds = MAX(10, save_restoreSeqPeriodInSeconds);
		save_restoreNumSeqFiles = MIN(10, MAX(0, save_restoreNumSeqFiles));
		do_seq_check = (difftime(time(NULL), last_seq_check) > save_restoreSeqPeriodInSeconds/2);

		just_remounted = 0;
		if ((save_restoreNFSOK == 0) && save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0]) {
			if (difftime(time(NULL), remount_check_time) > 60) {
				dismountFileSystem();
				just_remounted = mountFileSystem();
				remount_check_time = time(NULL);
			}
		}

		/* look at each list */
		semTake(sr_mutex,WAIT_FOREVER);
		plist = lptr;
		while (plist != 0) {
			if (save_restoreDebug >= 30)
				errlogPrintf("save_restore: '%s' save_state = 0x%x\n",
					plist->reqFile, plist->save_state);
			/* connect the channels on the first instance of this set */
			/*
			 * We used to call enable_list() from create_data_set(), if the
			 * list already existed and was just getting a new method.  In that
			 * case, we'd only enable new lists (with enabled_method==0) here.
			 * Now, we want to avoid having the task that called create_data_set()
			 * setting up CA monitors that we're going to have to manage, so we
			 * make all the calls to enable_list().
			 */ 
			/* if (plist->enabled_method == 0) { */
			if (plist->enabled_method != plist->save_method) {
				plist->not_connected = connect_list(plist);
				enable_list(plist);
			}

			/*
			 * Save lists that have triggered.  If we've just successfully remounted,
			 * save lists that are in failure.
			 */
			if ( (plist->save_state & SINGLE_EVENTS)
			  || ((plist->save_state & MONITORED) == MONITORED)
			  || ((plist->status == SR_STATUS_FAIL) && just_remounted)) {

				/* fetch all of the channels */
				plist->not_connected = get_channel_list(plist);

				/* write the data to disk */
				if ((plist->not_connected == 0) || (save_restoreIncompleteSetsOk))
					write_save_file(plist);
			}

			/*** Periodically make sequenced backup of most recent saved file ***/
			if (do_seq_check) {
				if (save_restoreNumSeqFiles && plist->last_save_file &&
					(difftime(time(NULL), plist->backup_time) > save_restoreSeqPeriodInSeconds)) {
					do_seq(plist);
				}
				last_seq_check = time(NULL);
			}

			/*** restart timers and reset save requests ***/
			if (plist->save_state & PERIODIC) {
				if (wdStart(plist->saveWdId, plist->period, periodic_save, (int)plist) < 0) {
					errlogPrintf("could not add %s to the period scan", plist->reqFile);
				}
			}
			if (plist->save_state & SINGLE_EVENTS) {
				/* Note that this clears PERIODIC, TRIGGERED, and MANUAL bits */
				plist->save_state = plist->save_state & ~SINGLE_EVENTS;
			}
			if ((plist->save_state & MONITORED) == MONITORED) {
				if(wdStart(plist->saveWdId, plist->monitor_period, on_change_timer, (int)plist)
				  < 0 ) {
					errlogPrintf("could not add %s to the period scan", plist->reqFile);
				}
				plist->save_state = plist->save_state & ~MONITORED;
			}

			/* find worst status */
			if (plist->status <= save_restoreStatus ) {
				save_restoreStatus = plist->status;
				strcpy(save_restoreStatusStr, plist->statusStr);
			}
			if (reboot_restoreStatus < save_restoreStatus) {
				save_restoreStatus = reboot_restoreStatus;
				strcpy(save_restoreStatusStr, reboot_restoreStatusStr);
			}

			/* next list */
			plist = plist->pnext;
		}

		/* release the list */
		semGive(sr_mutex);

		/* report status */
		save_restoreHeartbeat = (save_restoreHeartbeat+1) % 2;
		if (ca_state(save_restoreStatus_chid) == cs_conn)
			ca_put(DBR_LONG, save_restoreStatus_chid, &save_restoreStatus);
		if (ca_state(save_restoreHeartbeat_chid) == cs_conn)
			ca_put(DBR_LONG, save_restoreHeartbeat_chid, &save_restoreHeartbeat);
		if (ca_state(save_restoreStatusStr_chid) == cs_conn)
			ca_put(DBR_STRING, save_restoreStatusStr_chid, &save_restoreStatusStr);
		if (ca_state(save_restoreRecentlyStr_chid) == cs_conn) {
			save_restoreRecentlyStr[39] = '\0';
			status = ca_put(DBR_STRING, save_restoreRecentlyStr_chid, &save_restoreRecentlyStr);
		}

		for (plist = lptr; plist; plist = plist->pnext) {
			/* If this is the first time for a list, connect to its status PV's */
			if (plist->status_PV[0] == '\0') {
				/* Build PV names */
				strcpy(plist->status_PV, status_prefix);
				strncat(plist->status_PV, plist->save_file, 39-strlen(status_prefix));
				cp = strrchr(plist->status_PV, (int)'.');
				*cp = 0;
				strcpy(plist->state_PV, plist->status_PV);
				strcpy(plist->statusStr_PV, plist->status_PV);
				strcpy(plist->time_PV, plist->status_PV);
				strncat(plist->status_PV, "Status",
					39-strlen(status_prefix)-(strlen(plist->save_file)-4));
				strncat(plist->state_PV, "State",
					39-strlen(status_prefix)-(strlen(plist->save_file)-4));
				strncat(plist->statusStr_PV, "StatusStr",
					39-strlen(status_prefix)-(strlen(plist->save_file)-4));
				strncat(plist->time_PV, "Time",
					39-strlen(status_prefix)-(strlen(plist->save_file)-4));
				ca_search(plist->status_PV, &plist->status_chid);
				ca_search(plist->state_PV, &plist->state_chid);
				ca_search(plist->statusStr_PV, &plist->statusStr_chid);
				ca_search(plist->time_PV, &plist->time_chid);
				if (ca_pend_io(0.5)!=ECA_NORMAL) {
					errlogPrintf("Can't connect to status PV(s) for list '%s'\n", plist->save_file);
				}
			}

			if (ca_state(plist->status_chid) == cs_conn)
				ca_put(DBR_LONG, plist->status_chid, &plist->status);
			if (ca_state(plist->state_chid) == cs_conn)
				ca_put(DBR_LONG, plist->state_chid, &plist->save_state);
			if (ca_state(plist->statusStr_chid) == cs_conn)
				ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			if (plist->status >= SR_STATUS_WARN) {
				(void)ctime_r(&plist->save_time, plist->timeStr, &buflen);
				if ((cp = strrchr(plist->timeStr, (int)':'))) cp[3] = 0;
				if (ca_state(plist->time_chid) == cs_conn)
					ca_put(DBR_STRING, plist->time_chid, &plist->timeStr);
			}
		}

		if (remove_dset) {
			if ((remove_status = remove_data_set(remove_filename))) {
				errlogPrintf("error removing %s\n", remove_filename);
			}
			remove_filename[0] = 0;
			remove_dset = 0;
			semGive(sem_remove);
		}

		/* go to sleep for a while */
		ca_pend_event((double)min_delay);
    }
	return(OK);
}
	

/*
 * connect all of the channels in a save set
 *
 * NOTE: Assumes that the sr_mutex is taken
 */
STATIC int connect_list(struct chlist *plist)
{
	struct channel	*pchannel;

	/* connect all channels in the list */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		if (save_restoreDebug >= 10)
			errlogPrintf("save_restore:connect_list: channel '%s'\n", pchannel->name);
		if (ca_search(pchannel->name,&pchannel->chid) == ECA_NORMAL) {
			strcpy(pchannel->value,"Search Issued");
		} else {
			strcpy(pchannel->value,"Search Not Issued");
		}
	}
	if (ca_pend_io(5.0) == ECA_TIMEOUT) {
		errlogPrintf("not all searches successful\n");
	}
		
	sprintf(save_restoreRecentlyStr, "list '%s' connected", plist->save_file);
	return(get_channel_list(plist));
}


/*
 * enable new save methods
 *
 * NOTE: Assumes the sr_mutex is taken
 */
STATIC int enable_list(struct chlist *plist)
{
	struct channel	*pchannel;
	chid 			chid;			/* channel access id */

	DebugNV(4, "enable_list: entry\n");
	/* enable a periodic set */
	if ((plist->save_method & PERIODIC) && !(plist->enabled_method & PERIODIC)) {
		if (wdStart(plist->saveWdId, plist->period, periodic_save, (int)plist) < 0) {
			errlogPrintf("could not add %s to the period scan", plist->reqFile);
		}
		plist->enabled_method |= PERIODIC;
	}

	/* enable a triggered set */
	if ((plist->save_method & TRIGGERED) && !(plist->enabled_method & TRIGGERED)) {
		if (ca_search(plist->trigger_channel, &chid) != ECA_NORMAL) {
			errlogPrintf("trigger %s search failed\n", plist->trigger_channel);
		} else if (ca_pend_io(2.0) != ECA_NORMAL) {
			errlogPrintf("timeout on search of %s\n", plist->trigger_channel);
		} else if (ca_state(chid) != cs_conn) {
			errlogPrintf("trigger %s search not connected\n", plist->trigger_channel);
		} else if (ca_add_event(DBR_FLOAT, chid, triggered_save, (void *)plist, 0) !=ECA_NORMAL) {
			errlogPrintf("trigger event for %s failed\n", plist->trigger_channel);
		} else{
			plist->enabled_method |= TRIGGERED;
		}
	}

	/* enable a monitored set */
	if ((plist->save_method & MONITORED) && !(plist->enabled_method & MONITORED)) {
		for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
			Debug(10, "enable_list: calling ca_add_event for '%s'\n", pchannel->name);
			if (save_restoreDebug >= 10) errlogPrintf("enable_list: arg = 0x%x\n", plist);
			/*
			 * Work around obscure problem affecting USHORTS by making DBR type different
			 * from any possible field type.  This avoids tickling a bug that causes dbGet
			 * to overwrite the source field with its own value converted to LONG.
			 * (Changed DBR_LONG to DBR_TIME_LONG.)
			 */
			if (ca_add_event(DBR_TIME_LONG, pchannel->chid, on_change_save,
					(void *)plist, 0) != ECA_NORMAL) {
				errlogPrintf("could not add event for %s in %s\n",
					pchannel->name,plist->reqFile);
			}
		}
		DebugNV(4 ,"enable_list: done calling ca_add_event for list channels\n");
		if (ca_pend_io(5.0) != ECA_NORMAL) {
			errlogPrintf("timeout on monitored set: %s to monitored scan\n",plist->reqFile);
		}
		if (wdStart(plist->saveWdId, plist->monitor_period, on_change_timer, (int)plist) < 0) {
			errlogPrintf("watchdog for set %s not started\n",plist->reqFile);
		}
		plist->enabled_method |= MONITORED;
	}

	/* enable a manual request set */
	if ((plist->save_method & MANUAL) && !(plist->enabled_method & MANUAL)) {
		plist->enabled_method |= MANUAL;
	}

	sprintf(save_restoreRecentlyStr, "list '%s' enabled", plist->save_file);
	return(OK);
}


/*
 * fetch all channels in the save set
 *
 * NOTE: Assumes sr_mutex is taken
 */
#define INIT_STRING "!@#$%^&*()"
STATIC int get_channel_list(struct chlist *plist)
{

	struct channel *pchannel;
	int				not_connected = 0;
	unsigned short	num_channels = 0;

	/* attempt to fetch all channels that are connected */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		pchannel->valid = 0;
		if (ca_state(pchannel->chid) == cs_conn) {
			num_channels++;
			strcpy(pchannel->value, INIT_STRING);
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,(float *)pchannel->value);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,(double *)pchannel->value);
			} else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			if (ca_field_type(pchannel->chid) == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
			pchannel->valid = 1;
		} else {
			not_connected++;
		}
	}
	if (ca_pend_io(MIN(10.0, .1*num_channels)) != ECA_NORMAL) {
		errlogPrintf("get_channel_list: not all gets completed");
		not_connected++;
	}

	/* convert floats and doubles, check to see which gets completed */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				sprintf(pchannel->value, FLOAT_FMT, *(float *)pchannel->value);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				sprintf(pchannel->value, DOUBLE_FMT, *(double *)pchannel->value);
			}
			/* then we at least had a CA connection.  Did it produce? */
			pchannel->valid = strcmp(pchannel->value, INIT_STRING);
		}
	}

	return(not_connected);
}

/* Actually write the file
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
STATIC int write_it(char *filename, struct chlist *plist)
{
	FILE 			*out_fd;
	struct channel	*pchannel;
	int 			n, i;
	char			datetime[32];

	errno = 0;
	if ((out_fd = fopen(filename,"w")) == NULL) {
		errlogPrintf("save_restore:write_it - unable to open file '%s'\n", filename);
		if (errno) myPrintErrno(errno);
		if (++save_restoreIoErrors > save_restoreRemountThreshold) {
			save_restoreNFSOK = 0;
			strncpy(save_restoreRecentlyStr, "Too many I/O errors",39);
		}
		return(ERROR);
	}
	fGetDateStr(datetime);
	errno = 0;
	n = fprintf(out_fd,"# %s\tAutomatically generated - DO NOT MODIFY - %s\n",
			SRversion, datetime);
	if (n <= 0 || errno) {
		if (n <= 0) errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
		if (errno) myPrintErrno(errno);
		goto trouble;
	}
	if (plist->not_connected) {
		errno = 0;
		n = fprintf(out_fd,"! %d channel(s) not connected - or not all gets were successful\n",
			plist->not_connected);
			if (n <= 0 || errno) {
				if (n <= 0) errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
				if (errno) myPrintErrno(errno);
				goto trouble;
			}
	}
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		errno = 0;
		if (pchannel->valid) {
			n = fprintf(out_fd, "%s ", pchannel->name);
		} else {
			n = fprintf(out_fd, "#%s ", pchannel->name);
		}
		if (n <= 0 || errno) {
			if (n <= 0) errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
			if (errno) myPrintErrno(errno);
			goto trouble;
		}

		errno = 0;
		if (pchannel->enum_val >= 0) {
			n = fprintf(out_fd, "%d\n",pchannel->enum_val);
		} else {
			n = fprintf(out_fd, "%-s\n", pchannel->value);
		}
		if (n <= 0 || errno) {
			if (n <= 0) errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
			if (errno) myPrintErrno(errno);
			goto trouble;
		}
	}
#if 0
	if (save_restoreDebug == 999) {
		errlogPrintf("save_restore: simulating task crash.  Bye, bye!\n");
		exit(-1);
	}
#endif

	errno = 0;
	n = fprintf(out_fd, "<END>\n");
	if (n <= 0 || errno) {
		if (n <= 0) errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
		if (errno) myPrintErrno(errno);
		goto trouble;
	}

	errno = 0;
	n = fflush(out_fd);
	if (n != 0 || errno) {
		if (n != 0) errlogPrintf("save_restore:write_it: fflush returned %d\n", n);
		if (errno) myPrintErrno(errno);
	}

	errno = 0;
#ifdef vxWorks
	n = ioctl(fileno(out_fd),FIOSYNC,0);	/* NFS flush to disk */
	if (n == ERROR) errlogPrintf("save_restore: ioctl(,FIOSYNC,) returned %d\n", n);
#else
	n = fsync(fileno(out_fd));
	if (n != 0) errlogPrintf("save_restore:write_it: fsync returned %d\n", n);
#endif
	if (errno) myPrintErrno(errno);

	errno = 0;
	n = fclose(out_fd);
	if (n != 0 || errno) {
		if (n != 0) errlogPrintf("save_restore:write_it: fclose returned %d\n", n);
		if (errno) myPrintErrno(errno);
		goto trouble;
	}
	return(OK);

trouble:
	/* try to close file */
	for (i=1, n=1; n && i<100;i++) {
		n = fclose(out_fd);
		if (n && ((i%10)==0)) {
			errlogPrintf("save_restore:write_it: fclose('%s') returned %d (%d tries)\n",
				plist->save_file, n, i);
			taskDelay(600);
		}
	}
	if (i >= 60) {
		errlogPrintf("save_restore:write_it: Can't close '%s'; giving up.\n", plist->save_file);
	}
	return(ERROR);
}


/* state of a backup restore file */
#define BS_NONE 	0	/* Couldn't open the file */
#define BS_BAD		1	/* File exists but looks corrupted */
#define BS_OK		2	/* File is good */
#define BS_NEW		3	/* Just wrote the file */

STATIC int check_file(char *file)
{
	FILE *fd;
	char tmpstr[20];
	int	 file_state = BS_NONE;

	if ((fd = fopen(file, "r")) != NULL) {
		if ((fseek(fd, -6, SEEK_END)) ||
			(fgets(tmpstr, 6, fd) == 0) ||
			(strncmp(tmpstr, "<END>", 5) != 0)) {
				file_state = BS_BAD;
		}
		fclose(fd);
		file_state = BS_OK;
	}
	return(file_state);
}


/*
 * save_file - save routine
 *
 * Write a list of channel names and values to an ASCII file.
 *
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
STATIC int write_save_file(struct chlist *plist)
{
	char	save_file[PATH_SIZE+3] = "", backup_file[PATH_SIZE+3] = "";
	char	tmpstr[PATH_SIZE+50];
	int		backup_state = BS_OK;
	char	datetime[32];

	plist->status = SR_STATUS_OK;
	strcpy(plist->statusStr, "Ok");

	/* Make full file names */
	strncpy(save_file, saveRestoreFilePath, sizeof(save_file) - 1);
	strncat(save_file, plist->save_file, MAX(0, sizeof(save_file) - 1 - strlen(save_file)));
	strcpy(backup_file, save_file);
	strncat(backup_file, "B", 1);

	/* Ensure that backup is ok before we overwrite .sav file. */
	backup_state = check_file(backup_file);
	if (backup_state != BS_OK) {
		errlogPrintf("write_save_file: Backup file (%s) bad or not found.  Writing a new one.\n", 
			backup_file);
		if (backup_state == BS_BAD) {
			/* make a backup copy of the corrupted file */
			strcpy(tmpstr, backup_file);
			strcat(tmpstr, "_SBAD_");
			if (save_restoreDatedBackupFiles) {
				fGetDateStr(datetime);
				strcat(tmpstr, datetime);
				sprintf(save_restoreRecentlyStr, "Bad file: '%sB'", plist->save_file);
			}
			(void)myFileCopy(backup_file, tmpstr);
		}
		if (write_it(backup_file, plist) == ERROR) {
			errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
			errlogPrintf("save_restore:write_save_file: Can't write new backup file.  I quit.\n");
			errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
			plist->status = SR_STATUS_FAIL;
			strcpy(plist->statusStr, "Can't write .savB file");
			return(ERROR);
		}
		plist->status = SR_STATUS_WARN;
		strcpy(plist->statusStr, ".savB file was bad");
		backup_state = BS_NEW;
	}

	/*** Write the save file ***/
	Debug(1, "write_save_file: saving to %s\n", save_file);
	if (write_it(save_file, plist) == ERROR) {
		errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		errlogPrintf("save_restore:write_save_file: Can't write save file.  I quit.\n");
		errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		plist->status = SR_STATUS_FAIL;
		strcpy(plist->statusStr, "Can't write .sav file");
		sprintf(save_restoreRecentlyStr, "Can't write '%s'", plist->save_file);
		return(ERROR);
	}

	/* keep the name and time of the last saved file */
	(void)time(&plist->save_time);
	strcpy(plist->last_save_file, plist->save_file);

	/*** Write a backup copy of the save file ***/
	if (backup_state != BS_NEW) {
		/* make a backup copy */
		if (myFileCopy(save_file, backup_file) != OK) {
			errlogPrintf("save_restore:write_save_file - Couldn't make backup '%s'\n",
				backup_file);
			plist->status = SR_STATUS_WARN;
			strcpy(plist->statusStr, "Can't write .savB file");
			sprintf(save_restoreRecentlyStr, "Can't write '%sB'", plist->save_file);
			return(ERROR);
		}
	}
	if (plist->not_connected) {
		plist->status = SR_STATUS_WARN;
		sprintf(plist->statusStr,"%d %s not saved", plist->not_connected, 
			plist->not_connected==1?"value":"values");
	}
	sprintf(save_restoreRecentlyStr, "Wrote '%s'", plist->save_file);
	return(OK);
}


/*
 * do_seq - copy .sav file to .savX, where X is in
 * [0..save_restoreNumSeqFiles].  If .sav file can't be opened,
 * write .savX file explicitly, as we would write .sav file.
 *
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
STATIC void do_seq(struct chlist *plist)
{
	char	*p, save_file[PATH_SIZE+3] = "", backup_file[PATH_SIZE+3] = "";
	int		i;
	struct stat fileStat;

	/* Make full file names */
	strncpy(save_file, saveRestoreFilePath, sizeof(save_file) - 1);
	strncat(save_file, plist->save_file, MAX(0, sizeof(save_file) - 1 - strlen(save_file)));
	strcpy(backup_file, save_file);
	p = &backup_file[strlen(backup_file)];

	/* If first time for this list, determine which existing file is oldest. */
	if (plist->backup_sequence_num == -1) {
		double dTime, max_dTime = -1.e9;

		plist->backup_sequence_num = 0;
		for (i=0; i<save_restoreNumSeqFiles; i++) {
			sprintf(p, "%1d", i);	/* (over)write sequence number */
			if (stat(backup_file, &fileStat)) {
				/* can't check date; just assume this file is oldest */
				plist->backup_sequence_num = i;
				break;
			}
			dTime = difftime(time(NULL), fileStat.st_mtime);
			if (dTime > max_dTime) {
				max_dTime = dTime;
				plist->backup_sequence_num = i;
			}
		}
	}

	if (check_file(save_file) == BS_NONE) {
		errlogPrintf("save_restore:do_seq - '%s' not found.  Writing a new one.\n",
			save_file);
		(void) write_save_file(plist);
	}
	sprintf(p, "%1d", plist->backup_sequence_num);
	if (myFileCopy(save_file, backup_file) != OK) {
		errlogPrintf("save_restore:do_seq - Can't copy save file to '%s'\n",
			backup_file);
		if (write_it(backup_file, plist) == ERROR) {
			errlogPrintf("save_restore:do_seq - Can't write seq. file from PV list.\n");
			if (plist->status >= SR_STATUS_WARN) {
				plist->status = SR_STATUS_SEQ_WARN;
				strcpy(plist->statusStr, "Can't write sequence file");
			}
			sprintf(save_restoreRecentlyStr, "Can't write '%s%1d'",
				plist->save_file, plist->backup_sequence_num);
			return;
		} else {
			errlogPrintf("save_restore:do_seq: Wrote seq. file from PV list.\n");
			sprintf(save_restoreRecentlyStr, "Wrote '%s%1d'",
				plist->save_file, plist->backup_sequence_num);
		}
	} else {
		sprintf(save_restoreRecentlyStr, "Wrote '%s%1d'",
			plist->save_file, plist->backup_sequence_num);
	}

	plist->backup_time = time(NULL);
	if (++(plist->backup_sequence_num) >=  save_restoreNumSeqFiles)
		plist->backup_sequence_num = 0;
}


int set_savefile_name(char *filename, char *save_filename)
{
	struct chlist	*plist;

	/* is save set defined - add new save mode if necessary */
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->reqFile,filename)) {
			strcpy(plist->save_file,save_filename);
			semGive(sr_mutex);
			sprintf(save_restoreRecentlyStr, "New save file: '%s'", save_filename);
			return(OK);
		}
		plist = plist->pnext;
	}
	errlogPrintf("No save set enabled for %s\n",filename);
	semGive(sr_mutex);
	return(ERROR);
}


int create_periodic_set(char *filename, int period, char *macrostring)
{
	return(create_data_set(filename, PERIODIC, period, 0, 0, macrostring));
}


int create_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	if (trigger_channel && (isalpha(trigger_channel[0]) || isdigit(trigger_channel[0]))) {
		return(create_data_set(filename, TRIGGERED, 0, trigger_channel, 0, macrostring));
	}
	else {
		errlogPrintf("create_triggered_set: Error: trigger-channel name is required.\n");
		return(ERROR);
	}
}


int create_monitor_set(char *filename, int period, char *macrostring)
{
	return(create_data_set(filename, MONITORED, 0, 0, period, macrostring));
}


int create_manual_set(char *filename, char *macrostring)
{
	return(create_data_set(filename, MANUAL, 0, 0, 0, macrostring));
}


/*
 * create a data set
 */
STATIC int create_data_set(
	char	*filename,			/* save set request file */
	int		save_method,
	int		period,				/* maximum time between saves  */
	char	*trigger_channel,	/* db channel to trigger save  */
	int		mon_period,			/* minimum time between saves  */
	char	*macrostring
)
{
	struct chlist	*plist;
	int				inx;			/* i/o status 	       */
	ULONG			ticks;
	int				per_ticks, mon_ticks;
	char			*dot;

	if (save_restoreDebug) {
		errlogPrintf("create_data_set: file '%s', method %x, period %d, trig_chan '%s', mon_period %d\n",
			filename, save_method, period, trigger_channel ? trigger_channel : "NONE", mon_period);
	}

	/* initialize save_restore routines */
	if (!save_restore_init) {
		if ((sr_mutex = semMCreate(SEM_Q_FIFO)) == 0) {
			errlogPrintf("create_data_set: could not create list header mutex");
			return(ERROR);
		}
		if ((sem_remove = semBCreate(SEM_Q_FIFO, SEM_EMPTY)) == 0) {
			errlogPrintf("create_data_set: could not create delete list semaphore\n");
			return(ERROR);
		}
		if ((taskID = taskSpawn("save_restore",taskPriority,VX_FP_TASK,
			10000, save_restore,0,0,0,0,0,0,0,0,0,0))
		  == ERROR) {
			errlogPrintf("create_data_set: could not create save_restore task");
			return(ERROR);
		}

		save_restore_init = 1;
	}

	/* Convert periods into clock ticks */
	per_ticks = MAX(period, min_period) * vxTicksPerSecond;
	mon_ticks = MAX(mon_period, min_period) * vxTicksPerSecond;

	/* is save set defined - add new save mode if necessary */
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->reqFile,filename)) {
			if (plist->save_method & save_method) {
				semGive(sr_mutex);
				errlogPrintf("create_data_set: '%s' already in %x mode",filename,save_method);
				return(ERROR);
			}else{
				if (save_method == TRIGGERED)
					if (trigger_channel) {
						strcpy(plist->trigger_channel,trigger_channel);
					} else {
						errlogPrintf("create_data_set: no trigger channel");
						semGive(sr_mutex);
						return(ERROR);
					}
				else if (save_method == PERIODIC)
					plist->period = per_ticks;
				else if (save_method == MONITORED)
					plist->monitor_period = mon_ticks;
				plist->save_method |= save_method;
				/*
				 * We don't really want to call enable_list() from here, because
				 * it starts CA monitors that will be handled by the save_restore
				 * task.  It seems better to have the save_restore task do all
				 * of its own CA stuff.
				 */
				/* enable_list(plist); */

				semGive(sr_mutex);
				return(OK);
			}
		}
		plist = plist->pnext;
	}
	semGive(sr_mutex);

	/* create a new channel list */
	ca_task_initialize();
	if ((plist = (struct chlist *)calloc(1,sizeof (struct chlist)))
	  == (struct chlist *)0) {
		errlogPrintf("create_data_set: channel list calloc failed");
		return(ERROR);
	}
	if ((plist->saveWdId = wdCreate()) == 0) {
		errlogPrintf("create_data_set: could not create watchdog");
		return(ERROR);
	}
	strncpy(plist->reqFile, filename, sizeof(plist->reqFile)-1);
	plist->pchan_list = (struct channel *)0;
	plist->period = per_ticks;
	if (trigger_channel) {
	    strncpy(plist->trigger_channel,trigger_channel, sizeof(plist->trigger_channel)-1);
	} else {
	    plist->trigger_channel[0]=0;
	}
	plist->last_save_file[0] = 0;
	plist->save_method = save_method;
	plist->enabled_method = 0;
	plist->save_state = 0;
	plist->save_ok = 0;
	plist->monitor_period = mon_ticks;
	plist->backup_time = time(NULL);
	plist->backup_sequence_num = -1;
	plist->status = SR_STATUS_FAIL;
	strcpy(plist->statusStr,"Initializing list");
	/* save_file name */
	strcpy(plist->save_file, plist->reqFile);

	dot = strrchr(plist->save_file, '.');
	if (dot)
	   inx = dot - plist->save_file;
	else
	   inx = strlen(plist->save_file);

	plist->save_file[inx] = 0;	/* truncate if necessary to leave room for ".sav" + null */
	strcat(plist->save_file,".sav");

	/* read the request file and populate plist with the PV names */
	if (readReqFile(plist->reqFile, plist, macrostring) == ERROR) {
		free(plist);
		return(ERROR);
	}

	/* link it to the save set list */
	semTake(sr_mutex,WAIT_FOREVER);
	plist->pnext = lptr;
	lptr = plist;
	semGive(sr_mutex);

	/* this should be absolute time as a string */
	ticks = tickGet();

	return(OK);
}


/*
 * fdbrestore - restore routine
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database vaules.
 *
 */
int fdbrestore(char *filename)
{	
	struct channel	*pchannel;
	struct chlist	*plist;
	int				found;
	char			channel[80];
	char			restoreFile[PATH_SIZE+1] = "";
	char			bu_filename[PATH_SIZE+1] = "";
	char			buffer[120], *bp, c;
	char			input_line[120];
	int				n;
	long			status;
	FILE			*inp_fd;
	chid			chanid;

	/* if this is the current file name for a save set - restore from there */
	found = FALSE;
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while ((plist != 0) && !found) { 
		if (strcmp(plist->last_save_file,filename) == 0) {
			found = TRUE;
		}else{
			plist = plist->pnext;
		}
	}
	if (found ) {
		/* verify quality of the save set */
		if (plist->not_connected > 0) {
			errlogPrintf("fdbrestore: %d channel(s) not connected or fetched\n",plist->not_connected);
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("fdbrestore: aborting restore\n");
				semGive(sr_mutex);
				strncpy(save_restoreRecentlyStr, "Manual restore failed",39);
				return(ERROR);
			}
		}

		for (pchannel = plist->pchan_list; pchannel !=0; pchannel = pchannel->pnext) {
			ca_put(DBR_STRING,pchannel->chid,pchannel->value);
		}
		if (ca_pend_io(1.0) != ECA_NORMAL) {
			errlogPrintf("fdbrestore: not all channels restored\n");
		}
		semGive(sr_mutex);
		strncpy(save_restoreRecentlyStr, "Manual restore succeeded",39);
		return(OK);
	}
	semGive(sr_mutex);

	/* open file */
	strncpy(restoreFile, saveRestoreFilePath, sizeof(restoreFile) - 1);
	strncat(restoreFile, filename, MAX(sizeof(restoreFile) -1 - strlen(restoreFile),0));
	if ((inp_fd = fopen_and_check(restoreFile, "r", &status)) == NULL) {
		errlogPrintf("fdbrestore: Can't open save file.");
		strncpy(save_restoreRecentlyStr, "Manual restore failed",39);
		return(ERROR);
	}

	(void)fgets(buffer, 120, inp_fd); /* discard header line */
	/* restore from data file */
	while ((bp=fgets(buffer, 120, inp_fd))) {
		/* get PV_name, one space character, value */
		/* (value may be a string with leading whitespace; it may be */
		/* entirely whitespace; the number of spaces may be crucial; */
		/* it might also consist of zero characters) */
		n = sscanf(bp,"%s%c%[^\n]", channel, &c, input_line);
		if (n < 3) *input_line = 0;
		if (isalpha(channel[0]) || isdigit(channel[0])) {
			if (ca_search(channel, &chanid) != ECA_NORMAL) {
				errlogPrintf("ca_search for %s failed\n", channel);
			} else if (ca_pend_io(0.5) != ECA_NORMAL) {
				errlogPrintf("ca_search for %s timeout\n", channel);
			} else if (ca_put(DBR_STRING, chanid, input_line) != ECA_NORMAL) {
				errlogPrintf("ca_put of %s to %s failed\n", input_line,channel);
			}
		} else if (channel[0] == '!') {
			n = atoi(&channel[1]);
			errlogPrintf("%d channel%c not connected / fetch failed", n, (n==1) ? ' ':'s');
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("aborting restore\n");
				fclose(inp_fd);
				strncpy(save_restoreRecentlyStr, "Manual restore failed",39);
				return(ERROR);
			}
		}
	}
	fclose(inp_fd);

	/* make  backup */
	strcpy(bu_filename,filename);
	strcat(bu_filename,".bu");
	(void)myFileCopy(restoreFile,bu_filename);
	strncpy(save_restoreRecentlyStr, "Manual restore succeeded",39);
	return(OK);
}


/*
 * save_restoreShow -  Show state of save_restore; optionally, list save sets
 *
 */
void save_restoreShow(int verbose)
{
	struct chlist	*plist;
	struct channel 	*pchannel;
	struct pathListElement *p = reqFilePathList;
	char tmpstr[50];
	int NFS_managed = save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && 
		saveRestoreFilePath[0];

	printf("BEGIN save_restoreShow\n");
	printf("  Status: '%s' - '%s'\n", SR_STATUS_STR[save_restoreStatus], save_restoreStatusStr);
	printf("  Debug level: %d\n", save_restoreDebug);
	printf("  Save/restore incomplete save sets? %s\n", save_restoreIncompleteSetsOk?"YES":"NO");
	printf("  Write dated backup files? %s\n", save_restoreDatedBackupFiles?"YES":"NO");
	printf("  Number of sequence files to maintain: %d\n", save_restoreNumSeqFiles);
	printf("  Time interval between sequence files: %d seconds\n", save_restoreSeqPeriodInSeconds);
	printf("  NFS host: '%s'; address:'%s'\n", save_restoreNFSHostName, save_restoreNFSHostAddr);
	printf("  NFS mount status: %s\n",
		save_restoreNFSOK?"Ok":NFS_managed?"Failed":"not managed by save_restore");
	printf("  I/O errors: %d\n", save_restoreIoErrors);
	printf("  request file path list:\n");
	while (p) {
		printf("    '%s'\n", p->path);
		p = p->pnext;
	}
	printf("  save file path:\n    '%s'\n", saveRestoreFilePath);
	semTake(sr_mutex,WAIT_FOREVER);
	for (plist = lptr; plist != 0; plist = plist->pnext) {
		printf("    %s: \n",plist->reqFile);
		printf("    Status: '%s' - '%s'\n", SR_STATUS_STR[plist->status], plist->statusStr);
		printf("    Last save time  :%s", ctime(&plist->save_time));
		printf("    Last backup time:%s", ctime(&plist->backup_time));
		strcpy(tmpstr, "[ ");
		if (plist->save_method & PERIODIC) strcat(tmpstr, "PERIODIC ");
		if (plist->save_method & TRIGGERED) strcat(tmpstr, "TRIGGERED ");
		if ((plist->save_method & MONITORED)==MONITORED) strcat(tmpstr, "TIMER+CHANGE ");
		if (plist->save_method & MANUAL) strcat(tmpstr, "MANUAL ");
		strcat(tmpstr, "]");
		printf("    methods: %s\n", tmpstr);
		strcpy(tmpstr, "[ ");
		if (plist->save_state & PERIODIC) strcat(tmpstr, "PERIOD ");
		if (plist->save_state & TRIGGERED) strcat(tmpstr, "TRIGGER ");
		if (plist->save_state & TIMER) strcat(tmpstr, "TIMER ");
		if (plist->save_state & CHANGE) strcat(tmpstr, "CHANGE ");
		if (plist->save_state & MANUAL) strcat(tmpstr, "MANUAL ");
		strcat(tmpstr, "]");
		printf("    save_state = 0x%x\n", plist->save_state);
		printf("    period: %d; trigger chan: '%s'; monitor period: %d\n",
		   plist->period,plist->trigger_channel,plist->monitor_period);
		printf("    last saved file - %s\n",plist->last_save_file);
		printf("    %d channel%c not connected (or ca_get failed)\n",plist->not_connected,
			(plist->not_connected == 1) ? ' ' : 's');
		if (verbose) {
			for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
				printf("\t%s\t%s",pchannel->name,pchannel->value);
				if (pchannel->enum_val >= 0) printf("\t%d\n",pchannel->enum_val);
				else printf("\n");
			}
		}
	}
	printf("reboot-restore status:\n");
	dbrestoreShow();
	printf("END save_restoreShow\n");
	semGive(sr_mutex);
}


int set_requestfile_path(char *path, char *pathsub)
{
	struct pathListElement *p, *pnew;
	char fullpath[PATH_SIZE+1] = "";
	int path_len=0, pathsub_len=0;

	if (path && *path) path_len = strlen(path);
	if (pathsub && *pathsub) pathsub_len = strlen(pathsub);
	if (path_len + pathsub_len > (PATH_SIZE-1)) {	/* may have to add '/' */
		errlogPrintf("set_requestfile_path: 'path'+'pathsub' is too long\n");
		return(ERROR);
	}

	if (path && *path) {
		strcpy(fullpath, path);
		if (pathsub && *pathsub) {
			if (*pathsub != '/' && path[strlen(path)-1] != '/') {
				strcat(fullpath, "/");
			}
			strcat(fullpath, pathsub);
		}
	} else if (pathsub && *pathsub) {
		strcpy(fullpath, pathsub);
	}

	if (*fullpath) {
		/* return(set_requestfile_path(fullpath)); */
		pnew = (struct pathListElement *)calloc(1, sizeof(struct pathListElement));
		if (pnew == NULL) {
			errlogPrintf("set_requestfile_path: calloc failed\n");
			return(ERROR);
		}
		strcpy(pnew->path, fullpath);
		if (pnew->path[strlen(pnew->path)-1] != '/') {
			strcat(pnew->path, "/");
		}

		if (reqFilePathList == NULL) {
			reqFilePathList = pnew;
		} else {
			for (p = reqFilePathList; p->pnext; p = p->pnext)
				;
			p->pnext = pnew;
		}
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_savefile_path(char *path, char *pathsub)
{
	char fullpath[PATH_SIZE+1] = "";
	int path_len=0, pathsub_len=0;

	if (save_restoreNFSOK) dismountFileSystem();

	if (path && *path) path_len = strlen(path);
	if (pathsub && *pathsub) pathsub_len = strlen(pathsub);
	if (path_len + pathsub_len > (PATH_SIZE-1)) {	/* may have to add '/' */
		errlogPrintf("set_requestfile_path: 'path'+'pathsub' is too long\n");
		return(ERROR);
	}

	if (path && *path) {
		strcpy(fullpath, path);
		if (pathsub && *pathsub) {
			if (*pathsub != '/' && path[strlen(path)-1] != '/') {
				strcat(fullpath, "/");
			}
			strcat(fullpath, pathsub);
		}
	} else if (pathsub && *pathsub) {
		strcpy(fullpath, pathsub);
	}

	if (*fullpath) {
		strcpy(saveRestoreFilePath, fullpath);
		if (saveRestoreFilePath[strlen(saveRestoreFilePath)-1] != '/') {
			strcat(saveRestoreFilePath, "/");
		}
		if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0]) mountFileSystem();
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_saveTask_priority(int priority)
{
	if (priority < 0 || priority > 255) {
		errlogPrintf("save_restore - priority must be >=0 and <= 255\n");
		return(ERROR);
	}
	taskPriority = priority;
	if (taskIdVerify(taskID) == OK) {
	    if (taskPrioritySet(taskID, priority) != OK) {
			errlogPrintf("save_restore - Couldn't set task priority to %d\n", priority);
			return(ERROR);
	    }
	}
	return(OK);
}



/* remove a data set from the list */

int remove_data_set(char *filename)
{
	int found = 0;
	int numchannels = 0;
	struct chlist *plist, *previous;
	struct channel *pchannel, *pchannelt;

	/* find the data set */
	plist = lptr;
	previous = 0;
	while(plist) {
		if (!strcmp(plist->reqFile, filename)) {  
			found = 1;
			break;
		}
		previous = plist;
		plist = plist->pnext;
	}

	if (found) {
		semTake(sr_mutex,WAIT_FOREVER);

		pchannel = plist->pchan_list;
		while(pchannel) {
			if (ca_clear_channel(pchannel->chid) != ECA_NORMAL) {
				errlogPrintf("remove_data_set: couldn't remove ca connection for %s\n", pchannel->name);
			}
			pchannel = pchannel->pnext;
			numchannels++;
		}
		if (ca_pend_io(MIN(10.0, numchannels*0.1)) != ECA_NORMAL) {
		       errlogPrintf("remove_data_set: ca_pend_io() timed out\n");
		}
		pchannel = plist->pchan_list;
		while(pchannel) {
			pchannelt = pchannel->pnext;
			free(pchannel);
			pchannel = pchannelt;

		}
		if (previous == 0) {
			lptr = plist->pnext;
		} else {
			previous->pnext = plist->pnext;
		}
		free(plist);

		semGive(sr_mutex);

	} else {
		errlogPrintf("remove_data_set: Couldn't find '%s'\n", filename);
		sprintf(save_restoreRecentlyStr, "Can't remove data set '%s'", filename);
		return(ERROR);
	}
	sprintf(save_restoreRecentlyStr, "Removed data set '%s'", filename);
	return(OK);
}

int reload_periodic_set(char *filename, int period, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		errlogPrintf("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		errlogPrintf("reload_periodic_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(save_restoreRecentlyStr, "Reloaded data set '%s'", filename);
		return(create_periodic_set(filename, period, macrostring));
	}
}

int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		errlogPrintf("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		errlogPrintf("reload_triggered_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(save_restoreRecentlyStr, "Reloaded data set '%s'", filename);
		return(create_triggered_set(filename, trigger_channel, macrostring));
	}
}


int reload_monitor_set(char * filename, int period, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		errlogPrintf("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		errlogPrintf("reload_monitor_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(save_restoreRecentlyStr, "Reloaded data set '%s'", filename);
		return(create_monitor_set(filename, period, macrostring));
	}
}

int reload_manual_set(char * filename, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		errlogPrintf("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		errlogPrintf("reload_manual_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(save_restoreRecentlyStr, "Reloaded data set '%s'", filename);
		return(create_manual_set(filename, macrostring));
	}
}


/*
 * fdbrestoreX - restore routine
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database vaules.
 *
 */
int fdbrestoreX(char *filename)
{	
	char			channel[80];
	char			restoreFile[PATH_SIZE+1] = "";
	char			buffer[120], *bp, c;
	char			input_line[120];
	int				n;
	FILE			*inp_fd;
	chid			chanid;


	/* open file */
	strncpy(restoreFile, saveRestoreFilePath, sizeof(restoreFile) - 1);
	strncat(restoreFile, filename, MAX(sizeof(restoreFile) -1 - strlen(restoreFile),0));

	if ((inp_fd = fopen(restoreFile, "r")) == NULL) {
		errlogPrintf("fdbrestore - unable to open file %s\n",filename);
		strncpy(save_restoreRecentlyStr, "Manual restore failed",39);
		return(ERROR);
	}

	/* restore from data file */
	while ((bp=fgets(buffer, 120, inp_fd))) {
		/* get PV_name, one space character, value */
		/* (value may be a string with leading whitespace; it may be */
		/* entirely whitespace; the number of spaces may be crucial; */
		/* it might also consist of zero characters) */
		n = sscanf(bp,"%s%c%[^\n]", channel, &c, input_line);
		if (n<3) *input_line = 0;
		if (isalpha(channel[0]) || isdigit(channel[0])) {
			if (ca_search(channel, &chanid) != ECA_NORMAL) {
				errlogPrintf("ca_search for %s failed\n",channel);
			} else if (ca_pend_io(0.5) != ECA_NORMAL) {
				errlogPrintf("ca_search for %s timeout\n",channel);
			} else if (ca_put(DBR_STRING, chanid,input_line) != ECA_NORMAL) {
				errlogPrintf("ca_put of %s to %s failed\n",input_line,channel);
			}
		} else if (channel[0] == '!') {
			n = atoi(&channel[1]);
			errlogPrintf("%d %s not connected (or caget failed)\n", n, (n==1)?"channel":"channels");
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("aborting restore\n");
				fclose(inp_fd);
				strncpy(save_restoreRecentlyStr, "Manual restore failed",39);
				return(ERROR);
			}
		}
	}
	fclose(inp_fd);

	strncpy(save_restoreRecentlyStr, "Manual restore succeeded",39);
	return(OK);
}

STATIC int readReqFile(const char *reqFile, struct chlist *plist, char *macrostring)
{
	struct channel	*pchannel = NULL;
	FILE   			*inp_fd = NULL;
	char			name[40] = "", *t=NULL, line[120]="", eline[120]="";
	char            templatefile[80] = "";
	char            new_macro[80] = "";
	int             i=0;
	MAC_HANDLE      *handle = NULL;
	char            **pairs = NULL;
	struct pathListElement *p;
	char tmpfile[PATH_SIZE+1] = "";

	if (save_restoreDebug >= 1) {
		errlogPrintf("save_restore:readReqFile: entry: reqFile='%s', plist=%p, macrostring='%s'\n",
			reqFile, plist, macrostring?macrostring:"NULL");
	}

	/* open request file */
	if (reqFilePathList) {
		/* try to find reqFile in every directory specified in reqFilePathList */
		for (p = reqFilePathList; p; p = p->pnext) {
			strcpy(tmpfile, p->path);
			strcat(tmpfile, reqFile);
			inp_fd = fopen(tmpfile, "r");
			if (inp_fd) break;
		}
	} else {
		/* try to find reqFile only in current working directory */
		inp_fd = fopen(reqFile, "r");
	}

	if (!inp_fd) {
		plist->status = SR_STATUS_FAIL;
		strcpy(plist->statusStr, "Can't open .req file");
		errlogPrintf("save_restore:readReqFile: unable to open file %s. Exiting.\n", reqFile);
		return(ERROR);
	}

	if (macrostring && macrostring[0]) {
		macCreateHandle(&handle, NULL);
		if (handle) {
			/* base < 3.14.2: macParseDefns uses handle->debug without checking handle==NULL */
			/* macParseDefns(NULL, macrostring, &pairs); */
			macParseDefns(handle, macrostring, &pairs);
			if (pairs) macInstallMacros(handle, pairs);
			if (save_restoreDebug >= 5) {
				errlogPrintf("save_restore:readReqFile: Current macro definitions:\n");
				macReportMacros(handle);
				errlogPrintf("save_restore:readReqFile: --------------------------\n");
			}
		}
	}

	/* place all of the channels in the group */
	while (fgets(line, 120, inp_fd)) {
		/* Expand input line. */
		if (handle && pairs) {
			macExpandString(handle, line, eline, 119);
		} else {
			strcpy(eline, line);
		}
		sscanf(eline, "%s", name);
		if (name[0] == '#') {
			/* take the line as a comment */
		} else if (strncmp(eline, "file", 4) == 0) {
			/* handle include file */
			Debug(2, "save_restore:readReqFile: preparing to include file: eline='%s'\n", eline);

			/* parse template-file name and fix obvious problems */
			templatefile[0] = '\0';
			t = &(eline[4]);
			while (isspace(*t)) t++;  /* delete leading whitespace */
			if (*t == '"') t++;  /* delete leading quote */
			while (isspace(*t)) t++;  /* delete any additional whitespace */
			/* copy to filename; terminate at whitespace or quote or comment */
			for (	i = 0;
					i<PATH_SIZE && !(isspace(*t)) && (*t != '"') && (*t != '#');
					t++,i++) {
				templatefile[i] = *t;
			}
			templatefile[i] = 0;

			/* parse new macro string and fix obvious problems */
			for (i=0; *t && *t != '#'; t++) {
				if (isspace(*t) || *t == ',') {
					if (i>=3 && (new_macro[i-1] != ','))
						new_macro[i++] = ',';
				} else if (*t != '"') {
					new_macro[i++] = *t;
				}
			}
			new_macro[i] = 0;
			if (i && new_macro[i-1] == ',') new_macro[--i] = 0;
			if (i < 3) new_macro[0] = 0; /* if macro has less than 3 chars, punt */
			readReqFile(templatefile, plist, new_macro);
		} else if (isalpha(name[0]) || isdigit(name[0]) || name[0] == '$') {
			pchannel = (struct channel *)calloc(1,sizeof (struct channel));
			if (pchannel == (struct channel *)0) {
				plist->status = SR_STATUS_WARN;
				strcpy(plist->statusStr, "Can't alloc channel memory");
				errlogPrintf("readReqFile: channel calloc failed");
			} else {
				/* add new element to the list */
				pchannel->pnext = plist->pchan_list;
				plist->pchan_list = pchannel;
				strcpy(pchannel->name, name);
				strcpy(pchannel->value,"Not Connected");
				pchannel->enum_val = -1;
			}
		}
	}
	/* close file */
	fclose(inp_fd);
	if (handle) {
		macDeleteHandle(handle);
		if (pairs) free(pairs);
	}

	if (save_restoreDebug >= 1)
		errlogPrintf("save_restore:readReqFile: exit: reqFile='%s'.\n", reqFile);
	return(OK);
}

