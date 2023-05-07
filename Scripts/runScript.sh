#!/bin/bash

# parse arguments
WIDTH=$1
HEIGHT=$2
THREADS=$3

# lauch of a cellular automaton process
./Version1/travel $WIDTH $HEIGHT $THREADS &

# get the process ID of the last launched process
PID=$!

# pip setup
PIPE=/tmp/travpipe
rm -f $PIPE
mkfifo $PIPE

send_cmd() {
  echo "$1" > "$PIPE"
}

# communicate with the process
while true; do
  read -r CMD

  case "$CMD" in
    r)
      send_cmd "$CMD"
      ;;
    g)
      send_cmd "$CMD"
      ;;
    b)
      send_cmd "$CMD"
      ;;
    end)
      send_cmd "$CMD"
      # Terminate the script
      break
      ;;
    *)
      echo "Invalid command: $CMD"
      ;;
  esac
done

# Terminate the traveler process
kill $PID

# Clean up named pipe
rm -f $PIPE
