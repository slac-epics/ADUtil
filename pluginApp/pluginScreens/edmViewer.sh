#! /bin/bash
#
# EDM Viewer launch script
#
# To use, define the following environment variables
#   IOC	    EPICS PV prefix for iocAdmin PV's
#   P 	    Prefix for Camera PV's
#   R 	    Region for Camera PV's
#			Full camera PV prefix: $(P)$(R)
#	IMAGE	Image name
#			Full image PV prefix: $(P)$(R)$(IMAGE)
# Example:
# IOC=$(IOC) P=$(P) R=$(R) ADStreamScreens/edmViewer.sh

# 

# Setup edm environment
# export EPICS_HOST_ARCH=linux-x86
# source /reg/g/pcds/setup/epicsenv-3.14.9.sh
source /reg/g/pcds/setup/epicsenv-3.14.12.sh
export EPICS_CA_MAX_ARRAY_BYTES=10000000

IMG_SIZE_X=`caget -t -f0 ${P}${R}${IMAGE}:ArraySize0_RBV`
IMG_SIZE_Y=`caget -t -f0 ${P}${R}${IMAGE}:ArraySize1_RBV`
IMG_N_BITS=`caget -t -f0 ${P}${R}${IMAGE}:BitsPerPixel_RBV`

# Contstants derived from widget positions and sizes in template files
if [ ${IMG_SIZE_Y} -gt 500 ]; then
	TEMPLATE=landscapeViewerTemplate.edl
	V_MIN_WIDTH=800
	V_MIN_HEIGHT=552
	V_IMG_X=200
	V_IMG_Y=48
else
	TEMPLATE=wideViewerTemplate.edl
	V_MIN_WIDTH=952
	V_MIN_HEIGHT=180
	V_IMG_X=4
	V_IMG_Y=180
fi

# Compute viewer width and height
V_WIDTH=$((  ${V_IMG_X} + ${IMG_SIZE_X} + 4 ))
V_HEIGHT=$(( ${V_IMG_Y} + ${IMG_SIZE_Y} + 4 ))

# Check against minimums
V_WIDTH=$((  ${V_MIN_WIDTH}  > ${V_WIDTH}  ? ${V_MIN_WIDTH}  : ${V_WIDTH}  ))
V_HEIGHT=$(( ${V_MIN_HEIGHT} > ${V_HEIGHT} ? ${V_MIN_HEIGHT} : ${V_HEIGHT} ))

# Create a custom viewer in /tmp
cat	ADStreamScreens/${TEMPLATE} | sed \
	-e "s/^w 999/w ${V_WIDTH}/"	\
	-e "s/^h 999/h ${V_HEIGHT}/"	\
	-e "s/^w 699/w ${IMG_SIZE_X}/"	\
	-e "s/^h 699/h ${IMG_SIZE_Y}/"	\
	-e "s/^nBitsPerPixel 8/nBitsPerPixel ${IMG_N_BITS}/"	\
	> /tmp/${IMG_SIZE_X}x${IMG_SIZE_Y}x${IMG_N_BITS}Viewer.edl

# Make custom viewer writable by everyone to avoid permission issues
chmod a+w /tmp/${IMG_SIZE_X}x${IMG_SIZE_Y}x${IMG_N_BITS}Viewer.edl

# Launch custom viewer
edm -x -eolc						\
	-m "IOC=${IOC}"					\
	-m "P=${P},R=${R}"				\
	-m "CAM=${P}"					\
	-m "HUTCH=${HUTCH}"				\
	-m "IMAGE=${IMAGE}"				\
	-m "IMG_SIZE_X=${IMG_SIZE_X}"	\
	-m "IMG_SIZE_Y=${IMG_SIZE_Y}"	\
	-m "IMG_N_BITS=${IMG_N_BITS}"	\
	/tmp/${IMG_SIZE_X}x${IMG_SIZE_Y}x${IMG_N_BITS}Viewer.edl	\
	&

