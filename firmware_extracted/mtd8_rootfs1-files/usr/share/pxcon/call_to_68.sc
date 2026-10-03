start 0
halac
net 0 51000 192.168.1.68 51000
setpower full
cs voice 0 txrx
bind handset 1 0 0
setgain handset_spkr -9
setgain handset_mic 12
setgain handset_spkdig 0
setgain handset_micdig 0
mconall ept_handset_mixp halaudio_handset_mixp mono
ad handset 0
