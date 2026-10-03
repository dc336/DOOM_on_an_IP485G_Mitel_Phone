start 0
halac
setpower full
cs voice 0 txrx
bind spkrphone 1 1 1
setgain handset_spkr -9
setgain handset_mic 12
setgain handset_spkdig 0
setgain handset_micdig 0
mconall ept_spkrphone_mixp halaudio_handset_mixp mono
ad spkrphone 0

ecsetregi 2 338 32

csxafac

csxafaddpoint ept 2 sbaec_capture_api capture audio_iod_api
csxafaddpoint ept 2 sbaec_capture_log capture audio_iod_cap_log
csxafaddpoint ept 2 sbaec_capture_sout capture audio_iod_sout
csxafaddpoint ept 2 sbaec_capture_sin capture audio_iod_sin
csxafaddpoint ept 2 sbaec_capture_rin capture audio_iod_rin
csxafaddpoint ept 2 sbaec_capture_rout capture audio_iod_rout
csxafaddpoint ept 2 sbaec_capture_evt capture audio_iod_cap_evt

csxafaddpoint ept 2 sbaec_internal_api inject audio_iod_api
csxafaddpoint ept 2 sbaec_inject_rin inject audio_iod_rin
csxafaddpoint ept 2 sbaec_inject_sin inject audio_iod_sin
csxafaddpoint ept 2 sbaec_inject_sout inject audio_iod_sout
csxafaddpoint ept 2 sbaec_inject_rout inject audio_iod_rout

csxafsyncenable ept

