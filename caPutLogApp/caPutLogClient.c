/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* caPutLogClient.c,v 1.25.2.6 2004/10/07 13:37:34 mrk Exp */
/*
 *      Author:         Jeffrey O. Hill
 *      Date:           080791
 */

/*
 * ANSI C
 */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <dbDefs.h>
#include <envDefs.h>
#include <errlog.h>
#include <logClient.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>

#define epicsExportSharedSymbols
#include "caPutLog.h"
#include "caPutLogClient.h"

#ifndef LOCAL
#define LOCAL static
#endif

#ifdef _WIN32
#define strtok_r strtok_s
#endif

LOCAL READONLY ENV_PARAM EPICS_CA_PUT_LOG_ADDR = {"EPICS_CA_PUT_LOG_ADDR", ""};

LOCAL struct clientItem {
    logClientId caPutLogClient;
    struct clientItem *next;
    char addr[1];
} *caPutLogClients = NULL;
static epicsMutexId caPutLogClientsMutex;

LOCAL FILE		*	caPutLogFp	= NULL;

epicsShareFunc int epicsShareAPI caPutLogFile (const char *file_path)
{
	if ( file_path == NULL || strlen(file_path) == 0 )
	{
		if ( caPutLogFp	!= NULL )
		{
			fclose( caPutLogFp );
			caPutLogFp = NULL;
		}
		return caPutLogSuccess;
	}

	caPutLogFp = fopen( file_path, "w+" );
	if ( caPutLogFp == NULL )
	{
        fprintf( stderr, "caPutLogFile: Unable to open log file %s\n", file_path );
        return caPutLogError;
	}
	return caPutLogSuccess;
}

/*
 *  caPutLogClientFlush ()
 */
void caPutLogClientFlush ()
{
    struct clientItem* c;
    epicsMutexMustLock(caPutLogClientsMutex);
    for (c = caPutLogClients; c; c = c->next) {
        logClientFlush (c->caPutLogClient);
    }
	if ( caPutLogFp != NULL ) {
		fflush( caPutLogFp );
	}
    epicsMutexUnlock(caPutLogClientsMutex);
}

/*
 *  caPutLogClientShow ()
 */
void caPutLogClientShow (unsigned level)
{
    struct clientItem* c;
    epicsMutexMustLock(caPutLogClientsMutex);
    for (c = caPutLogClients; c; c = c->next) {
        logClientShow (c->caPutLogClient, level);
    }
    epicsMutexUnlock(caPutLogClientsMutex);
}

/*
 *  caPutLogClientInit()
 */
int caPutLogClientInit (const char *addr_str)
{
    int status;
    struct sockaddr_in saddr;
    unsigned short default_port = 7011;
    struct clientItem** pclient;
    char *clientaddr;
    char *saveptr;
    char *addr_str_copy1;
    char *addr_str_copy2;

    if (!caPutLogClientsMutex)
        caPutLogClientsMutex = epicsMutexMustCreate();
    if (!addr_str || !addr_str[0]) {
        if (caPutLogClients) return caPutLogSuccess;
        addr_str = envGetConfigParamPtr(&EPICS_CA_PUT_LOG_ADDR);
    }
    if (addr_str == NULL) {
        errlogSevPrintf(errlogMajor, "caPutLog: server address not specified\n");
        return caPutLogError;
    }

    addr_str_copy2 = addr_str_copy1 = epicsStrDup(addr_str);

    epicsMutexMustLock(caPutLogClientsMutex);
    while(1) {
        clientaddr = strtok_r(addr_str_copy1, " \t\n\r", &saveptr);
        if (!clientaddr) break;
        addr_str_copy1 = NULL;

        for (pclient = &caPutLogClients; *pclient; pclient = &(*pclient)->next) {
            if (strcmp(clientaddr, (*pclient)->addr) == 0) {
                fprintf (stderr, "caPutLog: address %s already configured\n", clientaddr);
                break;
            }
        }
        if (*pclient) continue;

        status = aToIPAddr (clientaddr, default_port, &saddr);
        if (status<0) {
            fprintf (stderr, "caPutLog: bad address or host name %s\n", clientaddr);
            continue;
        }

        *pclient = callocMustSucceed(1,sizeof(struct clientItem)+strlen(clientaddr),"caPutLog");
        strcpy((*pclient)->addr, clientaddr);

        (*pclient)->caPutLogClient = logClientCreate (saddr.sin_addr, ntohs(saddr.sin_port));
        if (!(*pclient)->caPutLogClient) {
            fprintf (stderr, "caPutLog: cannot create logClient %s\n", clientaddr);
            free(*pclient);
            *pclient = NULL;
            continue;
        }

        (*pclient)->next = NULL;
    }
    epicsMutexUnlock(caPutLogClientsMutex);
    free(addr_str_copy2);
    return caPutLogClients ? caPutLogSuccess : caPutLogError;
}

/*
 * caPutLogClientSend ()
 */
void caPutLogClientSend (const char *message)
{
    struct clientItem* c;
    epicsMutexMustLock(caPutLogClientsMutex);
    for (c = caPutLogClients; c; c = c->next) {
        logClientSend (c->caPutLogClient, message);
    }
    if (caPutLogFp) {
		fwrite( message, sizeof(char), strlen(message), caPutLogFp );
		fflush( caPutLogFp );
    }
    epicsMutexUnlock(caPutLogClientsMutex);
}
