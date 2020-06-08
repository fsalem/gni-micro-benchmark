#!/bin/bash

#ifconfig ib0 | grep 'inet ' | cut -d: -f2 | sort -k1 -n | awk '{ print $2}'
/sbin/ip addr show ib0 | grep 'inet ' | awk '{print $2}' | cut -f1 -d'/'
