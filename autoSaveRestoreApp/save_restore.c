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
 *                argument 'verbose' to fdblist();
 * 04/28/03  tmm  v2.9 Drop add_XXXfile_path, and add second argument to
 *                set_XXXfile_path();
 * 05/20/03  tmm  v2.9a Looking for problems that cause save_restore() to hang
 */
#define		SRVERSION "save/restore V2.9a"

#include	<vxWorks.h>
#include	<stdio.h>
#include	<stdioLib.h>
#include	<taskLib.h>
#include	<wdLib.h>
#include	<nfsDrv.h>
#include	<ioLib.h>
#include	<string.h>
#include	<ctype.h>
extern int logMsg(char *fmt, ...);
#include	<tickLib.h>
#include	<usrLib.h>

#include	<dbDefs.h>
#include	<cadef.h>
#include	<epicsPrint.h>
#include 	<tsDefs.h>
#include    <macLib.h>
#include	"save_restore.h"

#define TIME2WAIT 20		/* time to wait for semaphore sem_remove */
#define PATH_SIZE 255		/* size of a single path element */

/*** Debugging variables, macros ***/
/* #define NODEBUG */
#ifdef NODEBUG
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V) {  if (l <= save_restoreDebug) \
			{ epicsPrintf("%s(%d):",__FILE__,__LINE__); \
			  epicsPrintf(FMT,V); } }
#define DebugNV(l,FMT) {  if (l <= save_restoreDebug) \
			{ epicsPrintf("%s(%d):",__FILE__,__LINE__); \
			  epicsPrintf(FMT); } }
#endif
volatile int save_restoreDebug = 0;

volatile int save_restore_test_fopen = 0;

#define FLOAT_FMT "%.7g"
#define DOUBLE_FMT "%.14g"

/*
 * data structure definitions 
 */
static struct chlist *lptr;		/* save-set listhead */
static SEM_ID 		sr_mutex;	/* mut(ual) ex(clusion) for list of save sets */
static SEM_ID		sem_remove;	/* delete list semaphore */

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
	char			reqFile[80];		/* full request file name */
	char			saveFile[PATH_SIZE+1];		/* full save file name */	
	char 			last_save_file[80];	/* file name last used for save */
	char			save_file[80];		/* file name to use on next save */
	int				save_method;		/* bit for each save method requested */
	int				enabled_method;		/* bit for each save method enabled */
	short			save_ok;			/* !=0 -> last save ok */
	int				save_status;		/* status of this save set 	*/
	int				period;			/* time between saves (max for on change) */
	int				monitor_period;	/* time between saves (max for on change) */
	char			trigger_channel[40];	/* db channel to trigger save  */
	WDOG_ID			saveWdId;
	int				not_connected;		/* # bad channels not saved/connected */
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
static short	save_restore_init = 0;
static char 	*SRversion = SRVERSION;
static struct pathListElement *reqFilePathList = NULL;
char			*saveRestoreFilePath = NULL;	/* path to save files, also used by dbrestore.c */
static int		taskPriority =  190;	/* initial task priority */
static int		taskID = 0;				/* save_restore task tid */
static char		remove_filename[80];	/* name of list to delete */
static int		remove_dset = 0;
static int		remove_status = 0;

/* configuration parameters */
static int	min_period	= 4;	/* save no more frequently than every 4 seconds */
static int	min_delay	= 1;	/* check need to save every 1 second */
				/* worst case wait can be min_period + min_delay */
volatile unsigned int	sr_save_incomplete_sets_ok = 1;		/* will save incomplete sets */
volatile unsigned int	sr_restore_incomplete_sets_ok = 1;	/* will restore incomplete sets */

/* functions */
static int periodic_save(struct chlist	*plist);
static void triggered_save(struct event_handler_args);
static int on_change_timer(struct chlist *plist);
static void on_change_save(struct event_handler_args);
int manual_save(char *request_file);
static int save_restore(void);
static int connect_list(struct channel	*pchannel);
static int enable_list(struct chlist *plist);
static int get_channel_list(struct channel *pchannel);
static int write_it(char *filename, struct chlist *plist);
static int save_file(struct chlist	*plist);
int set_savefile_name(char *filename, char *save_filename);
int create_periodic_set(char *filename, int period, char *macrostring);
int create_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int create_monitor_set(char *filename, int period, char *macrostring);
int create_manual_set(char *filename, char *macrostring);
static int create_data_set(char *filename,	int save_method, int period,
		char *trigger_channel, int mon_period, char *macrostring);
int fdbrestore(char *filename);
void fdblist(int verbose);

int set_requestfile_path(char *path, char *pathsub);
int set_savefile_path(char *path, char *pathsub);
int set_saveTask_priority(int priority);
int remove_data_set(char *filename);
int reload_periodic_set(char *filename, int period, char *macrostring);
int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int reload_monitor_set(char * filename, int period, char *macrostring);
int reload_manual_set(char * filename, char *macrostring);
int fdbrestoreX(char *filename);

static int readReqFile(const char *file, struct chlist *plist, char *macrostring);

/*
 * method PERIODIC - timer has elapsed
 */
static int periodic_save(struct chlist	*plist)
{
	plist->save_status |= PERIODIC;
	return(OK);
}


/*
 * method TRIGGERED - ca_monitor received for trigger PV
 */
static void triggered_save(struct event_handler_args event)
{
    struct chlist *plist = (struct chlist *) event.usr;

    if (event.dbr)
	plist->save_status |= TRIGGERED;
}


/*
 * method MONITORED - timer has elapsed
 */
static int on_change_timer(struct chlist *plist)
{
	if (save_restoreDebug >= 10) logMsg("on_change_timer for %s (period is %d ticks)\n",
			plist->reqFile, plist->monitor_period);
	plist->save_status |= TIMER;
	return(OK);
}


/*
 * method MONITORED - ca_monitor received for a PV
 * get all channels; write to disk; restart timer
 */
static void on_change_save(struct event_handler_args event)
{
    struct chlist *plist;
	if (save_restoreDebug >= 10) {
		logMsg("on_change_save: event = 0x%x, event.usr=0x%x\n",
			event, event.usr);
	}
    plist = (struct chlist *) event.usr;

    plist->save_status |= CHANGE;
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
		plist->save_status |= MANUAL;
	else
		epicsPrintf("saveset %s not found", request_file);
	semGive(sr_mutex);
	return(OK);
}


/*
 * save_restore task
 *
 * all requests are flagged in the common data store -
 * they are satisfied here - with all channel access contained within
 */
static int save_restore(void)
{
	struct chlist	*plist;

	while(1) {

		/* look at each list */
		semTake(sr_mutex,WAIT_FOREVER);
		plist = lptr;
		while (plist != 0) {
			if (save_restoreDebug >= 30)
				epicsPrintf("save_restore: '%s' save_status = 0x%x\n",
					plist->reqFile, plist->save_status);
			/* connect the channels on the first instance of this set */
			if (plist->enabled_method == 0) {
				plist->not_connected = connect_list(plist->pchan_list);
				enable_list(plist);
			}

			/* save files that have triggered */
			if ( (plist->save_status & SINGLE_EVENTS)
			  || ((plist->save_status & MONITORED) == MONITORED) ) {

				/* fetch all of the channels */
				plist->not_connected = get_channel_list(plist->pchan_list);

				/* write the data to disk */
				if ((plist->not_connected == 0) || (sr_save_incomplete_sets_ok))
					save_file(plist);
			}

			/* restart timers and reset save requests */
			if (plist->save_status & PERIODIC) {
				if (wdStart(plist->saveWdId, plist->period, periodic_save, (int)plist) < 0) {
					epicsPrintf("could not add %s to the period scan", plist->reqFile);
				}
			}
			if (plist->save_status & SINGLE_EVENTS) {
				plist->save_status = plist->save_status & ~SINGLE_EVENTS;
			}
			if ((plist->save_status & MONITORED) == MONITORED) {
				if(wdStart(plist->saveWdId, plist->monitor_period, on_change_timer, (int)plist)
				  < 0 ) {
					epicsPrintf("could not add %s to the period scan", plist->reqFile);
				}
				plist->save_status = plist->save_status & ~MONITORED;
			}

			/* next list */
			plist = plist->pnext;
		}

		/* release the list */
		semGive(sr_mutex);

		if (remove_dset) {
			if ((remove_status = remove_data_set(remove_filename))) {
				epicsPrintf("error removing %s\n", remove_filename);
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
static int connect_list(struct channel	*pchannel)
{
	struct channel	*sav_pchannel = pchannel;
	int				errors = 0;
	short			num_channels = 0;
	float			fval;
	double			dval;

	/* connect all channels in the list */
	while (pchannel != 0) {
		if (ca_search(pchannel->name,&pchannel->chid) == ECA_NORMAL) {
			strcpy(pchannel->value,"Search Issued");
		} else {
			strcpy(pchannel->value,"Search Not Issued");
		}
		pchannel = pchannel->pnext;
	}
	if (ca_pend_io(5.0) == ECA_TIMEOUT) {
		logMsg("not all searches successful\n");
	}
		
	/* fetch all channels in the list */
	for (pchannel = sav_pchannel; pchannel != 0; pchannel = pchannel->pnext) {
		if (ca_state(pchannel->chid) == cs_conn) {
			num_channels++;
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,&fval);
				sprintf(pchannel->value, FLOAT_FMT, fval);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,&dval);
				sprintf(pchannel->value, DOUBLE_FMT, dval);
			} else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			if (ca_field_type(pchannel->chid) == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
		} else {
			errors++;
			logMsg("%s is not connected\n",pchannel->name);
		}
	}
	if (ca_pend_io(MIN(10.0, num_channels*0.1)) == ECA_TIMEOUT) {
		logMsg("connect_list: not all channels initialized\n");
		errors++;
	} else {
		if (save_restoreDebug >= 20) {
			logMsg("connect_list: got %d channel values.\n", num_channels);
		}
	}
	return(errors);
}


/*
 * enable new save methods
 *
 * NOTE: Assumes the sr_mutex is taken
 */
static int enable_list(struct chlist *plist)
{
	struct channel	*pchannel;
	chid 			chid;			/* channel access id */

	DebugNV(4, "enable_list: entry\n");
	/* enable a periodic set */
	if ((plist->save_method & PERIODIC) && !(plist->enabled_method & PERIODIC)) {
		if (wdStart(plist->saveWdId, plist->period, periodic_save, (int)plist) < 0) {
			logMsg("could not add %s to the period scan", plist->reqFile);
		}
		plist->enabled_method |= PERIODIC;
	}

	/* enable a triggered set */
	if ((plist->save_method & TRIGGERED) && !(plist->enabled_method & TRIGGERED)) {
		if (ca_search(plist->trigger_channel, &chid) != ECA_NORMAL) {
			logMsg("trigger %s search failed\n", plist->trigger_channel);
		} else if (ca_pend_io(2.0) != ECA_NORMAL) {
			logMsg("timeout on search of %s\n", plist->trigger_channel);
		} else if (ca_state(chid) != cs_conn) {
			logMsg("trigger %s search not connected\n", plist->trigger_channel);
		} else if (ca_add_event(DBR_FLOAT, chid, triggered_save, (void *)plist, 0) !=ECA_NORMAL) {
			logMsg("trigger event for %s failed\n", plist->trigger_channel);
		} else{
			plist->enabled_method |= TRIGGERED;
		}
	}

	/* enable a monitored set */
	if ((plist->save_method & MONITORED) && !(plist->enabled_method & MONITORED)) {
		for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
			Debug(10, "enable_list: calling ca_add_event for '%s'\n", pchannel->name);
			if (save_restoreDebug >= 10) logMsg("enable_list: arg = 0x%x\n", plist);
			/*
			 * Work around obscure problem affecting USHORTS by making DBR type different
			 * from any possible field type.  This avoids tickling a bug that causes dbGet
			 * to overwrite the source field with its own value converted to LONG.
			 * (Changed DBR_LONG to DBR_TIME_LONG.)
			 */
			if (ca_add_event(DBR_TIME_LONG, pchannel->chid, on_change_save,
					(void *)plist, 0) != ECA_NORMAL) {
				logMsg("could not add event for %s in %s\n",
					pchannel->name,plist->reqFile);
			}
		}
		DebugNV(4 ,"enable_list: done calling ca_add_event for list channels\n");
		if (ca_pend_io(1.0) != ECA_NORMAL) {
			logMsg("timeout on monitored set: %s to monitored scan\n",plist->reqFile);
		}
		if (wdStart(plist->saveWdId, plist->monitor_period, on_change_timer, (int)plist) < 0) {
			logMsg("watchdog for set %s not started\n",plist->reqFile);
		}
		plist->enabled_method |= MONITORED;
	}

	/* enable a manual request set */
	if ((plist->save_method & MANUAL) && !(plist->enabled_method & MANUAL)) {
		plist->enabled_method |= MANUAL;
	}
	return(OK);
}


/*
 * fetch all channels in the save set
 *
 * NOTE: Assumes sr_mutex is taken
 */
#define INIT_STRING "!@#$%^&*()"
static int get_channel_list(struct channel *pchannel)
{
	struct channel 	*tpchannel = pchannel;
	int				not_connected = 0;
	unsigned short	num_channels = 0;
	float 			fval;
	double 			dval;

	/* attempt to fetch all channels that are connected */
	while (pchannel != 0) {
		pchannel->valid = 0;
		if (ca_state(pchannel->chid) == cs_conn) {
			strcpy(pchannel->value, INIT_STRING);
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,&fval);
				sprintf(pchannel->value, FLOAT_FMT, fval);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,&dval);
				sprintf(pchannel->value, DOUBLE_FMT, dval);
			}
			else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			num_channels++;
			if (ca_field_type(pchannel->chid) == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
			pchannel->valid = 1;
		}else{
			not_connected++;
		}
		pchannel = pchannel->pnext;
	}
	if (ca_pend_io(.1*num_channels) != ECA_NORMAL) {
		logMsg("get_channel_list: not all gets completed");
		not_connected++;
	}

	/* check to see which gets completed */
	for (pchannel=tpchannel; pchannel; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			/* then we at least had a CA connection.  Did it produce? */
			pchannel->valid = strcmp(pchannel->value, INIT_STRING);
		}
	}

	return(not_connected);
}

/* Actually write the file
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
static int write_it(char *filename, struct chlist	*plist)
{
	FILE 			*out_fd;
	struct channel	*pchannel;

	if ((out_fd = fopen(filename,"w")) == NULL) {
		logMsg("save_file - unable to open file %s", filename);
		return(ERROR);
	}
	fprintf(out_fd,"# %s\tAutomatically generated - DO NOT MODIFY\n", SRversion);
	if (plist->not_connected) {
		fprintf(out_fd,"! %d channel(s) not connected - or not all gets were successful\n",
			plist->not_connected);
	}
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			fprintf(out_fd, "%s ", pchannel->name);
		} else {
			fprintf(out_fd, "#%s ", pchannel->name);
		}
		if (pchannel->enum_val >= 0) {
			fprintf(out_fd, "%d\n",pchannel->enum_val);
		} else {
			fprintf(out_fd, "%-s\n", pchannel->value);
		}
	}
#if 0
	if (save_restoreDebug == 999) {
		logMsg("save_restore: simulating task crash.  Bye, bye!\n");
		exit(-1);
	}
#endif
	fprintf(out_fd, "<END>\n");
	fflush(out_fd);
	ioctl(fileno(out_fd),FIOSYNC,0);	/* NFS flush needs a little extra push */
	fclose(out_fd);
	return(OK);
}


/* state of backup restore file */
#define BS_BAD 	0
#define BS_OK	1
#define BS_NEW	2

/*
 * save_file - save routine
 *
 * Write a list of channel names and values to an ASCII file.
 *
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
static int save_file(struct chlist	*plist)
{
	FILE	*inp_fd, *out_fd;
	char	save_file_backup[PATH_SIZE+3] = "", tmpfile[PATH_SIZE+1] = "";
	char	tmpstr[20], *p;
	int		backup_state = BS_OK, cant_open=0;

	if (save_restore_test_fopen) {
		/*
		 * Ensure that we can create a new file in the target directory before we
		 * start messing with the existing save file.
		 */
        strncpy(tmpfile, plist->saveFile, sizeof(tmpfile) - 20);
		p = strrchr(tmpfile, (int)'/');
		if (p) {
			p[1] = 0;	/* delete file name from tmpfile, leaving path and final '/' */
		} else {
			tmpfile[0] = 0;	/* no path; use current directory */
		}
        strcat(tmpfile, "save_restore_testXQ");
		if ((out_fd = fopen(tmpfile, "r")) != NULL) {
			/* file exists (left over from previous test) */
			fclose(out_fd);
			remove(tmpfile);
		}
		if ((out_fd = fopen(tmpfile, "w")) == NULL) {
			cant_open = 1;
			logMsg("save_file:  Can't open test file '%s'.\n", tmpfile);
		} else {
			fclose(out_fd);
			remove(tmpfile);
			if (save_restoreDebug) logMsg("fopen('%s') succeeded\n", tmpfile);
		}
	}

	/*
	 * Check out the backup copy of the save file before we blow away the save file.
	 * If the backup isn't ok, write a new one.
	 */
	strcpy(save_file_backup, plist->saveFile);
	strncat(save_file_backup, "B", 1);
	if ((inp_fd = fopen(save_file_backup, "r")) == NULL) {
		backup_state = BS_BAD;
	} else {
		if ((fseek(inp_fd, -6, SEEK_END)) ||
			(fgets(tmpstr, 6, inp_fd) == 0) ||
			(strncmp(tmpstr, "<END>", 5) != 0)) {
				backup_state = BS_BAD;
		}
		fclose(inp_fd);
	}
	if (backup_state == BS_BAD) {
		logMsg("save_file: Backup file (%s) is bad.  Writing a new one.\n", 
			save_file_backup);
		if (write_it(save_file_backup, plist) == -1) {
			logMsg("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
			logMsg("save_restore:save_file: Can't write new backup file.  I quit.\n");
			logMsg("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
			return(ERROR);
		}
		backup_state = BS_NEW;
	}

	/*** Write the save file ***/
	Debug(1, "save_file: saving to %s\n", plist->saveFile);
	if (write_it(plist->saveFile, plist) == -1) {
		logMsg("*** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		logMsg("save_restore:save_file: Can't write save file.  I quit.\n");
		logMsg("*** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		return(ERROR);
	}

	if (cant_open) {
		logMsg("save_file:  Used existing file for autosave,\n");
		logMsg("...but this file is vulnerable to corruption\n");
		logMsg("...because I can't change its length.\n");
	}

	/* keep the name of the last saved file */
	strcpy(plist->last_save_file,plist->save_file);		

	/*** Write a backup copy of the save file ***/
	if (backup_state != BS_NEW) {
		/* make a backup copy */
		strcpy(save_file_backup, plist->saveFile);
		strncat(save_file_backup, "B", 1);
		if (copy(plist->saveFile, save_file_backup) != OK) {
			logMsg("save_restore:save_file - Couldn't make backup '%s'\n",
				save_file_backup);
			return(ERROR);
		}
	}
	return(OK);
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
			return(OK);
		}
		plist = plist->pnext;
	}
	logMsg("No save set enabled for %s\n",filename);
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
		logMsg("create_triggered_set: Error: trigger-channel name is required.\n");
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
static int create_data_set(
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

	if (save_restoreDebug) {
		logMsg("create_data_set: file '%s', method %x, period %d, trig_chan '%s', mon_period %d\n",
			filename, save_method, period, trigger_channel ? trigger_channel : "NONE", mon_period);
	}

	/* initialize save_restore routines */
	if (!save_restore_init) {
		if ((sr_mutex = semMCreate(SEM_Q_FIFO)) == 0) {
			logMsg("create_data_set: could not create list header mutex");
			return(ERROR);
		}
		if ((sem_remove = semBCreate(SEM_Q_FIFO, SEM_EMPTY)) == 0) {
			logMsg("create_data_set: could not create delete list semaphore\n");
			return(ERROR);
		}
		if ((taskID = taskSpawn("save_restore",taskPriority,VX_FP_TASK,
			10000, save_restore,0,0,0,0,0,0,0,0,0,0))
		  == ERROR) {
			logMsg("create_data_set: could not create save_restore task");
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
				logMsg("create_data_set: %s in %x mode",filename,save_method);
				return(ERROR);
			}else{
				if (save_method == TRIGGERED)
					if (trigger_channel) {
						strcpy(plist->trigger_channel,trigger_channel);
					} else {
						logMsg("create_data_set: no trigger channel");
						semGive(sr_mutex);
						return(ERROR);
					}
				else if (save_method == PERIODIC)
					plist->period = per_ticks;
				else if (save_method == MONITORED)
					plist->monitor_period = mon_ticks;
				plist->save_method |= save_method;
				enable_list(plist);

				semGive(sr_mutex);
				return(OK);
			}
		}
		plist = plist->pnext;
	}
	semGive(sr_mutex);

	/* create a new channel list */
	if ((plist = (struct chlist *)calloc(1,sizeof (struct chlist)))
	  == (struct chlist *)0) {
		logMsg("create_data_set: channel list calloc failed");
		return(ERROR);
	}
	if ((plist->saveWdId = wdCreate()) == 0) {
		logMsg("create_data_set: could not create watchdog");
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
	plist->save_status = 0;
	plist->save_ok = 0;
	plist->monitor_period = mon_ticks;
	/* save_file name */
	strcpy(plist->save_file, plist->reqFile);
	inx = 0;
	while ((plist->save_file[inx] != 0) && (plist->save_file[inx] != '.') && (inx < 74)) inx++;
	plist->save_file[inx] = 0;	/* truncate if necessary to leave room for ".sav" + null */
	strcat(plist->save_file,".sav");
	if (saveRestoreFilePath) {
		strncpy(plist->saveFile, saveRestoreFilePath, sizeof(plist->saveFile) - 1);
	}
	strncat(plist->saveFile, plist->save_file, MAX(sizeof(plist->saveFile) - 1 -
			strlen(plist->saveFile),0));

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
	FILE			*inp_fd;
	chid			chanid;
	unsigned short	i, j;

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
			logMsg("fdbrestore: %d channel(s) not connected or fetched\n",plist->not_connected);
			if (!sr_restore_incomplete_sets_ok) {
				logMsg("fdbrestore: aborting restore\n");
				semGive(sr_mutex);
				return(ERROR);
			}
		}

		for (pchannel = plist->pchan_list; pchannel !=0; pchannel = pchannel->pnext) {
			ca_put(DBR_STRING,pchannel->chid,pchannel->value);
		}
		if (ca_pend_io(1.0) != ECA_NORMAL) {
			logMsg("fdbrestore: not all channels restored\n");
		}
		semGive(sr_mutex);
		return(OK);
	}
	semGive(sr_mutex);

	/* open file */
	if (saveRestoreFilePath) {
		strncpy(restoreFile, saveRestoreFilePath, sizeof(restoreFile) - 1);
	}
	strncat(restoreFile, filename, MAX(sizeof(restoreFile) -1 - strlen(restoreFile),0));
	if ((inp_fd = fopen_and_check(restoreFile, "r")) == NULL) {
		logMsg("fdbrestore: Can't open save file.");
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
				logMsg("ca_search for %s failed\n", channel);
			} else if (ca_pend_io(0.5) != ECA_NORMAL) {
				logMsg("ca_search for %s timeout\n", channel);
			} else if (ca_put(DBR_STRING, chanid, input_line) != ECA_NORMAL) {
				logMsg("ca_put of %s to %s failed\n", input_line,channel);
			}
		} else if (channel[0] == '!') {
			for (i = 0; isspace(input_line[i]); i++);	/* skip blanks */
			for (j = 0; isdigit(input_line[i]); i++, j++)
				channel[j] = input_line[i];
			channel[j] = 0;
			logMsg("%s channels not connected / fetch failed", channel);
			if (!sr_restore_incomplete_sets_ok) {
				logMsg("aborting restore\n");
				fclose(inp_fd);
				return(ERROR);
			}
		}
	}
	fclose(inp_fd);

	/* make  backup */
	strcpy(bu_filename,filename);
	strcat(bu_filename,".bu");		/* this may want to be the date */
	copy(restoreFile,bu_filename);

	return(OK);
}


/*
 * fdblist -  list save sets
 *
 */
void fdblist(int verbose)
{
	struct chlist	*plist;
	struct channel 	*pchannel;
	struct pathListElement *p = reqFilePathList;

	printf("fdblist\n");
	printf("request file path list:\n");
	while (p) {
		printf("... '%s'\n", p->path);
		p = p->pnext;
	}
	printf("save file path = '%s'\n", saveRestoreFilePath?saveRestoreFilePath:"");
	semTake(sr_mutex,WAIT_FOREVER);
	for (plist = lptr; plist != 0; plist = plist->pnext) {
		printf("%s: \n",plist->reqFile);
		printf("  save_status = 0x%x\n", plist->save_status);
		printf("  last saved file - %s\n",plist->last_save_file);
		printf("  method %x period %d trigger chan %s monitor period %d\n",
		   plist->save_method,plist->period,plist->trigger_channel,plist->monitor_period);
		printf("  %d channels not connected - or failed gets\n",plist->not_connected);
		if (verbose) {
			for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
				printf("\t%s\t%s",pchannel->name,pchannel->value);
				if (pchannel->enum_val >= 0) printf("\t%d\n",pchannel->enum_val);
				else printf("\n");
			}
		}
	}
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
		logMsg("set_requestfile_path: 'path'+'pathsub' is too long\n");
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
			logMsg("set_requestfile_path: calloc failed\n");
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

	if (path && *path) path_len = strlen(path);
	if (pathsub && *pathsub) pathsub_len = strlen(pathsub);
	if (path_len + pathsub_len > (PATH_SIZE-1)) {	/* may have to add '/' */
		logMsg("set_requestfile_path: 'path'+'pathsub' is too long\n");
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
		if (saveRestoreFilePath) free(saveRestoreFilePath);
		saveRestoreFilePath = (char *)calloc(PATH_SIZE+1,sizeof(char));
		if (saveRestoreFilePath == NULL) {
			logMsg("set_savefile_path: calloc failed\n");
			return(ERROR);
		}
		strcpy(saveRestoreFilePath, fullpath);
		if (saveRestoreFilePath[strlen(saveRestoreFilePath)-1] != '/') {
			strcat(saveRestoreFilePath, "/");
		}
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_saveTask_priority(int priority)
{
	if (priority < 0 || priority > 255) {
		logMsg("save_restore - priority must be >=0 and <= 255\n");
		return(ERROR);
	}
	taskPriority = priority;
	if (taskIdVerify(taskID) == OK) {
	    if (taskPrioritySet(taskID, priority) != OK) {
			logMsg("save_restore - Couldn't set task priority to %d\n", priority);
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
				logMsg("remove_data_set: couldn't remove ca connection for %s\n", pchannel->name);
			}
			pchannel = pchannel->pnext;
			numchannels++;
		}
		if (ca_pend_io(MIN(10.0, numchannels*0.1)) != ECA_NORMAL) {
		       logMsg("remove_data_set: ca_pend_io() timed out\n");
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
		logMsg("remove_data_set: Couldn't find '%s'\n", filename);
		return(ERROR);
	}
	return(OK);
}

int reload_periodic_set(char *filename, int period, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		logMsg("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		logMsg("reload_periodic_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		return(create_periodic_set(filename, period, macrostring));
	}
}

int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		logMsg("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		logMsg("reload_triggered_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		return(create_triggered_set(filename, trigger_channel, macrostring));
	}
}


int reload_monitor_set(char * filename, int period, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		logMsg("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		logMsg("reload_monitor_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		return(create_monitor_set(filename, period, macrostring));
	}
}

int reload_manual_set(char * filename, char *macrostring)
{
	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	if (semTake(sem_remove, vxTicksPerSecond*TIME2WAIT) != OK) {
		logMsg("Error: timeout on remove_data_set %s\n", filename);
		return(ERROR);
	}
	if (remove_status) {
		logMsg("reload_manual_set: error removing %s\n", filename);
		return(ERROR);
	} else {
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
	int				i,j;


	/* open file */
	if (saveRestoreFilePath) {
		strncpy(restoreFile, saveRestoreFilePath, sizeof(restoreFile) - 1);
	}
	strncat(restoreFile, filename, MAX(sizeof(restoreFile) -1 - strlen(restoreFile),0));

	if ((inp_fd = fopen(restoreFile, "r")) == NULL) {
		logMsg("fdbrestore - unable to open file %s\n",filename);
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
				logMsg("ca_search for %s failed\n",channel);
			} else if (ca_pend_io(0.5) != ECA_NORMAL) {
				logMsg("ca_search for %s timeout\n",channel);
			} else if (ca_put(DBR_STRING, chanid,input_line) != ECA_NORMAL) {
				logMsg("ca_put of %s to %s failed\n",input_line,channel);
			}
		} else if (channel[0] == '!') {
			for (i = 0; isspace(input_line[i]); i++);	/* skip blanks */
			for (j = 0; isdigit(input_line[i]); i++,j++)
				channel[j] = input_line[i];
			channel[j] = 0;
			logMsg("%s channels not connected / fetch failed\n", channel);
			if (!sr_restore_incomplete_sets_ok) {
				logMsg("aborting restore\n");
				fclose(inp_fd);
				return(ERROR);
			}
		}
	}
	fclose(inp_fd);

	return(OK);
}

static int readReqFile(const char *reqFile, struct chlist *plist, char *macrostring)
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
		logMsg("save_restore:readReqFile: entry: reqFile='%s', plist=%p, macrostring='%s'\n",
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
		logMsg("save_restore:readReqFile: unable to open file %s\n", reqFile);
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
				logMsg("save_restore:readReqFile: Current macro definitions:\n");
				macReportMacros(handle);
				logMsg("save_restore:readReqFile: --------------------------\n");
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
				logMsg("readReqFile: channel calloc failed");
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
	return(OK);
}

