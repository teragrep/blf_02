#!/bin/bash
cd /code || exit 1;
autoreconf -fvi
bash configure
make
make install DESTDIR=/code/buildroot
