#
# MonoBin4Viewer.cmd
#
# This script creates, configures, and loads db records for one
# monochrome viewer w/ 4x4 binning

########## PCDS standard image w/ ROI plugin ############
# Create an ROI plugin for our standard image
epicsEnvSet( "QSIZE", "5" )
epicsEnvSet( "PLUGIN_SRC", "$(CAM_PORT)" )
# TODO: epicsEnvSet( "PLUGIN_ROI", 4 )
epicsEnvSet( "N", "7" )
< setupScripts/pluginROI.cmd

# Create a Std Image plugin, set to get data from it's ROI plugin
epicsEnvSet( "PLUGIN_SRC", "ROI7" )
epicsEnvSet( "IMAGE_NAME", "THUMBNAIL" )
epicsEnvSet( "IMAGE_FTVL", "UCHAR" )
epicsEnvSet( "IMAGE_TYPE", "Int8" )
< setupScripts/pluginImage.cmd

