#!/bin/bash
echo "Counting lines of C++ source code in" `pwd` 
make clean >/dev/null
echo "Total lines                                 :" `cat *.cpp *.h | wc -l`
echo "Total lines without blank lines             :" `cat *.cpp *.h | grep -v -e "^\s*$" | wc -l`
echo "Total lines without blank lines and comments:" `cat *.cpp *.h | sed -e 's#//.*$##g;s#/\*.*\*/##g' | grep -v -e "^\s*$" | wc -l`
