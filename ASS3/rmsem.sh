#!/bin/bash


for y in `ipcs -s | grep $USER| cut -f2 -d" "`; do ipcrm -s $y; done

