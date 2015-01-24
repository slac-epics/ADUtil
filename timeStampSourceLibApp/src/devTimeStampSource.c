/*
 *  November 5, 2013   KHKIM
 *  Demonstrate the user-defined time source function
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cantProceed.h>
#include <alarm.h>
#include <dbDefs.h>
#include <devLib.h>
#include <dbAccess.h>
#include <recSup.h>
#include <devSup.h>
#include <dbScan.h>

#include <epicsPrint.h>

#include <recGbl.h>
#include <longinRecord.h>
#include <longoutRecord.h>

#include <epicsExport.h>

#include "registerTimeStampSource.h"

#define TSSTR_EVENT_CODE        "EventCode"
#define TSSTR_ACTIVE_PULSEID    "ActivePulseID"
#define TSSTR_CURRENT_PULSEID   "CurrentPulseID"
#define TSSTR_DIFF_PULSEID      "DiffPulseID"
#define TSSTR_CURRENT_TIMESLOT  "CurrentTimeSlot"
#define TSSTR_ERTICKS           "ErTicks"


typedef enum {
   TSIDX_undefined,
   TSIDX_event_code,
   TSIDX_active_pulseID,
   TSIDX_current_pulseID,
   TSIDX_diff_pulseID,
   TSIDX_current_timeslot,
   TSIDX_erTicks 
} timestampsource_idx;


typedef struct {
    ELLNODE               node;
    dbCommon              *precord;
    adTimeStampSource_p   pTss;
    timestampsource_idx   idx;
} timestampsource_usrPvt_t, *timestampsource_usrPvt_p; 


void devTimeStampSource_listPV(ELLLIST *plist)
{
    timestampsource_usrPvt_p     pusrPvt;
    dbCommon                     *precord;

    pusrPvt = (timestampsource_usrPvt_p) ellFirst(plist);
    while(pusrPvt) {
        precord = pusrPvt->precord;
        printf("    %s\n", precord->name);
        pusrPvt = (timestampsource_usrPvt_p) ellNext(&pusrPvt->node);
    }
    
}


static long devLiTimeStampSource_init_record(longinRecord *precord);
static long devLiTimeStampSource_get_ioint_info(int cmd, struct dbCommon *precord, IOSCANPVT *pvt_ps);
static long devLiTimeStampSource_read_longin(longinRecord *precord);

struct {
    long        number;
    DEVSUPFUN   report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   read_longin;
    DEVSUPFUN   speical_linconv;
} devLiTimeStampSource = {
    6,
    NULL,
    NULL,
    devLiTimeStampSource_init_record,
    devLiTimeStampSource_get_ioint_info,
    devLiTimeStampSource_read_longin,
    NULL
};

epicsExportAddress(dset, devLiTimeStampSource);

static long devLoTimeStampSource_init_record(longoutRecord *precord);
static long devLoTimeStampSource_write_longout(longoutRecord *precord);

struct {
    long        number;
    DEVSUPFUN   report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   write_longout;
    DEVSUPFUN   special_linconv;
} devLoTimeStampSource = {
    6,
    NULL,
    NULL,
    devLoTimeStampSource_init_record,
    NULL,
    devLoTimeStampSource_write_longout,
    NULL
};

epicsExportAddress(dset, devLoTimeStampSource);

static long devLiTimeStampSource_init_record(longinRecord *precord)
{
    int                       i;
    char                      option_string[2][32];
    timestampsource_usrPvt_p  pusrPvt;
    adTimeStampSource_p       pTss;
    timestampsource_idx       idx = TSIDX_undefined;

    switch(precord->inp.type) {
        case INST_IO:
            i = sscanf(precord->inp.value.instio.string, "%s %s", option_string[0], option_string[1]);
            if(i<2) goto errl;
            if(!(pTss=find_timeStampSource(option_string[0]))) {
                errlogPrintf("Rec(%s) cannot find port for the timestamp source registry %s\n",
                             precord->name, option_string[0]);
                goto errl;
             }
             if(!strcmp(TSSTR_EVENT_CODE, option_string[1]))            idx = TSIDX_event_code;
             else if(!strcmp(TSSTR_ACTIVE_PULSEID,   option_string[1])) idx = TSIDX_active_pulseID;
             else if(!strcmp(TSSTR_CURRENT_PULSEID,  option_string[1])) idx = TSIDX_current_pulseID;
             else if(!strcmp(TSSTR_DIFF_PULSEID,     option_string[1])) idx = TSIDX_diff_pulseID;
             else if(!strcmp(TSSTR_CURRENT_TIMESLOT, option_string[1])) idx = TSIDX_current_timeslot;
             else if(!strcmp(TSSTR_ERTICKS,          option_string[1])) idx = TSIDX_erTicks;
             else {
                 errlogPrintf("Rec(%s) cannot find parameter in the timesource registry (%s, %s)\n"
                              "or, does not support the parameter.\n",
                              precord->name, option_string[0], option_string[1]);
                 goto errl;
             }

            break;

        default:
            errl:
            recGblRecordError(S_db_badField, (void*)precord,
                              "devLiTimeStampSource (init_record) Illegal INP field");
            precord->udf = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

    pusrPvt = mallocMustSucceed(sizeof(timestampsource_usrPvt_t), "Data Structure for devLiTimeStampSource");
    pusrPvt->precord = precord;
    pusrPvt->pTss = pTss;
    pusrPvt->idx  = idx;
    
    precord->udf  = FALSE;
    precord->dpvt = (void*) pusrPvt;

    ellAdd(&pTss->recList, &pusrPvt->node);

    return 2;
}

static long devLiTimeStampSource_get_ioint_info(int cmd, struct dbCommon *precord, IOSCANPVT *pvt_ps)
{
    timestampsource_usrPvt_p     pusrPvt;
    adTimeStampSource_p          pTss;

    if(!precord->dpvt) return -1;


    pusrPvt = precord->dpvt;
    pTss    = pusrPvt->pTss;
    *pvt_ps = pTss->ioScanPvt;

    return 0;

}

static long devLiTimeStampSource_read_longin(longinRecord *precord)
{
    timestampsource_usrPvt_p   pusrPvt;
    adTimeStampSource_p        pTss;
    timestampsource_idx        idx;

    if(!precord->dpvt) {
        errlogPrintf("Rec(%s) does not have user private data.\n", precord->name);
        goto errl;
     }

    pusrPvt = precord->dpvt;
    pTss    = pusrPvt->pTss;
    idx     = pusrPvt->idx;

    switch(idx) {
        case TSIDX_event_code:
            precord->val = getEventCode_timeStampSource(pTss);
            break;
        case TSIDX_active_pulseID:
            precord->val = getActivePulseID_timeStampSource(pTss);
            break;
        case TSIDX_current_pulseID:
            precord->val = getCurrentPulseID_timeStampSource(pTss);
            break;
        case TSIDX_diff_pulseID:
            precord->val = getDiffPulseID_timeStampSource(pTss);
            break;
        case TSIDX_current_timeslot:
            precord->val = getCurrentTimeSlot_timeStampSource(pTss);
            break;
        case TSIDX_erTicks:
            precord->val = getErTicks_timeStampSource(pTss);
            break;

        default:
            goto errl;
            break;
    }

    precord->time = pTss->active_timestamp;
    return 0;


    errl: 
        precord->udf = TRUE;
        return -1;
}


static long devLoTimeStampSource_init_record(longoutRecord *precord)
{
    int                         i;
    char                        option_string[2][32];
    timestampsource_usrPvt_p    pusrPvt;
    adTimeStampSource_p         pTss;
    timestampsource_idx         idx = TSIDX_undefined;

    switch(precord->out.type) {
        case INST_IO:
            i = sscanf(precord->out.value.instio.string, "%s %s", option_string[0], option_string[1]);
            if(i < 2) goto errl;
            if(!(pTss = find_timeStampSource(option_string[0]))) {
                errlogPrintf("Rec(%s) cannnot find port for the timestamp source registry %s\n",
                              precord->name, option_string[0]);
                goto errl;
            }
            if(!strcmp(TSSTR_EVENT_CODE, option_string[1])) idx = TSIDX_event_code;
            else {
                errlogPrintf("Rec(%s) cannot find parameter in the timesource registry (%s, %s)\n"
                             "or, does not support the paramter.\n",
                             precord->name, option_string[0], option_string[1]);
                goto errl;
            }
            break;

        default:
            errl:
            recGblRecordError(S_db_badField, (void*) precord,
                              "devLoTimeStampSource (init_record) Illegal OUT field");
            precord->udf = TRUE;
            precord->dpvt = NULL;
            return S_db_badField;
    }

    pusrPvt = mallocMustSucceed(sizeof(timestampsource_usrPvt_t), "Data Structure for devLoTimeStampSource");
    pusrPvt->precord = precord;
    pusrPvt->pTss = pTss;
    pusrPvt->idx  = idx;


    precord->udf  = FALSE;
    precord->dpvt = (void*) pusrPvt;

    ellAdd(&pTss->recList, &pusrPvt->node);
    return 2;
}

static long devLoTimeStampSource_write_longout(longoutRecord *precord)
{
    timestampsource_usrPvt_p     pusrPvt;
    adTimeStampSource_p          pTss;
    timestampsource_idx          idx;

    if(!precord->dpvt) {
        errlogPrintf("Rec(%s) does not have user private data.\n", precord->name);
        goto errl;
    }

    pusrPvt = precord->dpvt;
    pTss    = pusrPvt->pTss;
    idx     = pusrPvt->idx;


    switch(idx) {
       case TSIDX_event_code:
           setEventCode_timeStampSource(pTss, precord->val);
           break;

        default:
            goto errl;
            break;

    }

    
    return 0;


    errl:
        precord->udf = TRUE;
        return -1;
}


