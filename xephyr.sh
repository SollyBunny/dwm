#!/bin/sh

FAKE=":5"
unset XDG_SEAT
# CMD="Xephyr -br -ac +iglx -screen 1920x1080 $FAKE"
# CMD="Xephyr -br -ac +iglx -screen 1920x1200 $FAKE"
CMD="Xephyr -br -ac +iglx -screen 960x600 $FAKE"
echo "$CMD"
$CMD
