#!/bin/sh

filesdir=$1
searchstr=$2

#############################################################################
#ChatGPT: Conversation https://chatgpt.com/share/696f1985-0f14-800e-9cde-966db70ef4dd
#############################################################################
#Refernce: For conditional scripting
#https://ryanstutorials.net/bash-scripting-tutorial/bash-if-statements.php#ifelif
#############################################################################

if [ $# -ne 2 ]
then 
	if [ $# -eq 1 ]
	then	
		echo "Error : Second Parameter is missing"
		echo "Fix: You need to input First Parameter as path to a directory and Second Parameter as text string that will be used to search in files"
	else 
		echo "Error : First and Second Parameter are missing"
		echo "Fix: You need to input First Parameter as path to a directory and Second Parameter as text string that will be used to search in files"
	fi	
	exit 1


#############################################################################
#Reference : For finding if the argument is a directory
#https://stackoverflow.com/questions/4665051/check-if-passed-argument-is-file-or-directory-in-bash
#############################################################################

elif [ -d "${filesdir}" ]
then


############################################################################
#Reference : For getting number of files in directory and subdirectory
#https://www.howtogeek.com/count-files-in-a-directory-on-linux/
############################################################################

	filecount=$(find "$filesdir" -type f | wc -l)

############################################################################
#Reference : For searching string in files using grep 
#https://linuxhandbook.com/grep-search-all-files-directories/
###########################################################################

	searchcount=$(grep -r "$searchstr" "$filesdir" | wc -l)
 	 
	echo "The number of files are $filecount and the number of matching lines are $searchcount" 
	exit 0
else 
	echo "filesdir does not represent a directory on the filesystem"
	echo "Fix: You need to input First Parameter as path to a directory"
	exit 1
fi 
