#!/bin/bash
cd /code || exit 1;
autoreconf -fvi
CFLAGS="-mavx2 -O3" bash configure
make
make install DESTDIR=/code/buildroot
