/* ticks.c, 2006-07-25 */
/* current author: Damir Franetic */

/* Author: ELW: 2014-Jan-24 */
/* Changes source file name: axisTicks.c */
#include <stdio.h>
#include <dbEvent.h>
#include <dbDefs.h>
#include <dbCommon.h>
#include <recSup.h>
#include <aSubRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

int axisTicks(aSubRecord *precord)
{	
	unsigned long idx,N,ticksNum;
	float min,max,start;
	
	idx = 0;
	N=*(unsigned long *)precord->a;
	min=*(float *)precord->b;
	max=*(float *)precord->c;
        start=*(float *)precord->d;
        ticksNum=*(unsigned long *)precord->e;	

	while(idx < N)
	{
	  if (idx<=ticksNum)	       
		*((float *)precord->vala + idx) = (idx + 1)*(max-min)/N + start;
		
	  else
		*((float *)precord->vala + idx) = 100;				
	  idx++;		
	}
	
	return 0;
}

epicsRegisterFunction(axisTicks);
