#!/bin/sh
# Script to send raw can frames from the output of candump (from BBB)
# First argument is can device, the rest are input files

# Remove can device from argument list
can_dev=$1
shift

# Angstrom has a different version of the can tools
if [ "$(cat /etc/issue | grep Angstrom)" ]; then
	# Running on Angstrom
	bs=' 0x'
	can_send="cansend $can_dev -e -i"
	sid='0x'
	eid=' '
else
	# Running on something else
	bs='.'
	can_send="cansend $can_dev"
	sid=''
	eid='#'
fi

# Construct cansend commands from the contents of the remaining arguments
tail -q -n +2 $@ | \
	sed "s/ \+$//;
		s/ \[.\] //;
		s/ /$bs/g
		s/^<0x\([0-9 a-f]\+\)>/$can_send $sid\1$eid/;" | \
	sh - > /dev/null

