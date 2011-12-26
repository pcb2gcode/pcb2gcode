#!/bin/sh

libtoolize && aclocal && automake --add-missing && autoreconf && ./configure && make -j
