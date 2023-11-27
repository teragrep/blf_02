#!/bin/bash
cd /code || exit 1;
autoreconf -fvi
CFLAGS=-march=haswell bash configure
make
make install DESTDIR=/code/buildroot
