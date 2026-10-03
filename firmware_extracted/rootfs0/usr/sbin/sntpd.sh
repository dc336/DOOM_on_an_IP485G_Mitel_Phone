set -x
sleep 2
hwclock -s
sleep 15
while true; do
sntp -r -P no pool.ntp.org
y=$?
if [ $y = 0 ]; then
   which hwclock
   if [ $? = 0 ]; then
      hwclock -w
   fi
 sleep 86400
else
 hwclock -s
 sleep 60
fi
done
