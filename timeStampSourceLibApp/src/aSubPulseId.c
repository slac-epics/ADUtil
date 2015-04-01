#include <aSubRecord.h>           /* for sgtruct aSubRecord */
#include <registryFunction.h>     /* for epics register function  */
#include <epicsExport.h>          /* for epicsExport              */


#define LOWER_17BITS_MASK    (0x0001ffff)   /* mask for pulse id */

static long aSubPulseIdInit(aSubRecord *prec)
{
    prec->udf = 0;

    return 0;
}

static long aSubPulseId(aSubRecord *prec)
{
    int i, *a, *v;

    a = (int *) prec->a;
    v = (int *) prec->vala;

    for(i = 0; i < prec->nea; i++) {
        v[i] = a[i] & LOWER_17BITS_MASK;
    }
    for( ; i < prec->nova ;i++) {
        v[i] =0;
    }
    prec->neva = prec->nea;

    return 0;
}


epicsRegisterFunction(aSubPulseIdInit);
epicsRegisterFunction(aSubPulseId);
