#!/bin/sh

echo Cleaning directory...
make distclean

echo Generating all neccesary files...
./autogen.sh

echo Making binaries...
make

