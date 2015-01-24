TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#========================================

# ================================================
# Build the IOC application
# ================================================
# This will attempt to build an IOC application 
# for every target including the host computer.
LIBRARY_IOC = axisTicks

# axisTicks.dbd will be installed
DBD += axisTicks.dbd

# ========================================================================
# Add locally compiled object code
# For example, subroutines
# ========================================================================
axisTicks_SRCS += axisTicks.c
#=========================================================================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
