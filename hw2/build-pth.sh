#!/bin/sh

if [ ! -d pth-2.0.7 ]; then
    tar xf pth-2.0.7.tar.gz
fi

cd pth-2.0.7
./configure --prefix=$(pwd)/install
make -j 4
make install
echo 'Pth is built!'
