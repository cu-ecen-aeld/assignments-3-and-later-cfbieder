#!/bin/sh


if [ $# -ne 2 ]
  then
    echo 'Error: Invalid number of arguments'
    echo 'Total number of arguments should be 2'
    echo 'The order of the arguments should be'
    echo '    1) File Directory Path'
    echo '    2) String to be searched'
    exit 1
fi

if [ ! -d $1 ]
  then
    echo 'Error: Directory does not exist'
    exit 1
fi
echo "The number of files are $(find $1 -type f | wc -l) and the number of matching lines are $(grep -r $2 $1 | wc -l)"
