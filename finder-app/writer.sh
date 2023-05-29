#!/bin/bash
if [[ $# < 2 ]]
then
	echo "expected number of parameters: 2"
	exit 1
fi

writefile=$1
writestr=$2

dir_name=$(dirname $writefile)
base_name=$(basename $writefile)

mkdir -p $dir_name

if ! [ -d "$dir_name" ]
then
        echo "$dir_name can not be created"
        exit 1
fi

cd $dir_name
touch $base_name

if ! [ -e "$base_name" ]
then 
	echo "File $base_name could not be created"
	exit 1
fi

echo $writestr > $base_name


