#!/bin/sh

echo Configuring...
cmake -B build -S .

echo Building...
cmake --build build
