#include <epicsTime.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <perfMeasure.h>
#include <subRecord.h>            /* for struct subRecord         */
#include "drvMrfEr.h"
#include "evrPattern.h"
#include "evrTime.h"

#include <recSup.h>
#include <dbAccess.h>
#include <dbAccessDefs.h>
#include <ereventRecord.h>

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
epicsMutexId tssMutex;

int tssTimeStampSource = 0;
int tssSourceEvent = 0;
int initialized = 0;

#define TSS_BUFFER_SIZE 25
#define CIRCULAR_INCREMENT(X) (X + 1) % TSS_BUFFER_SIZE
#define CIRCULAR_DECREMENT(X) (TSS_BUFFER_SIZE + X - 1) % TSS_BUFFER_SIZE

typedef struct {
  epicsTimeStamp tssTimeStamp;
  perfParm_ts *tssAcquisitionPerf;
} TssNode;

TssNode tssBuffer[TSS_BUFFER_SIZE];
int tssResyncCount = 0;
int tssBufferHead = 0;
int tssBufferTail = 0;
int tssBufferAcquisitionState = 0;
int tssLostImages = 0;
int tssLostPulseId = 131040;
int tssInvalidPulseIdCount = 0;
int tssBufferFull = 0;
int tssBufferCount = 0; // Number of image acquisitions started, but not yet tagged
int tssLastPulseId = -1;
int tssTriggerReenablePlease = 0;
int tssImageFiducials = 3; // Expected PulseID difference between images, e.g.
                           //  120 Hz acquisition means difference by 3
                           //   60 Hz acquisition means difference by 6

/** Elapsed time in usec (perfMeasure keeps these as doubles) */
int tssElapsed = 0;
int tssElapsedMax = 0;
int tssElapsedMin = 100000;
struct dbAddr tssTriggerAddr;
perfParm_ts *tssRestartPerf = NULL;

perfParm_ts *fromTrig2Acq = NULL;
perfParm_ts *eventPerf = NULL;
int eventPerfFirst = 1;

typedef struct TimeStampNode TimeStampNode;

struct TimeStampNode {
  int index;
  int evrIndex;
  TimeStampNode *next;
  epicsTimeStamp *timeStamp;
  epicsTimeStamp evrTimeStamp;
};
extern TimeStampNode *pTSSList;
TimeStampNode *pNext = NULL;

/**
 * The .ENAB field is 1 when the trigger is "Enabled", and 0 when
 * it is set to "Disable".
 */
static void tssSetTrigger(epicsEnum16 enab) {
  ereventRecord *precord = (ereventRecord *) tssTriggerAddr.precord;

  dbScanLock(tssTriggerAddr.precord);
  precord->enab = enab;
  precord->proc = 1;
  dbProcess(tssTriggerAddr.precord);
  dbScanUnlock(tssTriggerAddr.precord);
}

/**
 * This resets the perfMeasure counters only
 */
static long tssImageAcquisitionReset(subRecord *prec) {
  epicsMutexLock(tssMutex);

  int i = 0;
  for (; i <= TSS_BUFFER_SIZE; ++i) {
    resetPerfMeasure(tssBuffer[i].tssAcquisitionPerf);
  }
  tssElapsedMax = 0;
  tssElapsedMin = 1000000;

  epicsMutexUnlock(tssMutex);

  return 0;
}

/**
 * This function gets called whenever the PV $(DEV):Acquire changes
 */
static long tssImageAcquisition(subRecord *prec) {
  int acquisitionState = prec->a;

  epicsMutexLock(tssMutex);

  printf("TSS: tssImageAcquisition(%d, %d)\n", tssBufferAcquisitionState, acquisitionState);

  // Don't reset acquisition if system is already acquiring data
  // The $(DEV):Acquire PV must be off when the system starts
  if (acquisitionState > 0 && tssBufferAcquisitionState > 0) {
    epicsMutexUnlock(tssMutex);
    return 0;
  }

  tssBufferHead = 0;
  tssBufferTail = 0;
  tssBufferCount = 0;

  /** If tssBufferAcquisitionState is 1, then PULSEIDs are recorded */
  tssBufferAcquisitionState = acquisitionState; 

  /** Enable/Disable EVR based on the state of $(DEV):Acquisition PV */
  tssSetTrigger(acquisitionState);

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
struct dbr_enumStrs p;

static long tssStartInit(subRecord *prec) {
  tssMutex = epicsMutexMustCreate();

  char perfMeasureName[30];  
  int i = 0;
  for (i = 0; i < TSS_BUFFER_SIZE; ++i) {
    sprintf(perfMeasureName, "ImageAcquisition-%d", i);
    tssBuffer[i].tssAcquisitionPerf = makePerfMeasure(perfMeasureName, "Time between trigger and image tagging");
  }
  tssBufferTail = 0;
  tssBufferHead = 0;
  tssBufferCount = 0;
  tssRestartPerf = makePerfMeasure("Resync", "Time elapsed after trigger disable");

  fromTrig2Acq = makePerfMeasure("Trig2Acq", "");
  eventPerf = makePerfMeasure("EventIrq", "Time between EVR record callback");
  eventPerfFirst = 1;

  if (dbNameToAddr("EVR:DMP1:PM10:EVENT1CTRL.ENAB", &tssTriggerAddr) != 0) {
    printf("ERROR: Failed to find EVR trigger ENABLE record\n");
  }
  else {
    printf("INFO: Found EVR record\n");
  }

  return 0;
}

/**
 * This function is called by tssStart on every event 140. It records the current
 * PULSEID that is later used to tag the image (by PipelinedTimeStampSource
 * function)
 */
int getPulseId(int sourceEvent) {
  epicsTimeStamp timeStamp;
  int status = evrTimeGet(&timeStamp, sourceEvent);

  if (tssBufferAcquisitionState == 1) {
    if (pNext == NULL) {
      pNext=pTSSList;
    }
    
    pNext->timeStamp->secPastEpoch=timeStamp.secPastEpoch;
    pNext->timeStamp->nsec=timeStamp.nsec;
    pNext->evrTimeStamp = timeStamp;

    /*
    printf("[%d] sec=%d nsec=%d ptr=0x%xTSS\n",
	   pNext->index,
	   pNext->timeStamp->secPastEpoch,
	   pNext->timeStamp->nsec,
	   pNext->timeStamp);
    */
    
    pNext = pNext->next;
  }

  if (status != 0) {
    printf("ERROR: evrTimeGet() returned non-zero value\n");
    timeStamp.nsec = 0x1FFFF;

    /** Disable EVR and let FIFO flush */
    tssSetTrigger(0);
    tssTriggerReenablePlease = 1;
    tssResyncCount++;

    int count = tssBufferCount;
    tssBufferHead = 0;
    tssBufferTail = 0;
    tssBufferCount = 0;
    

    /*    printf("STOP: evrTimeGet returned non-zero (%d)\n", count);*/
  }

  /**
   * Save timestamp to the head of the buffer and start perfMeasure, only
   * if the camera is enabled (i.e. Acquisition PV is 1)
   */
  if (tssBufferAcquisitionState == 1) {
    if ((timeStamp.nsec & 0x1FFFF) == 0x1FFFF) {
      tssInvalidPulseIdCount++;

      /** Disable EVR and let FIFO flush */
      tssSetTrigger(0);
      tssTriggerReenablePlease = 1;
      tssResyncCount++;

      tssBufferHead = 0;
      tssBufferTail = 0;
      tssBufferCount = 0;
      
	
      return 1;
    }
    else {
      if (tssBufferCount < TSS_BUFFER_SIZE) {
	tssBuffer[tssBufferHead].tssTimeStamp = timeStamp;
	startPerfMeasure(tssBuffer[tssBufferHead].tssAcquisitionPerf);
	startPerfMeasure(fromTrig2Acq);
	tssBufferHead = CIRCULAR_INCREMENT(tssBufferHead);
	tssBufferCount++;
	if (tssBufferCount == TSS_BUFFER_SIZE) {
	  tssBufferFull = 1;
	}
      }
      else {
	tssLostImages++;

	/** Disable EVR and let FIFO flush */
	tssSetTrigger(0);
	tssTriggerReenablePlease = 1;
	tssResyncCount++;
	
	int count = tssBufferCount;
	tssBufferHead = 0;
	tssBufferTail = 0;
	tssBufferCount = 0;
   

	return 1;
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

  if (eventPerfFirst == 1) {
    startPerfMeasure(eventPerf);
    eventPerfFirst = 0;
  }
  else {
    endPerfMeasure(eventPerf);
    calcPerfMeasure(eventPerf);
    startPerfMeasure(eventPerf);
  }

  epicsMutexLock(tssMutex);

  prec->val = getPulseId(sourceEvent);

  epicsMutexUnlock(tssMutex);

  return 0;  
}

/**
 * This function is called on every event 141 (60 Hz)
 */
static long tssRestart(subRecord *prec) {
  /* Does not do anything if reenable not set */
  if (tssTriggerReenablePlease == 0) {
    return 0;
  }

  epicsMutexLock(tssMutex);

  tssSetTrigger(1);
  resetPerfMeasure(eventPerf);
  eventPerfFirst = 1;

  tssTriggerReenablePlease = 0;

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
   * The current pulseId is sitting in the head of the circular buffer (minus one),
   * there is no need to get it again from the EVR.
   */
  currentPulseId = tssBuffer[CIRCULAR_DECREMENT(tssBufferHead)].tssTimeStamp.nsec & 0x1FFFF;

  int difference = ((currentPulseId + pulseIdWrap) - imagePulseId) % pulseIdWrap;

  int lostImages = 0;
  if (difference > tssImageFiducials) {
    /**
     * If difference is greater than tssImageFiducials,
     * then at least one image is lost
     */
    lostImages = (difference-1) / tssImageFiducials;

    /** Report lost images one by one */
    int i = 0;
    for (; i < lostImages; ++i) {
      tssLostPulseId = (imagePulseId + tssImageFiducials + i*tssImageFiducials) % pulseIdWrap; 
      //      printf("%d %d\n", lostImages, tssLostPulseId);
      tssLostImages++;
    }
    scanIoRequest(pTss->slowIoScanPvt);
  }
}

/**
 * Each slot in the tssBuffer keeps the pulseID at the acquisition rate (e.g. 120 Hz).
 * The time separation between each slot is 2.777ms multiplied by the tssImageFiducials.
 * If this function takes too long to get called then the images/pulseids will start
 * getting out of sync. All pulseIds and images received until this point are discarded
 * and the EVR is restarted.
 *
 * @return 0 if times are fine, 1 if there have been delays and fifo was flushed
 */
static int tssSync() {
  /** Don't sync if camera is not being triggered */
  ereventRecord *precord = (ereventRecord *) tssTriggerAddr.precord;
  dbScanLock(tssTriggerAddr.precord);
  int enabled = precord->enab;
  dbProcess(tssTriggerAddr.precord);
  dbScanUnlock(tssTriggerAddr.precord);
  if (enabled < 1) {
    return 0;
  }

  /** This is the time in useconds between PulseIds */
  double fiducial360Hz = 2777.7777;

  /**
   * This is the maximum time delay accepted before we know there are missing
   * images -> 16ms.
   */
  double tolerance = (fiducial360Hz * 3) * 2;

  /**
   * If last elapsed time is more than the tolerance just cleanup past PulseIds
   * from FIFO and start fresh.
   */
  if (tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time > tolerance) {
    /*tssImageAcquisitionReset(NULL);*/
    
    int resetTS = tssBuffer[tssBufferTail].tssTimeStamp.nsec & 0x1FFFF;
    int count = tssBufferCount;
    
    tssBufferHead = 0;
    tssBufferTail = 0;
    tssBufferCount = 0;
    
    tssSetTrigger(0);
    tssTriggerReenablePlease = 1;
    /*      tssSetTrigger(1);*/
    
    //    printf("STOP: tssReset %d (count=%d)\n", resetTS, count);
    
    return 1;
  }

  return 0;
}

/** 
 * This function is called by areaDetector when it is time to time stamp
 * the image.
 */
static void PipelinedTimeStampSource(void *userPvt, epicsTimeStamp *pTimeStamp)
{
  // Disable timestamp setting... (pTSS)
  /*
  epicsTimeStamp ts;
  epicsTimeStamp *pTimeStamp = &ts;
  */
  // End disable (pTSS)

  adTimeStampSource_p pTss = userPvt;
  if(!pTss) return;
  TimeStampSourceProcess(pTss, pTimeStamp);
  epicsMutexLock(tssMutex);
  
  /*   tssSetTrigger(0); // Take a single image! */

  /**
   * There must be some element in the tss buffer, if so the tssBufferTail should 
   * be pointing to the element that contains the image pulseId and the perfMeasure
   */
  if (tssBufferCount > 0) {
    pTimeStamp->secPastEpoch = tssBuffer[tssBufferTail].tssTimeStamp.secPastEpoch;
    pTimeStamp->nsec = tssBuffer[tssBufferTail].tssTimeStamp.nsec;

    /**
     * Make sure the same PulseId is not used again to tag another image.
     * If that is the case use the invalid PulseId. Stop EVR, clear tssBuffer
     * and restart acquisition.
     */
    if (tssLastPulseId == (pTimeStamp->nsec & 0x1FFFF)) {
      pTimeStamp->nsec |= 0x1FFFF;

      tssSetTrigger(0);
      tssTriggerReenablePlease = 1;
      tssResyncCount++;

      tssBufferHead = 0;
      tssBufferTail = 0;
      tssBufferCount = 0;
      

      //      printf("STOP: duplicate %d (h=%d,t=%d,c=%d)\n", tssLastPulseId,
      //	     tssBufferHead, tssBufferTail, tssBufferCount);
    }
    tssLastPulseId = pTimeStamp->nsec & 0x1FFFF;

    endPerfMeasure(tssBuffer[tssBufferTail].tssAcquisitionPerf);
    calcPerfMeasure(tssBuffer[tssBufferTail].tssAcquisitionPerf);

    /**
     * Calculate the elapsed time between the EVR record callback (trigger)
     * and image timestamp callback.
     */
    tssElapsed = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time;
    if (tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_min < tssElapsedMin) {
      tssElapsedMin = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_min;
    } 
    if (tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_max > tssElapsedMax) {
      tssElapsedMax = tssBuffer[tssBufferTail].tssAcquisitionPerf->elapsed_time_max;
    } 
    
    tssCheckPulseId(pTss, pTimeStamp->nsec & 0x1FFFF);

    if (tssSync() == 0) {
      tssBufferTail = CIRCULAR_INCREMENT(tssBufferTail);
      tssBufferFull = 0;
      tssBufferCount--;
    }
    else {
      /** If acquisition took long time, consider image bad */
      pTimeStamp->nsec |= 0x1FFFF;
    }
    
  }
  else {
    /*     printf("ERROR: empty buffer! last id %d\n", tssLastPulseId);*/
    pTimeStamp->nsec = tssBuffer[tssBufferTail].tssTimeStamp.nsec | 0x1FFFF;
  }

  epicsMutexUnlock(tssMutex);
  
  scanIoRequest(pTss->ioScanPvt);
}

epicsRegisterFunction(tssImageAcquisitionReset);
epicsRegisterFunction(tssImageAcquisition);
epicsRegisterFunction(tssStartInit);
epicsRegisterFunction(tssStart);
epicsRegisterFunction(tssRestart);
epicsRegisterFunction(PipelinedTimeStampSource);
