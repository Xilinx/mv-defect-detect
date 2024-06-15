# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc.

for file in ./*; do
    f=$(echo "${file##*/}");
    filename=$(echo $f| cut  -d'.' -f 1); #file has extension, it return only filename
    echo $filename
    gst-launch-1.0 filesrc location=$file ! jpegdec  ! videoconvert  ! videoscale ! video/x-raw, width=1920, height=1080, format=GRAY8 ! filesink location=$filename.y8
done
cat Mango_*.y8 > input_video.y8