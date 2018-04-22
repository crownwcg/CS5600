#!/bin/sh
#
# $Id: compile-q2.sh 522 2012-01-31 04:06:32Z pjd $
#
if [ x"$1" = xclean ] ; then
    rm -f homework-q2
    exit
fi

gcc -g -O1 -Wall -o homework-q2 homework.c misc.c -DQ2 -lpthread -lm
