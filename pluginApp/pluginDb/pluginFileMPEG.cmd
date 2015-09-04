#
# pluginFileMPEG.cmd
#
# Required env vars
#	CAM			- PV Prefix for all camera related PV's
#	N			- Plugin number, must be unique if more than one FileMPEG plugin
#	PLUGIN_SRC	- Which port should this plugin get its data from

# Configure the plugin
ffmpegFileConfigure( "FMPG$(N)", 16, 0, "$(PLUGIN_SRC)", 0, 0)

# Load the plugin records
dbLoadRecords( "db/pluginFileMPEG.db",  "CAM=$(CAM_PV),CAM_PORT=$(CAM_PORT),PLUGIN_SRC=$(PLUGIN_SRC),N=$(N),IMAGE_BIT_DEPTH=$(IMAGE_BIT_DEPTH)" )
