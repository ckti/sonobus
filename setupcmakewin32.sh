#!/bin/bash

DEPS=""

if [ -n "${AAX_SDK_PATH}" ] ; then
  echo "Will build AAX plugin"
  DEPS="$DEPS -DAAX_SDK_PATH=${AAX_SDK_PATH}"
fi

if [ -n "${VST2_SDK_PATH}" ] ; then
  echo "Will build VST2 plugin"
  DEPS="$DEPS -DVST2_SDK_PATH=${VST2_SDK_PATH}"
fi


cmake -G "Visual Studio 15 2017" -T "host=x64" $DEPS -B build32






