#!/bin/sh

LARPEXEC=$([ -f /lib/ld-linux-armhf.so.3 ] && echo "larplayer" || echo "larplayer_pw2")
LIBDIR=$([ -f /lib/ld-linux-armhf.so.3 ] && echo "libs_hf/" || echo "libs_pw2/")
#LARPEXEC="larplayer"

echo "Starting LARP - The Libre Audiobook Player for Pkindle..."
lipc-set-prop -s com.lab126.btfd BTenable 0:1
sleep 1
cd /mnt/us/LARP
LD_LIBRARY_PATH=$LIBDIR ./$LARPEXEC

pkill -x $LARPEXEC
