#!/bin/bash

export FI_PSM2_NAME_SERVER=1
export I_MPI_HYDRA_TOPOLIB=ipl
export I_MPI_FABRICS=ofa

PROCID=$SLURM_PROCID
IP_ADR=$(./get_addresses.sh)
echo "hostname=$(hostname) PROC_ID=$PROCID, IP_ADR=$IP_ADR"
PROCID=$SLURM_PROCID

stdbuf -i0 -o0 -e0 ./test-psm2 $IP_ADR $PROCID >> "$PROCID.out" 2>&1

wait
