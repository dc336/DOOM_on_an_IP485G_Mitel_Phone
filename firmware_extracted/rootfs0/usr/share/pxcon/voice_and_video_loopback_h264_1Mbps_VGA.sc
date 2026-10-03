start 0
halac
net 0 4000 127.0.0.1 4000
net 1 4002 127.0.0.1 4002
setpower full
cs voice 0 txrx
bind handset 1 0 0
setgain handset_spkr -9
setgain handset_mic 12
setgain handset_spkdig 0
setgain handset_micdig 0
mconall ept_handset_mixp halaudio_handset_mixp mono
ad handset 0
cs video 1 inactive
codec 3 h264
res 3 vga
framerate 3 30fps
bitrate 3 1m
iperiod 3 0.5s
display 3 encode 0 0 0 0 0 0 320 240 3 lcd none
display 3 decode 0 0 0 0 0 0 800 480 2 lcd none
ad 3 3
cnxmode 3 txrx

