#!/bin/bash
gcc -c $1.s
objdump -d $1.o > $1.d
