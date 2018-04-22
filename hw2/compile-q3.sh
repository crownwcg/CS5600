#!/bin/sh
#
# $Id: compile-q3.sh 522 2012-01-31 04:06:32Z pjd $
#

if [ x"$1" = xclean ] ; then
    rm -f homework-q3
    exit
fi

gcc -I pth-2.0.7/install/include -g -Wall -o homework-q3 misc.c homework.c -DQ3 -L pth-2.0.7/install/lib -lpth -lm
