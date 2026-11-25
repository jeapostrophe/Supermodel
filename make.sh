#!/bin/sh -xe
dylib=supermodel_libretro.dylib
LIB=lib/$dylib
make -f Makefiles/Makefile.OSX LIBRETRO=1 $LIB
RT=../../retro-trainer
CORES=$RT/config/cores
CORE=$CORES/$dylib
rm -f $CORE
ln -s $(pwd)/$LIB $CORE
cd $RT
source make.sh spikeofe
