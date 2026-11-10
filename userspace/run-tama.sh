#!/bin/sh
#
# run-tama.sh - start the Tamagotchi emulator and keep it running.
#
# Launched by hand for now; a future S51tamafb will do the same thing at boot.
#
# Why a retry loop: exec is subject to the same contiguity rule as any
# allocation (KRN-001). The binary is 224 KB static and, once the heap has
# fragmented, starting it can fail with SIGSEGV even though nothing is wrong
# with it -- measured at about one launch in three with the largest free block
# at 256 KB. Retrying costs a second and is honest about the limit; pretending
# the first attempt always works is what made this look like a code fault.
#
# Usage: run-tama.sh [seconds] [fps]
#
# fps is the DISPLAY refresh rate, not the emulation speed. Pacing in tamafb
# keeps the guest at exactly 1.00x real time whatever fps is: each frame runs
# CLOCK_HZ/fps emulated cycles and then sleeps to the deadline. So raising fps
# costs nothing in correctness and only shortens the wait before a screen
# change is noticed -- which is the whole point of 10 rather than 4. At 4 fps
# a boundary came once every 250 ms; at 10 it comes every 100 ms.

SECS=${1:-900}
FPS=${2:-20}
LOG=/home/tama.log

cd /home || exit 1

i=0
while [ $i -lt 8 ]; do
	i=$((i + 1))
	./tamafb "$SECS" "$FPS" > "$LOG" 2>&1 &
	pid=$!
	sleep 3
	if kill -0 "$pid" 2>/dev/null; then
		echo "tamafb up after $i attempt(s), pid $pid"
		echo "$pid" > /var/run/tamafb.pid
		exit 0
	fi
	echo "attempt $i did not start, retrying"
done

echo "tamafb would not start after $i attempts; output:"
cat "$LOG"
exit 1
