#!/bin/sh
cd zloader.ls2k/

make cfg all tgt=rom CROSS_COMPILE=/opt/gcc-4.4-gnu/bin/mipsel-linux- DEBUG=-g
make dtb CROSS_COMPILE=/opt/gcc-4.4-gnu/bin/mipsel-linux-
