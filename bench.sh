#!/bin/zsh

for i in {1..50}; do
  /usr/bin/time -p ./arc samples/c/clang/sema.h --none --quit 2>&1 | grep '^real' | awk '{print $2}'
done | awk '{sum += $1; n++} END {print "Mean: " sum/n}'
