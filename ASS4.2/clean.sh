#!/bin/bash
#REMOVE SEMAPHORES
for y in `ipcs -s | grep $USER| cut -f2 -d" "`; do ipcrm -s $y; done
#REMOVE SHM
for y in `ipcs -m | grep $USER| cut -f2 -d" "`; do ipcrm --shmem-id=$y; done

