#!/bin/sh

LARKEXEC=$([ -f /lib/ld-linux-armhf.so.3 ] && echo "larkplayer" || echo "larkplayer_pw2")
LIBDIR=$([ -f /lib/ld-linux-armhf.so.3 ] && echo "libs_hf/" || echo "libs_pw2/")
#LARKEXEC="larkplayer"

echo "Starting LARK - The Libre Audiobook Player for Kindle..."
lipc-set-prop -s com.lab126.btfd BTenable 0:1
sleep 1
cd /mnt/us/LARK
LD_LIBRARY_PATH=$LIBDIR ./$LARKEXEC

pkill -x $LARKEXEC
