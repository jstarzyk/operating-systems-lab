#!/bin/bash

img="$1"
fil="$2"
out="$3"
echo "filter $fil:" >> $out
echo `cmake-build-debug/main 1 $img $fil $fil.pgm` >> $out
echo `cmake-build-debug/main 2 $img $fil $fil.pgm` >> $out
echo `cmake-build-debug/main 4 $img $fil $fil.pgm` >> $out
echo `cmake-build-debug/main 8 $img $fil $fil.pgm` >> $out
echo "" >> $out
