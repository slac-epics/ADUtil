TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC += timeStampSource

DBD += timeStampSource.dbd
INC += registerTimeStampSource.c

timeStampSource_DBD += timeStampSource.dbd

timeStampSource_SRCS += timeStampSource.c
timeStampSource_SRCS += registerTimeStampSource.c
timeStampSource_SRCS += devTimeStampSource.c

# Remove below:  Kukhee please confirm.
# EPICS BASE only needs to be linked in for an IOC application:
#timeStampSource_LIBS += $(EPICS_BASE_IOC_LIBS)


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

