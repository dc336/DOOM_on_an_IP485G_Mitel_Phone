#!/bin/sh
#
# oprofile script
#
# start and stop oprofile
# wrapper script for opcontrol


echo "* entering $0"

SESSION_DIR=/var/lib/oprofile
ERROR=0

case "$1" in
	init)
      if [ -d /proc/sys/perfcnt ]; then
         echo 1 > /proc/sys/perfcnt/stop
      fi
		mkdir -p ${SESSION_DIR} 
		mount -t tmpfs tmpfs ${SESSION_DIR} 
		mount -t tmpfs tmpfs /root	
		opcontrol --vmlinux=/boot/vmlinux 
	;;
	start)
		opcontrol --start
	;;
	dump)
		opcontrol --dump
	;;
	stop)
		opcontrol --stop
	;;
	shutdown)
		opcontrol --stop				
		opcontrol --shutdown
		umount -t tmpfs /root
		umount -t ${SESSION_DIR} 
		rm -rf ${SESSION_DIR}
	;;
	run)
		if [ $# -lt 2 ]; then
			echo "* Error: run needs to be following by your application" 
			ERROR=1	
		else 
			which $2
			if [ $? -ne 0 ]; then
				echo "* can't find your application" 
				ERROR=1	
			else 
				shift 1
				echo "* executing and profiling $*"
				opcontrol --start; $*; opcontrol --stop; opcontrol --dump 
			fi
		fi
	;;	
	*)
		ERROR=1
	;;
esac

if [ $ERROR == 1 ]; then
	echo
	echo "--- Usage $0 {init|start|dump|stop|run|shutdown}"
	echo
	echo "    init:            initialize for oprofile, daemon not started yet"
	echo "    start:           start the daemon"
	echo "    stop:            stop the daemon"
	echo "    dump:            force daemon to dump samples"
	echo "    run [your app]:  this is a combination of command for profiling userland application, it "
	echo "                       1. start the daemon"
	echo "                       2. run the application"
	echo "                       3. stop the daemon"
	echo "                       4. force the daemon to dump samples"
	echo "    shutdown:        shutdown oprofile and deinit"
	echo 
	echo "--- typical usage and session examples ---"
	echo 
	echo " if you want to profile your userland application"
	echo "   # $0 init"
	echo "   # $0 run (app) "
	echo "   # opreport --symbols \`which app\` [other options]"
	echo 
	echo " if you want to profile the entire system (kernel + all applications)"
	echo "   # $0 init"
	echo "   # $0 start"
	echo "   # $0 stop" 
	echo "   # $0 dump"
	echo "   # opreport --symbols [other options]"
	echo 
	exit -1
fi
echo leaving $0...						 
