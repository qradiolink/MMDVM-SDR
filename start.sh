#!/bin/bash

NUM_CHAN=$1
cd build/
exit_all()
{
  killall mmdvm
}

for i in $(seq 1 $NUM_CHAN)
do
   ./mmdvm -c $i &
done

trap exit_all SIGINT
wait
