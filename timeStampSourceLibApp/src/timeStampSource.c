#include <epicsTime.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include "drvMrfEr.h"
#include "evrPattern.h"
#include "evrTime.h"


#include "registerTimeStampSource.h"

/*
 *  November 5, 2013   KHKIM
 *  Demonstrate the user-defined time source function
 */


static void TimeStampSource(void *userPvt, epicsTimeStamp *pTimeStamp)
{

   adTimeStampSource_p  pTss = userPvt;

   if(!pTss) return;

   epicsMutexLock (pTss->lock);

   ErGetTicks(0, &pTss->erTicks);
   evrTimeGet(pTimeStamp, pTss->eventCode); 
   evrTimeGetFromPipeline(&pTss->current_timestamp,  evrTimeCurrent, pTss->current_modifier_a, 0,0,0,0);

   pTss->active_timestamp  = *pTimeStamp;
   pTss->current_timeslot  = TIMESLOT(pTss->current_modifier_a);
   pTss->active_pulseID    = PULSEID(pTss->active_timestamp);
   pTss->current_pulseID   = PULSEID(pTss->current_timestamp);
   pTss->diff_pulseID      = pTss->current_pulseID - pTss->active_pulseID;

   epicsMutexUnlock (pTss->lock);

   scanIoRequest(pTss->ioScanPvt);
}

epicsRegisterFunction(TimeStampSource);


