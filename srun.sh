#!/bin/bash

export FI_PSM2_NAME_SERVER=1
export I_MPI_HYDRA_TOPOLIB=ipl
export I_MPI_FABRICS=ofa

echo "hostname=$(hostname) PROC_ID=$SLURM_PROCID"

PROCID=$SLURM_PROCID

stdbuf -i0 -o0 -e0 ./test-psm2 >> "$PROCID.out" 2>&1

wait
