#!/bin/bash

# Read the contents of the configuration file into a variable
configPath=$1
config=$(cat $configPath)

# Remove comment lines and extract node hostnames
nodes=$(echo "$config" | grep -E '^[0-9]+$' | head -n 1)
hostnames=$(echo "$config" | grep -E '^[0-9]+\s+\w+\.\w+\.\w+\s+[0-9]+$' | cut -d ' ' -f 2)
hostname_array=($hostnames)
netID="jxp220032"

# Get the PIDs of the processes started by the launch.sh script
pids=$(pgrep -f "\.totalorder Launcher/$configPath")

# Loop through the PIDs and terminate the corresponding processes
for pid in $pids
do
  echo "Terminating process $pid ..."
  kill $pid
done

for remotehost in "${hostname_array[@]}"
do
  # Skip the host machine
  if [[ "$remotehost" == "$host" ]]; then
    continue
  fi
  echo "Connecting to $remotehost ..."
  ssh $netID@$remotehost "pids=\$(pgrep -f \"\.totalorder $configPath\"); for pid in \$pids; do echo \"Terminating process \$pid ...\"; kill \$pid; done"
done
