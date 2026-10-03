export QWS_KEYBOARD="brcm1107"
export QWS_DISPLAY="VNC:directfb:flip=onsync,wait"
export LD_LIBRARY_PATH=/usr/local/QtEmbedded-4.6.2-arm/plugins/gfxdrivers
software=`cat /proc/manuf/software`
software=${software:-p8cg}
export PHONE_APP=${software}_phone
export PS1='\W\$ '
export PATH=/sbin:/usr/sbin:/bin:/usr/bin
/bin/cli
