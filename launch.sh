#!/bin/bash
mpirun -np 2 -npernode 1 -hostfile bin/nodefile bin/oncillamem
