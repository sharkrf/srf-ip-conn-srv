#!/bin/sh
self=`readlink "$0"`
if [ -z "$self" ]; then
	self=$0
fi
scriptname=`basename "$self"`
scriptdir=${self%$scriptname}

cd $scriptdir
scriptdir=`pwd`

rm -rf Debug Release CMakeFiles
rm -rf Makefile cmake_install.cmake CMakeCache.txt
