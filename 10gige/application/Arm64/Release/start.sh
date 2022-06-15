#!/bin/sh

# Configure the media nodes
sudo media-ctl -d /dev/media0 -V "\"imx547 7-001a\":0 [fmt:SRGGB10_1X10/1920x1080 field:none @1/60]"
sudo media-ctl -d /dev/media0 -V "\"b0040000.v_demosaic\":1 [fmt:RBG888_1X24/1920x1080 field:none]"
sudo media-ctl -d /dev/media0 -V "\"b0000000.v_proc_ss\":1 [fmt:RBG888_1X24/1920x1080 field:none]"

# Wait for previous command to complete
echo $?

# Print the media graph to check device topology & its formats
echo "Check the media graph for 1920x1080p & formats"
echo "\n"
sudo media-ctl -d /dev/media0 -p

# Tune the sensor parameters for better image quality
echo "Run v4l2 commands to tune the sensor parameters for better image quality"
echo "\n"
sudo v4l2-ctl -d /dev/v4l-subdev0 -c exposure=10000
sudo v4l2-ctl -d /dev/v4l-subdev0 -c black_level=150
sudo v4l2-ctl -d /dev/v4l-subdev0 -c gain=250

# Wait for previous command to complete
echo $?

# Configure SLVS-EC IP Core
sudo /bin/busybox devmem 0xa0020000 32 0xa
sudo /bin/busybox devmem 0xa002000c 32 0x1

# Wait for previous command to complete
echo $?
