#!/bin/bash 

writefile=$1
writestr=$2

if [ $# -ne 2 ]
then 
	if [ $# -eq 1 ]
	then 
		echo "Error: Second Parameter is missing"
		echo "Fix: You need to input First Parameter as full path to a file and Second Parameter as text string that will be written in the file"
	else
		echo "Error: First and Second Parameter are not present"
		echo "Fix: You need to input First Parameter as full path to a file and Second Parameter as text string that will be written in the file"
	fi
exit 1
fi

dirpath=$(dirname "$writefile")

#########################################################################
#Reference: Creating directories 
#https://stackoverflow.com/questions/4906579/how-to-use-bash-to-create-a-folder-if-it-doesnt-already-exist
#########################################################################
if [ ! -d "$dirpath" ]
then 
	mkdir -p "$dirpath"
fi

########################################################################
#Reference: Creat a file if it is not existing and overwrite to file
#https://www.geeksforgeeks.org/techtips/write-to-a-file-from-the-shell/
#https://stackoverflow.com/questions/4676459/write-to-file-but-overwrite-it-if-it-exists
########################################################################

echo "$writestr" > "$writefile"

if [ $? -ne 0 ]
then
	echo "Error: Could not create file $writefile"
	exit 1
fi

written=$(cat "$writefile")
if [ "$written" != "$writestr" ]; then
    echo "Error: File content does not match"
    exit 1
fi
exit 0
