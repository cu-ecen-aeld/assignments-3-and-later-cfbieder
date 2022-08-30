#!/bin/bash


if [ $# -ne 2 ]
  then
    echo 'Error: Invalid number of arguments'
    echo 'Total number of arguments should be 2'
    echo 'The order of the arguments should be'
    echo '    1) File Name'
    echo '    2) Test to be added to file'
    exit 1
fi
echo $2 > $1
