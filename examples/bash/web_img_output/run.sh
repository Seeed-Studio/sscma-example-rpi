gst-launch-1.0  v4l2src device=/dev/video0 ! videoconvert ! videoscale ! \
    video/x-raw,width=1280,height=720,format=RGB,pixel-aspect-ratio=1/1,framerate=15/1 ! \
    sscma_yolov5 model=../net/epoch_300_float.ncnn.bin,../net/epoch_300_float.ncnn.param labels=../net/coco.txt ! \
    jpegenc ! multifilesink location=output.jpg