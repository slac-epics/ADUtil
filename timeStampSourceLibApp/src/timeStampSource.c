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

int tssTimeStampSource = 0;
int tssSourceEvent = 0;
int initialized = 0;

#define TSS_BUFFER_SIZE 5
#define CIRCULAR_INCREMENT(X) (X + 1) % TSS_BUFFER_SIZE
#define CIRCULAR_DECREMENT(X) (TSS_BUFFER_SIZE + X - 1) % TSS_BUFFER_SIZE

typedef struct {
  epicsTimeStamp tssTimeStamp;
  perfParm_ts *tssAcquisitionPerf;
} TssNode;

TssNode tssBuffer[TSS_BUFFER_SIZE];
int tssBufferHead = 0;
int tssBufferTail = 0;
int tssBufferAcquisitionState = 0;
int tssLostImages = 0;
int tssLostPulseId = 131040;
int tssInvalidPulseIdCount = 0;
int tssBufferFull = 0;
int tssBufferCount = 0;

/** Elapsed time in usec (perfMeasure keeps these as doubles) */
int tssElapsed = 0;
int tssElapsedMax = 0;
int tssElapsedMin = 100000;

static long tssImageAcquisitionReset(subRecord *prec) {
  epicsMutexLock(tssMutex);

  int i = 0;
  for (; i <= TSS_BUFFER_SIZE; ++i) {
    resetPerfMeasure(tssBuffer[i].tssAcquisitionPerf);
  }
  tssElapsedMax = 0;
  tssElapsedMin = 1000000;

  epicsMutexUnlock(tssMutex);
}

static long tssImageAcquisition(subRecord *prec) {
  int acquisitionState = prec->a;

  epicsMutexLock(tssMutex);

  tssBufferHead = 0;
  tssBufferTail = 0;
  tssBufferCount = 0;

  /** If tssBufferAcquisitionState is 1, then PULSEIDs are recorded */
  tssBufferAcquisitionState = acquisitionState; 

  epicsMutexUnlock(tssMutex);

  return 0;
}


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

  char perfMeasureName[30];  
  int i = 0;
  for (i = 0; i < TSS_BUFFER_SIZE; ++i) {
    sprintf(perfMeasureName, "ImageAcquisition-%d", i);
    tssBuffer[i].tssAcquisitionPerf = makePerfMeasure(perfMeasureName, "Time between trigger and image tagging");
  }
  tssBufferTail = 0;
  tssBufferHead = 0;
  tssBufferCount = 0;

  return 0;
}

/**
 * This function is called by tssStart on every event 140. It records the current
 * PULSEID that is later used to tag the image (by PipelinedTimeStampSource
 * function)
 */
int getPulseId(int sourceEvent) {
  epicsTimeStamp timeStamp;
  epicsFloat64 elapsedTime = 0;

  evrTimeGet(&timeStamp, sourceEvent);
  evrTimeGetFromPipeline(&timeStamp, evrTimeCurrent, 0, 0,0,0,0);

  /**
   * Save timestamp to the head of the buffer and start perfMeasure, only
   * if the camera is enabled (i.e. Acquisition PV is 1)
   */
  if (tssBufferAcquisitionState == 1) {
    /**
     * If an invalid PULSEID was received, then skip the buffer
     */
    if ((timeStamp.nsec & 0x1FFFF) == 0x1FFFF) {
      tssInvalidPulseIdCount++;
    }
    else {
      /** Add new element if there is space */
      if (tssBufferCount < TSS_BUFFER_SIZE) {
	tssBuffer[tssBufferHead].tssTimeStamp = timeStamp;
	startPerfMeasure(tssBuffer[tssBufferHead].tssAcquisitionPerf);
	tssBufferHead = CIRCULAR_INCREMENT(tssBufferHead);
	tssBufferCount++;
	if (tssBufferCount == TSS_BUFFER_SIZE) {
	  tssBufferFull = 1;
	}
      }
      else {
	printf("buffer full, overflowing! (loosing PulseId %d)\n", timeStamp.nsec & 0x1FFFF);
      }
    }
  }

  return 0;
}

/**
 * This function is called on every event 140
 */
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

/**
 * This function is called at the time an image is timestamped. The difference
 * between the timestamping pulseId and the current active pulseId must be:
 * - zero: if the image is transferred and tagged within 8.333ms
 * - three: if the image is transferred within 8.333ms and processed within
 *          another 8.333ms
 * If the difference is greater than 3, then images have been lost!
 */
static void tssCheckPulseId(adTimeStampSource_p pTss, int imagePulseId) {
  int sourceEvent = 140;
  epicsTimeStamp timeStamp;
  int currentPulseId;
  int pulseIdWrap = 131040;

  /**
   * The current pulseId is sitting in the head of the circular buffer,
   * there is no need to get it again from the EVR.
   */
  currentPulseId = tssBuffer[CIRCULAR_DECREMENT(tssBufferHead)].tssTimeStamp.nsec & 0x1FFFF;

  int difference = ((currentPulseId + pulseIdWrap) - imagePulseId) % pulseIdWrap;

  int lostImages = 0;
  if (difference > 3) {
    /** If difference is 6, then one image is lost */
    lostImages = (difference-1) / 3;

    /** Report lost images one by one */
    int i = 0;
    for (; i < lostImages; ++i) {
      tssLostPulseId = (imagePulseId + 3 + i*3) % pulseIdWrap; 
      /*      printf("%d %d\n", lostImages, tssLostPulseId);*/
      tssLostImages++;
    }
    scanIoRequest(pTss->slowIoScanPvt);
  }
}

/**
 * Each slot in the tssBuffer keeps the pulseID at 120 Hz. The time separation
 * between each slot is 8.333 ms. If this function takes too long to get
 * called then the images/pulseids will start getting out of sync. The best 
 * approximation of to get thinks back in sync is discard the pulseids following 
 * the one just used up. E.g. if perfMeasure says it took 30ms between the 
 * pulseid and the tagging, then remove 30/8.333=3.6 -> three pulseIDs, i.e.
 * increase the tail by that much (reseting perfMeasures that won't be used). It
 * may be useful to check the latest readout times to define if there should
 * be one or two pulseIds left in the buffer.
 */
static void tssSync() {
  /** This is the time in useconds between PulseIds */
  double fiducial = 8333.3333;

  /**
   * This is the maximum time delay accepted before we know there are missing
   * images.
   */
  double tolerance = fiducial * 2;

  if (tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time > tolerance) {
    printf("Long elapsed time detected: %f (%d)",
	   tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time,
	   tssBuffer[tssBufferTail].tssTimeStamp.nsec & 0x1FFFF);
    
    double takeAway = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time - tolerance;

    do {
      tssBufferTail = CIRCULAR_INCREMENT(tssBufferTail);
      endPerfMeasure(tssBuffer[tssBufferTail].tssAcquisitionPerf);
      takeAway -= fiducial;
      tssBufferCount--;
      printf(".");
    } while (takeAway > fiducial);
    printf("\n");
  }
}

/** 
 * This function is called by areaDetector when it is time to time stamp
 * the image.
 */
static void PipelinedTimeStampSource(void *userPvt, epicsTimeStamp *pTimeStamp)
{
   adTimeStampSource_p pTss = userPvt;
   if(!pTss) return;
   TimeStampSourceProcess(pTss, pTimeStamp);

   epicsMutexLock(tssMutex);
 
   if (tssBufferCount > 0) {
     pTimeStamp->secPastEpoch = tssBuffer[tssBufferTail].tssTimeStamp.secPastEpoch;
     pTimeStamp->nsec = tssBuffer[tssBufferTail].tssTimeStamp.nsec;
     endPerfMeasure(tssBuffer[tssBufferTail].tssAcquisitionPerf);
     calcPerfMeasure(tssBuffer[tssBufferTail].tssAcquisitionPerf);

     tssElapsed = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time;
     if (tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_min < tssElapsedMin) {
       tssElapsedMin = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_min;
     } 
     if (tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_max > tssElapsedMax) {
       tssElapsedMax = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_max;
     } 
     
     tssCheckPulseId(pTss, pTimeStamp->nsec & 0x1FFFF);
     tssSync();
     
     tssBufferTail = CIRCULAR_INCREMENT(tssBufferTail);
     tssBufferFull = 0;
     tssBufferCount--;
   }
   
   epicsMutexUnlock(tssMutex);
   
   scanIoRequest(pTss->ioScanPvt);
}

epicsRegisterFunction(tssImageAcquisitionReset);
epicsRegisterFunction(tssImageAcquisition);
epicsRegisterFunction(tssStartInit);
epicsRegisterFunction(tssStart);
epicsRegisterFunction(PipelinedTimeStampSource);
