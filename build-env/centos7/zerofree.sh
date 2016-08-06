#!/bin/bash
set -e -u

dd if=/dev/zero of=/root/empty bs=1M || :
rm /root/empty
dd if=/dev/zero of=/boot/empty bs=1M || :
rm /boot/empty

SWAPDEV=`cat /proc/swaps | tail -1 | awk '{print $1}'`
swapoff $SWAPDEV
dd if=/dev/zero of=$SWAPDEV bs=1M || :
mkswap $SWAPDEV
swapon $SWAPDEV
