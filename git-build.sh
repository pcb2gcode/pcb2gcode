#!/bin/sh

libtoolize
automake --add-missing
autoreconf
./configure
make -j
