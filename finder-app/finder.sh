#!/bin/bash
if [[ $# < 2 ]]
then
	echo "expected number of parameters: 2"
	exit 1
fi

filesdir=$1
searchstr=$2

if ! [ -d "$filesdir" ]
then 
	echo "$filesdir does not represent a directory on a file system"
	exit 1
fi


number_of_files=$(grep -R -c $searchstr $filesdir | wc -l)
number_of_lines=$(grep -R $searchstr $filesdir | wc -l)

echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"
