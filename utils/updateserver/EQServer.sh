#!/bin/bash
#bashisms woo

#### Dynamic zones have been commented out, add if you must.
#### Loginserver runs last to avoid players that login too fast from getting loaded in an instance or denied access.

ulimit -c unlimited
case "$1" in
start)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
 
rm -rf logs/*.log
chmod --recursive ugo+rwx quests
 
#sleep 5
echo -en "\r\t $(tput bold)$(tput setaf 2)Loading Shared Mem...$(tput sgr0)";
./shared_memory > /dev/null 2>&1 &
 
sleep 2
echo -en "\r\t $(tput bold)$(tput setaf 2)Starting World Server...$(tput sgr0)";
./world > /dev/null 2>&1 &
echo $! > world.pid
 
sleep 3
echo -en "\r\t $(tput bold)$(tput setaf 2)Starting Query Server...$(tput sgr0)";
./queryserv > /dev/null 2>&1 &
echo $! > queryserv.pid

sleep 5
echo -en "\r\t $(tput bold)$(tput setaf 2)Starting Chat Server...$(tput sgr0)";
./ucs > /dev/null 2>&1 &
echo $! > ucs.pid

sleep 5
echo -en "\r\t $(tput bold)$(tput setaf 2)Starting the Zone Launchers...$(tput sgr0)";
echo -e "\r";

boats()
{
./eqlaunch boats > /dev/null 2>&1 &
echo $! > eqlauncha.pid
}
zone1()
{
./eqlaunch zone1 > /dev/null 2>&1 &
echo $! > eqlaunchb.pid
}
zone2()
{
./eqlaunch zone2 > /dev/null 2>&1 &
echo $! > eqlaunchc.pid
}
zone3()
{
./eqlaunch zone3 > /dev/null 2>&1 &
echo $! > eqlaunchd.pid
}
#dynzone1()
#{
#./eqlaunch dynzone1 > /dev/null 2>&1 &
#echo $! > eqlaunche.pid
#}
#dynzone2()
#{
#./eqlaunch dynzone2 > /dev/null 2>&1 &
#echo $! > eqlaunchf.pid
#}
boats &
zone1 &
zone2 &
zone3 #&
#dynzone1 &
#dynzone2

let timet=120;
for i in $(seq 1 120);do
	echo -en "\r\t [$(tput bold)$(tput setaf 2)$timet$(tput sgr0)] seconds remain until all zones are up.";
	let timet=timet-1;
	sleep 1;
done;

#echo -e "\r";
echo -en "\r\t\033[K $(tput bold)$(tput setaf 2)Starting Login Server...$(tput sgr0)";
echo -e "\r";
./loginserver > /dev/null 2>&1 &
echo $! > loginserver.pid
;;
stop)
kill $(cat world.pid)
kill $(cat queryserv.pid)
kill $(cat ucs.pid)
kill $(cat eqlauncha.pid)
kill $(cat eqlaunchb.pid)
kill $(cat eqlaunchc.pid)
kill $(cat eqlaunchd.pid)
kill $(cat eqlaunche.pid)
kill $(cat eqlaunchf.pid)
kill $(cat loginserver.pid)
killall world
killall queryserv
killall ucs
killall eqlauncha
killall eqlaunchb
killall eqlaunchc
killall eqlaunchd
killall eqlaunche
killall eqlaunchf
killall eqlaunch
killall zone
killall loginserver
rm -f *.pid
echo All server components have been exited.
;;
restart|reload)
$0 stop
$0 start
;;
status)
if [ -f loginserver.pid ] && ps -p $(cat loginserver.pid) > /dev/null; then
echo -e Login Server '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Login Server '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f world.pid ] && ps -p $(cat world.pid) > /dev/null; then
echo -e World Server '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e World Server '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f queryserv.pid ] && ps -p $(cat queryserv.pid) > /dev/null; then
echo -e Query Server '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Query Server '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f ucs.pid ] && ps -p $(cat ucs.pid) > /dev/null; then
echo -e Chat Server '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Chat Server '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f eqlauncha.pid ] && ps -p $(cat eqlauncha.pid) > /dev/null; then
echo -e Boat Launcher '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Boat Launcher '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f eqlaunchb.pid ] && ps -p $(cat eqlaunchb.pid) > /dev/null; then
echo -e Zone1 Launcher '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Zone1 Launcher '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f eqlaunchc.pid ] && ps -p $(cat eqlaunchc.pid) > /dev/null; then
echo -e Zone2 Launcher '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Zone2 Launcher '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
if [ -f eqlaunchd.pid ] && ps -p $(cat eqlaunchd.pid) > /dev/null; then
echo -e Zone3 Launcher '\t\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
else
echo -e Zone3 Launcher '\t\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
fi
#if [ -f eqlaunche.pid ] && ps -p $(cat eqlaunche.pid) > /dev/null; then
#echo -e Dynamic1 Zone Launcher '\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
#else
#echo -e Dynamic1 Zone Launcher '\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
#fi
#if [ -f eqlaunchf.pid ] && ps -p $(cat eqlaunchf.pid) > /dev/null; then
#echo -e Dynamic2 Zone Launcher '\t\t' [$(tput bold)$(tput setaf 2)UP$(tput sgr0)]
#else
#echo -e Dynamic2 Zone Launcher '\t\t' [$(tput bold)$(tput setaf 1)DOWN$(tput sgr0)]
#fi
if (($(pgrep -c zone) == 82)); then
echo -e '\t' $(tput bold)$(tput setaf 2)\($(pgrep -c zone)\)$(tput sgr0) $(tput bold)of 82 zones launched
else
echo -e '\t' $(tput bold)$(tput setaf 1)\($(pgrep -c zone)\)$(tput sgr0) $(tput bold)of 82 zones launched
fi
;;
help|*)
printf "Usage: \n $0 [start|stop|reload|restart|status|help]"
printf "\n\n"
printf " start\t\tStarts the server components\n"
printf " stop\t\tStops all the server components started by this script\n"
printf " restart/reload\tRestarts the server\n"
printf " status\t\tLists the status of the server components\n"
printf " help\t\tDisplays this message\n"
;;
 
esac
exit 0
