/*
 *  November 5, 2013   KHKIM
 *  Demonstrate the user-defined time source function
 */




#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <cantProceed.h>
#include <ellLib.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsString.h>
#include <epicsStdioRedirect.h>
#include <iocsh.h>
#include <gpHash.h>
#include <registryFunction.h>
// epicsExportSharedSymbols
#include <shareLib.h>
#include <asynDriver.h>
#include <asynOctet.h>
#include <asynOption.h>
#include <asynOctetSyncIO.h>

#include <epicsExport.h>

#include "registerTimeStampSource.h"

void devTimeStampSource_listPV(ELLLIST *plist);

static ELLLIST   timeStampSourceList;

static void init_timeStampSourceList(void)
{
    static int firstTime =1;
    if(!firstTime) return;
    firstTime = 0;

    ellInit(&timeStampSourceList);
}

adTimeStampSource_p find_timeStampSource(const char *portName)
{
    adTimeStampSource_p pTss = NULL;

    pTss = (adTimeStampSource_p) ellFirst(&timeStampSourceList);

    while(pTss) {
        if(!strcmp(pTss->portName, portName)) break;
        pTss = (adTimeStampSource_p) ellNext(&pTss->node);
    }

    return pTss;
}



adTimeStampSource_p  alloc_timeStampSource(const char *portName, const char *functionName)
{
    adTimeStampSource_p pTss = NULL;

    pTss = mallocMustSucceed(sizeof(adTimeStampSource_t), "Data Structure for TimeStampSource");
    strcpy(pTss->portName, portName);
    strcpy(pTss->functionName, functionName);
    pTss->pFunction = NULL;
    pTss->eventCode = 0;
    pTss->lock = epicsMutexMustCreate();
    scanIoInit(&pTss->ioScanPvt);
    scanIoInit(&pTss->slowIoScanPvt);
    ellInit(&pTss->recList);

    ellAdd(&timeStampSourceList, &pTss->node);

    return pTss;
}


static void display_timeStampSource(adTimeStampSource_p pTss)
{
    if(!pTss) return;

    printf(" port name: %16s     function name: %16s, (%p)\n", pTss->portName, pTss->functionName, (void*) pTss->pFunction);
    printf(" time-stamp source has %d PV(s)\n", ellCount(&pTss->recList));
    devTimeStampSource_listPV(&pTss->recList);
}


epicsInt32 setEventCode_timeStampSource(adTimeStampSource_p pTss, epicsInt32 eventCode)
{
    if(!pTss) return -1;

    epicsMutexLock(pTss->lock);
    pTss->eventCode = eventCode;
    epicsMutexUnlock(pTss->lock);

    return eventCode;
}


epicsInt32 getEventCode_timeStampSource(adTimeStampSource_p pTss)
{
    epicsInt32   eventCode = -1;

    if(!pTss) return eventCode;

    epicsMutexLock(pTss->lock);
    eventCode = pTss->eventCode;
    epicsMutexUnlock(pTss->lock);


    return eventCode;
}

epicsInt32 getActivePulseID_timeStampSource(adTimeStampSource_p pTss)
{
    epicsInt32   pulseID = -1;
    if(!pTss) return pulseID;

    epicsMutexLock(pTss->lock);
    pulseID = pTss->active_pulseID;
    epicsMutexUnlock(pTss->lock);

    return pulseID;
}

epicsInt32 getCurrentPulseID_timeStampSource(adTimeStampSource_p pTss)
{
    epicsInt32    pulseID = -1;
    if(!pTss) return pulseID;

    epicsMutexLock(pTss->lock);
    pulseID = pTss->current_pulseID;
    epicsMutexUnlock(pTss->lock);

    return pulseID;
}

epicsInt32 getDiffPulseID_timeStampSource(adTimeStampSource_p pTss)
{
    epicsInt32 diff_pulseID = 0;
    if(!pTss) return diff_pulseID;

    epicsMutexLock(pTss->lock);
    diff_pulseID = pTss->diff_pulseID;
    epicsMutexUnlock(pTss->lock);

    return diff_pulseID;
}

epicsInt32 getCurrentTimeSlot_timeStampSource(adTimeStampSource_p pTss)
{
    epicsInt32    timeSlot = -1;
    if(!pTss)  return timeSlot;

    epicsMutexLock(pTss->lock);
    timeSlot = pTss->current_timeslot;
    epicsMutexUnlock(pTss->lock);

    return timeSlot;
}

epicsInt32 getErTicks_timeStampSource(adTimeStampSource_p pTss)
{
    epicsInt32    erTicks = -1;
    if(!pTss) return erTicks;

    epicsMutexLock(pTss->lock);
    erTicks = pTss->erTicks;
    epicsMutexUnlock(pTss->lock);

    return erTicks;
}



epicsShareFunc int listUserTimeStampSource(const char *portName)
{
    adTimeStampSource_p pTss = NULL;

    if(portName && (strlen(portName) > 0)) {
        pTss = find_timeStampSource(portName);
        if(!pTss) {
            printf("listUserTimeStampSource, cannot find port name (%s)\n", portName);
            return 0;
        }
       display_timeStampSource(pTss);
       return 0;
    }

    printf("listUserTimeStampSource, found %d registration(s)\n", ellCount(&timeStampSourceList));
    pTss = (adTimeStampSource_p) ellFirst(&timeStampSourceList);
    while(pTss) {
        display_timeStampSource(pTss);
        pTss = (adTimeStampSource_p) ellNext(&pTss->node);
    }
    
    return 0;
}


epicsShareFunc int registerUserTimeStampSource(const char *portName, const char *functionName)
{
    asynUser            *pasynUser;
    asynStatus          status;
    timeStampCallback   pFunction;
    adTimeStampSource_p pTss;

    if(!portName || !functionName || (strlen(portName) == 0) || (strlen(functionName) == 0)) {
        printf("Usage: registerUserTimeStampSource portName functionName\n");
        return -1;
    }

    init_timeStampSourceList();
    pasynUser = pasynManager->createAsynUser(0,0);
    status    = pasynManager->connectDevice(pasynUser, portName, 0);
    if(status) {
        printf("registerUserTimeStampSource, cannot connect to port %s\n", portName);
        return -1;
    }
    pFunction = (timeStampCallback)registryFunctionFind(functionName);
    if(!pFunction) {
        printf("registerUserTimeStampSource, cannot find function %s\n", functionName);
        return -1;
    }
    pTss = find_timeStampSource(portName);
    if(pTss) {
        printf("registerUserTimeStampSource, found that function(%s) has been registered for port (%s)\n",
               pTss->functionName, pTss->portName);
        return -1;
    }
    
    pTss = alloc_timeStampSource(portName, functionName);
    pTss->pFunction = pFunction;
    pasynManager->registerTimeStampSource(pasynUser, (void*)pTss, pFunction);


    return 0;
}


static const iocshArg registerUserTimeStampSourceArg0 = { "portName", iocshArgString};
static const iocshArg registerUserTimeStampSourceArg1 = { "functionName", iocshArgString};
static const iocshArg * const registerUserTimeStampSourceArgs[] = {
                                           &registerUserTimeStampSourceArg0, 
                                           &registerUserTimeStampSourceArg1 };
static const iocshFuncDef registerUserTimeStampSourceDef = {
                         "registerUserTimeStampSource", 2, registerUserTimeStampSourceArgs};
static void registerUserTimeStampSourceCall(const iocshArgBuf *args)
{
    registerUserTimeStampSource(args[0].sval, args[1].sval);
}



static const iocshArg listUserTimeStampSourceArg0 = { "portName", iocshArgString};
static const iocshArg * const listUserTimeStampSourceArgs[] = {
                                           &listUserTimeStampSourceArg0 };
static const iocshFuncDef listUserTimeStampSourceDef = {
                         "listUserTimeStampSource", 1, listUserTimeStampSourceArgs};
static void listUserTimeStampSourceCall(const iocshArgBuf *args)
{
    listUserTimeStampSource(args[0].sval);
}

static void registerUserTimeStampSourceRegistrar(void)
{
    static int firstTime = 1;

    if(!firstTime) return;
    firstTime = 0;

    iocshRegister(&registerUserTimeStampSourceDef, registerUserTimeStampSourceCall);
    iocshRegister(&listUserTimeStampSourceDef,     listUserTimeStampSourceCall);

}

epicsExportRegistrar(registerUserTimeStampSourceRegistrar);

