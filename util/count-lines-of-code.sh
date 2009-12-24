#!/bin/bash
rm -f moc_*.cpp
echo "Total lines               :" `cat *.cpp *.h | wc -l`
echo "Total lines without spaces:" `cat *.cpp *.h | grep -v -e "^\s*$" | wc -l`
