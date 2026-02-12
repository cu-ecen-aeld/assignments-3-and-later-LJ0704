#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data
# Requirement b.i: config files at /etc/finder-app/conf
CONFIG_DIR=/etc/finder-app/conf

# Use the absolute path for configuration
username=$(cat "${CONFIG_DIR}/username.txt")

if [ $# -lt 3 ]
then
    if [ $# -ge 1 ]; then
        NUMFILES=$1
    fi
    # (Simplified the nested logic for clarity)
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

rm -rf "${WRITEDIR}"
mkdir -p "$WRITEDIR"

# Use absolute path for assignment check
assignment=$(cat "${CONFIG_DIR}/assignment.txt")

# Remove compilation steps as the Buildroot Makefile handles this
echo "Running writer as a native application from PATH"

for i in $( seq 1 $NUMFILES)
do
    # Requirement b: run with files found in PATH (removed ./)
    writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

# Requirement c: write output to /tmp/assignment4-result.txt
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

# Remove temporary directories after writing the result
rm -rf /tmp/aeld-data

set +e
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING}"
    exit 1
fi
