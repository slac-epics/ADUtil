#
# pluginMJPG.cmd
#
# Required env vars
#	CAM			- PV Prefix for all camera related PV's
#	N			- Plugin number, must be unique if more than one MJPG plugin
#	PLUGIN_SRC	- Which port should this plugin get its data from
#	HTTP_PORT	- Server HTTP port number

# Configure the plugin
# ffmpegServerConfigure( serverHttpPort )
ffmpegServerConfigure( $(HTTP_PORT) )

# ffmpegStreamConfigure( portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, maxMemory )
# Set maxMemory  to 0 for unlimited memory allocation
ffmpegStreamConfigure( "MJPG$(N)", 2, 0, "$(PLUGIN_SRC)", 0, 0)

# Load the plugin records
dbLoadRecords( "db/pluginMJPG.db",  "CAM=$(CAM_PV),CAM_PORT=$(CAM_PORT),PLUGIN_SRC=$(PLUGIN_SRC),N=$(N),IMAGE_BIT_DEPTH=$(IMAGE_BIT_DEPTH)" )
