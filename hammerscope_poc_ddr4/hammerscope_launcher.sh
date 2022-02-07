#!/bin/bash

cd kaslr
`cat build_command` > /dev/null 2>&1
cd ..

cd hammerscope_lib
`cat so_build_command` > /dev/null 2>&1
cd ..

foldername=results/$(date +%Y_%m_%d_%H_%M_%S)

if [[ ! -e $foldername ]]; then
    mkdir -p $foldername
fi

taskset --cpu-list 1 python $1 $2 $foldername