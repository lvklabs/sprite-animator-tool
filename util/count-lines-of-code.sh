#!/bin/bash
echo "Counting lines of C++ source code in" `pwd` 
rm -f moc_*.cpp ui_mainwindow.hi qrc_res.cpp
echo "Total lines               :" `cat *.cpp *.h | wc -l`
echo "Total lines without spaces:" `cat *.cpp *.h | grep -v -e "^\s*$" | wc -l`
