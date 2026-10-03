start 0
halac
net 0 51000 192.168.1.75 51000
setpower full
cs voice 0 inactive
bind handset 1 0 0
ad handset 0
ss 0 dial on -1 -15 -15 egress 0
mcon ept_handset_mixp halaudio_handset_mixp mono
setgain handset_spkr -15
setgain handset_spkdig 0
