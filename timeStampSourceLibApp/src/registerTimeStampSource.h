#ifndef   REGTIMESTAMPSOURCE_H
#define   REGTIMESTAMPSOURCE_H

#include <epicsStdio.h>
#include <epicsTime.h>
#include <ellLib.h>
#include <shareLib.h>
#include <dbScan.h>

#include "evrPattern.h"
#include "evrTime.h"


typedef  struct {
    ELLNODE          node;
    char             portName[32];
    char             functionName[32];
    void (*pFunction)(void *userPvt, epicsTimeStamp *pTimeStamp);
    IOSCANPVT        ioScanPvt;
    ELLLIST          recList;
    epicsMutexId     lock;
    epicsInt32       eventCode;
    epicsTimeStamp   active_timestamp;
    epicsTimeStamp   current_timestamp;
    epicsInt32       active_pulseID;
    epicsInt32       current_pulseID;
    epicsInt32       diff_pulseID;
    epicsInt32       active_timeslot;
    epicsInt32       current_timeslot;
    epicsUInt32      erTicks;
    evrModifier_ta   active_modifier_a;      /* is not implemented */
    evrModifier_ta   current_modifier_a;     /* implemented but, no device support */

} adTimeStampSource_t, *adTimeStampSource_p;


adTimeStampSource_p        find_timeStampSource(const char *portName);
adTimeStampSource_p        alloc_timeStampSource(const char *portName, const char *functionName);
epicsInt32                 setEventCode_timeStampSource(adTimeStampSource_p pTss, epicsInt32 eventCode);
epicsInt32                 getEventCode_timeStampSource(adTimeStampSource_p pTss);
epicsInt32                 getActivePulseID_timeStampSource(adTimeStampSource_p pTss);
epicsInt32                 getCurrentPulseID_timeStampSource(adTimeStampSource_p pTss);
epicsInt32                 getDiffPulseID_timeStampSource(adTimeStampSource_p pTss);
epicsInt32                 getCurrentTimeSlot_timeStampSource(adTimeStampSource_p pTss);
epicsInt32                 getErTicks_timeStampSource(adTimeStampSource_p pTss);
epicsShareFunc int         listUserTimeStampSource(const char *portName);
epicsShareFunc int         registerUserTimeStampSource(const char *portName, const char *functionName);

#endif
