TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this

# Create and install plugin DB Files
DB += pluginColorConvert.db
#DB += pluginFileMPEG.db
DB += pluginHDF5.db
DB += pluginImage.db
DB += pluginJPEG.db
DB += pluginMagick.db
#DB += pluginMJPG.db
DB += pluginNetCDF.db
DB += pluginNexus.db
DB += pluginOverlay.db
DB += pluginProcess.db
DB += pluginROI.db
DB += pluginStats.db
DB += pluginTIFF.db
DB += pluginTransform.db

# Install viewer cmd scripts
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/commonPlugins.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/MonoBin2Viewer.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/MonoBin4Viewer.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/MonoFullViewer.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pcdsPlugins.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginColorConvert.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginFileMPEG.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginHDF5.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginImage.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginJPEG.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginMagick.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginMJPG.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginNetCDF.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginNexus.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginOverlay.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginProcess.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginROI.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginStats.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginTIFF.cmd
DB_INSTALLS += $(TOP)/pluginApp/pluginDb/pluginTransform.cmd

#----------------------------------------------------
# Create and install (or just install)
# Archive files, templates, substitutions like this
#

# Create and install plugin archive Files
#ARCHIVE += pluginColorConvert.archive
##ARCHIVE += pluginFileMPEG.archive
#ARCHIVE += pluginHDF5.archive
#ARCHIVE += pluginImage.archive
#ARCHIVE += pluginJPEG.archive
##ARCHIVE += pluginMJPG.archive
#ARCHIVE += pluginMagick.archive
#ARCHIVE += pluginNetCDF.archive
#ARCHIVE += pluginNexus.archive
#ARCHIVE += pluginOverlay.archive
#ARCHIVE += pluginProcess.archive
#ARCHIVE += pluginROI.archive
#ARCHIVE += pluginStats.archive
#ARCHIVE += pluginTIFF.archive
#ARCHIVE += pluginTransform.archive

#----------------------------------------------------
# Create and install (or just install)
# Autosave request files, templates, substitutions like this
#

# Create and install plugin autosave Files
#REQ += pluginColorConvert.req
##REQ += pluginFileMPEG.req
#REQ += pluginHDF5.req
#REQ += pluginImage.req
#REQ += pluginJPEG.req
##REQ += pluginMJPG.req
#REQ += pluginMagick.req
#REQ += pluginNetCDF.req
#REQ += pluginNexus.req
#REQ += pluginOverlay.req
#REQ += pluginProcess.req
#REQ += pluginROI.req
#REQ += pluginStats.req
#REQ += pluginTIFF.req
#REQ += pluginTransform.req

# These must be after the individual plugins
#ARCHIVE += pcdsPlugins.archive
#REQ 	+= pcdsPlugins.req

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

