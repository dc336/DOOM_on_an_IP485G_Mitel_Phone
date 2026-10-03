#
# This script is called by callmgr when you select Quit from its menus
#

# Kill callctrl because it is not useful with callmgr and neds
# to be restarted when restarting callmgr.
killall callctrl
