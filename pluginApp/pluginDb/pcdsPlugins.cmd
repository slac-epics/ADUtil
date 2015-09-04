#
# pcdsPlugins.cmd
#
# This script creates, configures, and loads db records for our
# default set of plugins

# First load the common plugins
< setupScripts/commonPlugins.cmd

########## PCDS standard image w/ ROI plugin ############
# Create an ROI plugin for our standard image
epicsEnvSet( "QSIZE", "5" )
epicsEnvSet( "PLUGIN_SRC", "$(CAM_PORT)" )
epicsEnvSet( "N", "5" )
< setupScripts/pluginROI.cmd

# Create a Std Image plugin, set to get data from it's ROI plugin
epicsEnvSet( "PLUGIN_SRC", "ROI5" )
epicsEnvSet( "IMAGE_NAME", "IMAGE1" )
< setupScripts/pluginImage.cmd

########## Binned image w/ overlay plugins ############
# Create an ROI plugin for Binned images
epicsEnvSet( "PLUGIN_SRC", "$(CAM_PORT)" )
epicsEnvSet( "N", "6" )
< setupScripts/pluginROI.cmd

# Create an Overlay plugin, set it to get data from ROI6
epicsEnvSet( "PLUGIN_SRC", "ROI6" )
epicsEnvSet( "N", "2" )
< setupScripts/pluginOverlay.cmd

# Create a Std Image plugin for binned images
# Set it to get data from it's overlay plugin
epicsEnvSet( "PLUGIN_SRC", "Over2" )
epicsEnvSet( "IMAGE_NAME", "IMG_OVER2" )
epicsEnvSet( "IMAGE_FTVL", "CHAR" )
epicsEnvSet( "IMAGE_TYPE", "Int8" )
< setupScripts/pluginImage.cmd

########## Thumbnail image plugin ############
# Create an ROI plugin for thumbnail images
epicsEnvSet( "PLUGIN_SRC", "$(CAM_PORT)" )
epicsEnvSet( "N", "7" )
< setupScripts/pluginROI.cmd

# Create a Std Image plugin for thumbnail images
# Set it to get data from it's ROI plugin
epicsEnvSet( "PLUGIN_SRC", "ROI7" )
epicsEnvSet( "IMAGE_NAME", "THUMBNAIL" )
epicsEnvSet( "IMAGE_FTVL", "CHAR" )
epicsEnvSet( "IMAGE_TYPE", "Int8" )
< setupScripts/pluginImage.cmd

# Create a FileMPEG plugin, set it to get data from our std image
epicsEnvSet( "PLUGIN_SRC", "IMAGE1" )
epicsEnvSet( "N", "1" )
< setupScripts/pluginFileMPEG.cmd

# Create an MJPG plugin, set it to get data from our std image
epicsEnvSet( "PLUGIN_SRC", "IMAGE1" )
epicsEnvSet( "N", "1" )
< setupScripts/pluginMJPG.cmd
