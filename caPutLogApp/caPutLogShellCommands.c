#include <dbDefs.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "caPutLog.h"

static const iocshArg caPutLogInitArg0 = {"address", iocshArgString};
static const iocshArg caPutLogInitArg1 = {"config", iocshArgInt};
static const iocshArg *const caPutLogInitArgs[] = {
    &caPutLogInitArg0,
    &caPutLogInitArg1
};
static const iocshFuncDef caPutLogInitDef = {"caPutLogInit", 2, caPutLogInitArgs};
static void caPutLogInitCall(const iocshArgBuf *args)
{
    caPutLogInit(args[0].sval, args[1].ival);
}

static const iocshArg caPutLogFileArg0 = {"filePath", iocshArgString};
static const iocshArg *const caPutLogFileArgs[] = {
    &caPutLogFileArg0,
};
static const iocshFuncDef caPutLogFileDef = {"caPutLogFile", 1, caPutLogFileArgs};
static void caPutLogFileCall(const iocshArgBuf *args)
{
    caPutLogFile(args[0].sval);
}

static const iocshArg caPutLogReconfArg0 = {"config", iocshArgInt};
static const iocshArg *const caPutLogReconfArgs[] = {
    &caPutLogReconfArg0
};
static const iocshFuncDef caPutLogReconfDef = {"caPutLogReconf", 1, caPutLogReconfArgs};
static void caPutLogReconfCall(const iocshArgBuf *args)
{
    caPutLogReconf(args[0].ival);
}

static const iocshArg caPutLogShowArg0 = {"level", iocshArgInt};
static const iocshArg *const caPutLogShowArgs[] = {
    &caPutLogShowArg0
};
static const iocshFuncDef caPutLogShowDef = {"caPutLogShow", 1, caPutLogShowArgs};
static void caPutLogShowCall(const iocshArgBuf *args)
{
    caPutLogShow(args[0].ival);
}

static const iocshArg caPutLogSetTimeFmtArg0 = {"format", iocshArgPersistentString};
static const iocshArg *const caPutLogSetTimeFmtArgs[] = {
    &caPutLogSetTimeFmtArg0
};
static const iocshFuncDef caPutLogSetTimeFmtDef = {"caPutLogSetTimeFmt", 1, caPutLogSetTimeFmtArgs};
static void caPutLogSetTimeFmtCall(const iocshArgBuf *args)
{
    caPutLogSetTimeFmt(args[0].sval);
}

static void caPutLogRegister(void)
{
    static int done = FALSE;

    if(done) return;
    done = TRUE;
    iocshRegister(&caPutLogInitDef,caPutLogInitCall);
    iocshRegister(&caPutLogFileDef,caPutLogFileCall);
    iocshRegister(&caPutLogReconfDef,caPutLogReconfCall);
    iocshRegister(&caPutLogShowDef,caPutLogShowCall);
    iocshRegister(&caPutLogSetTimeFmtDef,caPutLogSetTimeFmtCall);
}
epicsExportRegistrar(caPutLogRegister);
