#!/bin/sh
# Supervisor for the AMOLED login. One getty at a time; restarted when a
# session ends, because without that the console is dead after the first
# logout. BusyBox init's own respawn does not work for tty1 here, so it is
# done with a loop.
while :; do
	/sbin/getty -L tty1 0 vt100 < /dev/tty1 > /dev/tty1 2>&1
	sleep 1
done
