#!/bin/sh

export LD_LIBRARY_PATH=.
export LD_PRELOAD=./libmemcheck.so
./sample
