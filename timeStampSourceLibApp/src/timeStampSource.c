#include <epicsTime.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <perfMeasure.h>
#include <subRecord.h>            /* for struct subRecord         */
#include "drvMrfEr.h"
#include "evrPattern.h"
#include "evrTime.h"


#include "registerTimeStampSource.h"

/*
 *  November 5, 2013   KHKIM
 *  Demonstrate the user-defined time source function
 */

static void TimeStampSourceProcess(adTimeStampSource_p pTss, epicsTimeStamp *pTimeStamp)
{
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
}

static void TimeStampSource(void *userPvt, epicsTimeStamp *pTimeStamp)
{
   adTimeStampSource_p  pTss = userPvt;
   if(!pTss) return;
   TimeStampSourceProcess(pTss, pTimeStamp);

   scanIoRequest(pTss->ioScanPvt);
}

epicsRegisterFunction(TimeStampSource);

/**
 * The following variables are used for the pipelined TSS
 */
perfParm_ts *tssAcquisitionPerf;
perfParm_ts *tssStartPerf;
int tssAcquisitionPerfStarted = 0;
epicsMutexId tssMutex;

epicsTimeStamp tssTimeStamp0;
epicsTimeStamp tssTimeStamp1;

int tssTimeStampSource = 0;
int tssSourceEvent = 0;
int initialized = 0;

/**
 * Subroutine record initialization function invoked by the $(DEV):TSS_START
 * record. This routine starts a perfMeasure measurement point, that gets
 * started when $(DEV):TSS_START is processed and ends when the 
 * PipelinedTimeStampSource() function is called by AD when the image is
 * ready to be timestamped.
 */
static long tssStartInit(subRecord *prec) {
  tssMutex = epicsMutexMustCreate();
  tssAcquisitionPerf = makePerfMeasure("ImageAcquisition", "Time between trigger and image tagging");
  tssStartPerf = makePerfMeasure("TriggerPeriod", "Delay between trigger events");

  return 0;
}

int getPulseId(int sourceEvent) {
  epicsTimeStamp *timeStamp;
  int is120Hz = 0;
  epicsFloat64 elapsedTime = 0;

  if (tssTimeStampSource == 0) {
    timeStamp = &tssTimeStamp0;
  }
  else {
    timeStamp = &tssTimeStamp1;
  }

  evrTimeGet(timeStamp, sourceEvent);
  evrTimeGetFromPipeline(timeStamp, evrTimeCurrent, 0, 0,0,0,0);

  /* Record how long it is taking between tssStart() calls */
  if (initialized == 0) {
    startPerfMeasure(tssStartPerf);
    initialized = 1;
  }
  else {
    /* Finish the measurement started on the previous cycle and calculate */
    endPerfMeasure(tssStartPerf);
    calcPerfMeasure(tssStartPerf);

    /* Start measurement for next cycle */
    startPerfMeasure(tssStartPerf);

    /**
     * If it takes less than 10ms then beam rate is 120Hz. In this case
     * the image must be tagged with the PULSEID stored in the previous call.
     */
    if (tssStartPerf->elapsed_time < 14000) {
      is120Hz = 1; 
    }
  }
  
  /*  if (sourceEvent == 140) {*/
  if (is120Hz != 0) { 
    tssTimeStampSource = !tssTimeStampSource;
  }

  /** Returns 1 if time delay indicates 120Hz operation, or 0 if event rate is lower */
  return is120Hz;
}

static long tssStart(subRecord *prec) {
  int sourceEvent = prec->a;
  tssSourceEvent = sourceEvent;

  epicsMutexLock(tssMutex);

  prec->val = getPulseId(sourceEvent);

  if (tssAcquisitionPerfStarted == 0) {
    startPerfMeasure(tssAcquisitionPerf);
    tssAcquisitionPerfStarted = 1;
  }
  epicsMutexUnlock(tssMutex);

  return 0;  
}

static void PipelinedTimeStampSource(void *userPvt, epicsTimeStamp *pTimeStamp)
{
   adTimeStampSource_p  pTss = userPvt;
   if(!pTss) return;
   TimeStampSourceProcess(pTss, pTimeStamp);

   epicsMutexLock(tssMutex);

   if (tssTimeStampSource == 0) {
     pTimeStamp->secPastEpoch = tssTimeStamp0.secPastEpoch;
     pTimeStamp->nsec = tssTimeStamp0.nsec;
   }
   else {
     pTimeStamp->secPastEpoch = tssTimeStamp1.secPastEpoch;
     pTimeStamp->nsec = tssTimeStamp1.nsec;
   }

   if (tssAcquisitionPerfStarted != 0)  {
     if (tssAcquisitionPerfStarted == 1 && tssSourceEvent == 140) {
       tssAcquisitionPerfStarted++;
     }
     else {
       endPerfMeasure(tssAcquisitionPerf);
       tssAcquisitionPerfStarted = 0;
       calcPerfMeasure(tssAcquisitionPerf);
     }
   }

   epicsMutexUnlock(tssMutex);

   scanIoRequest(pTss->ioScanPvt);
}

epicsRegisterFunction(tssStartInit);
epicsRegisterFunction(tssStart);
epicsRegisterFunction(PipelinedTimeStampSource);
