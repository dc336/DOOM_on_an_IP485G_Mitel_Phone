#!/bin/sh
#
#  This script will configure/deconfigure the network interfaces of a device 
#  using the 'ifconfig' configuration utility, and the 'udhcpc' DHCP client. 
#
#  The configuration is based upon a MAC address and network interface 
#   configuration file
#
#  Usage:
#     if_setup.sh up|down|reconfig
#        up         - configure network interfaces
#        down       - deconfigure network interfaces
#        reconfig   - reconfigure network interface after phone boots up, should be used after if_setup.sh down

# Uncomment for debug info
#set -x

#
# Verify the correct arguments are specified.
#
if [ $# -ne 1 ] || ! echo $1 | grep -q -E '^(up|down|reconfig)$'; then
   echo "Bad Parameters: $*"
   echo "Usage: if_setup.sh [up|down|reconfig]"
   exit 1
fi
config_type=$1

# Ensure eth0 device is set up [e.g. button box]
if ! ifconfig eth0 2>&1 >/dev/null; then
 mac_address_eth=`cat /proc/manuf/macAddress`
 # Bind MAC
 if [ -z "${mac_address_eth}" ]; then
  echo "********************"
  echo "** Error - No MAC address specified!"
  echo "********************"
  exit 1
 fi
 modprobe bcmring_net
 modprobe bcmring_eth_sla
 ifconfig eth0 hw ether ${mac_address_eth}
fi

# pull [net] section from any pphone.conf
sed -n -e '/\[net\]/,/^$/p' /nvdata/config/pphone.conf >/tmp/ifs$$conf 2>/dev/null

# dhcp 0 for no dhcp, 1 for dhcp
grep dhcpEnable=false /tmp/ifs$$conf
dhcp=$?

resolv_conf="/etc/resolv.conf"

# default resolv.conf if there is no dhcp dns servers
# use OpenDNS servers (this should work with any ISP)
echo -n > $resolv_conf
echo nameserver 208.67.222.222 >> $resolv_conf
echo nameserver 208.67.220.220 >> $resolv_conf

if [ $config_type = up ]; then 
   # Start loopback interface.
   ifconfig lo 127.0.0.1 netmask 255.0.0.0 up
fi   

if [ $dhcp -eq 1 ]; then
   #
   # This network interface uses DHCP.
   # 
   
   if [ $config_type = up -o $config_type = reconfig ]; then 
      ip_addr_saved=`sed -n -e 's;^lease\.ipAddress=\(.*\)$;\1;p' /nvdata/netcache 2>/dev/null`
      if [ "$ip_addr_saved" ]; then
         ip_addr_saved="-r $ip_addr_saved"
      fi

      #
      # Spawn the DHCP client. Try to use the most recently bound IP address 
      # for this device. Also, run the client in "background" mode so that it 
      # doesn't bail if we don't initially obtain an IP address.
      #
      ifconfig eth0 up
      udhcpc -b -i eth0 -s /usr/share/udhcpc/default.script ${ip_addr_saved} -p /var/run/udhcpc.eth0.pid
   else
      # Kill the DHCP client.
      #
      # SIGUSR2 forces udhcpc to release the current lease and go inactive,
      # and SIGTERM causes udhcpc to exit.
      pid=`cat /var/run/udhcpc.eth0.pid 2>/dev/null`
      if [ $pid ]; then
         kill -USR2 $pid
         kill -TERM $pid
         ps -o pid | grep "\<$pid\>" >/var/run/udhcpc.eth0.pid
      fi
      # 
      # Shutdown the network inteface.
      #
      ip addr flush dev eth0
   fi
else
   #
   # This network interface uses a static IP address. 
   #
   if [ $config_type = up -o $config_type = reconfig ]; then 
      ip_address=`sed -n -e 's;^ipAddress=\(.*\)$;\1;p' /tmp/ifs$$conf`
      [ -z ${ip_address} ] && echo "Error - No IP addr for eth" && exit 1

      subnet=`sed -n -e 's;^subnetMask=\(.*\)$;\1;p' /tmp/ifs$$conf`
      [ -z ${subnet} ] && echo "Error - No subnet for eth" && exit 1

      router=`sed -n -e 's;^gatewayAddress=\(.*\)$;\1;p' /tmp/ifs$$conf`
      [ -z ${router} ] && echo "Error - No router for eth" && exit 1

      # 
      # Bind the IP address for the device.
      #
      ifconfig eth0 ${ip_address} netmask ${subnet}

      #
      # Update the routing table.
      #
      if [ -n "${router}" ] ; then
         metric=0
         for i in $router; do
            route add default gw $i dev eth0 metric $((metric++))
         done
      fi

      #
      # Update the resolver configuration file with any DNS servers
      #
      echo -n > $resolv_conf
      for i in $dns; do
         echo adding dns $i
         echo nameserver $i >> $resolv_conf
      done
   else
      # 
      # Shutdown the network inteface.
      #
      ip addr flush dev eth0
   fi         
fi
rm /tmp/ifs$$*
