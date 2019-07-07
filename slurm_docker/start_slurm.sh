#!/bin/bash

set -xe 

# start authentication daemon
munged -f

# start slurm controller daemon
slurmctld

# start slurmc compute node daemon
slurmd


