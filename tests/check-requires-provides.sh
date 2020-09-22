#!/bin/bash

# don't run the test on the gles build
dummy=`grep "Package: qtmir-desktop" debian/control`
if [ $? -ne 0 ]; then
    exit 0
fi

requires=`grep lomiri-shell-application= CMakeLists.txt | sed -E 's/.*=(.*)\)/\1/'`
provides=`grep lomiri-application-impl- debian/control | sed -E 's/.*lomiri-application-impl-(.*),/\1/'`
if [ "$requires" -eq "$provides" ]; then
    exit 0
else
    echo "Error: We require lomiri-shell-application=$requires but provide lomiri-application-impl-$provides"
    exit 1
fi
