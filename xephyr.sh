#!/bin/sh

W=1920
H=1080
FAKE=":9"

CMD="Xephyr -br -ac +iglx -resizeable -screen "$W"x"$H" $FAKE"

echo "$CMD"
echo Xephyr shell, exit to close Xephyr
$CMD &
PID=$!
XDG_SEAT="" DISPLAY=$FAKE PS1="Xephyr\$ " sh
kill -9 $PID
echo
