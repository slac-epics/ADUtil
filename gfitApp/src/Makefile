TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
DBD += NDPluginStats2.dbd
LIBRARY_IOC += NDPluginStats2
NDPluginStats2_SRCS += NDPluginStats2.cpp
NDPluginStats2_SRCS += gfit.c


NDPluginStats2_LIBS += $(EPICS_BASE_IOC_LIBS)



#include $(AREA_DETECTOR)/ADApp/commonDriverMakefile

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

