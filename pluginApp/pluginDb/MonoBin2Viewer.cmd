#
# MonoBin2Viewer.cmd
#
# This script creates, configures, and loads db records for one
# monochrome viewer w/ 2x2 binning

########## PCDS standard image w/ ROI plugin ############
# Create an ROI plugin for our standard image
epicsEnvSet( "QSIZE", "5" )
epicsEnvSet( "PLUGIN_SRC", "$(CAM_PORT)" )
# TODO: epicsEnvSet( "PLUGIN_ROI", 2 )
epicsEnvSet( "N", "6" )
< setupScripts/pluginROI.cmd

# Create a Std Image plugin, set to get data from it's ROI plugin
epicsEnvSet( "PLUGIN_SRC", "ROI6" )
epicsEnvSet( "IMAGE_NAME", "IMAGE2" )
epicsEnvSet( "IMAGE_FTVL", "SHORT" )
epicsEnvSet( "IMAGE_TYPE", "Int16" )
< setupScripts/pluginImage.cmd

