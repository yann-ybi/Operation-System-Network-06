#!/bin/bash

# initialize variables
numProcesses=0
endedProcesses=0

# array to store PIPEs
declare -a PIPEs=()

# array to store PIDs of traveler processes
declare -a PIDs=()

# function to launch a new traveler process
trav() {
  width=$1
  height=$2
  numThreads=$3

  # increment numProcesses
  numProcesses=$((numProcesses+1))

  # pip setup
  PIPE="/tmp/travpipe${numProcesses}"
  PIPEs+=($PIPE)
  rm -f $PIPE
  mkfifo $PIPE
  
  # launch new process and get PID
  ./Version2/travel $width $height $numThreads $PIPE &

  # get the process ID of the last launched process
  PID=$!


}

# function to send a command to a process
send_cmd() {
  processIndex=$1
  c=$2
  
  # send command to process
  echo $c > "${PIPEs[$((processIndex-1))]}"
}

# loop to read commands
while true; do
  read -r CMD
  read -r CmD WIDTH HEIGHT THREADS <<< "$CMD"
  read -r processIndex cmd <<< "$CMD"

	if [[ $CmD == 'trav' ]]
  then
    trav $WIDTH $HEIGHT $THREADS
    
  elif [[ $cmd == 'r' ]] 
  then
    send_cmd $processIndex $cmd

  elif [[ $cmd == 'g' ]]
  then
    send_cmd $processIndex $cmd

  elif [[ $cmd == 'b' ]] 
  then
    send_cmd $processIndex $cmd

  elif [[ $cmd == 'end' ]] 
  then
    endedProcesses=$((endedProcesses+1))
    send_cmd $processIndex $cmd
  else
    echo "Invalid command: $CMD"
  fi

  if [[ $numProcesses == $endedProcesses ]]
  then
    break
  fi
done
