#!/bin/bash
#SBATCH --exclusive
##SBATCH -C flat,quad
#SBATCH --time=00:30:00
#SBATCH --error=job.%J.err
#SBATCH --output=job.%J.out
#SBATCH -p standard96:test

export FI_PSM2_NAME_SERVER=1
export I_MPI_HYDRA_TOPOLIB=ipl
export I_MPI_FABRICS=ofa
export LIBFABRIC=/home/bemsalem/libraries/libfabric-1.8.0/install/

echo "NODE_LIST=$SLURM_JOB_NODELIST"
# Get the addresses on the nodes
ADR=""
ADR=$(srun -N $SLURM_JOB_NUM_NODES get_addresses.sh | sort -k1 -n | awk '{ print $1}')
echo "ADD1=$ADR"
ADR=($(sort <<<"${ADR[*]}"))
echo "ADD2=$ADR"
ADR=$( IFS=$' '; echo "${ADR[*]}" )
echo "ADD3=$ADR"

echo "# of nodes = $SLURM_JOB_NUM_NODES, COMPUTE=$COMPUTE, INPUT=$INPUT, ADR='$ADR'"
rm "nodes"
for ip in $ADR
do
    echo "$ip"
    echo "$ip" >> "nodes"
done

LDFLAGS="-L$LIBFABRIC/lib -lfabric"
CXXFLAGS="-I$LIBFABRIC/include -std=c++11"

export LD_LIBRARY_PATH=$LIBFABRIC/lib:$LD_LIBRARY_PATH

echo "g++ $CXXFLAGS $LDFLAGS test-psm2.cpp -o test-psm2"
g++ $CXXFLAGS $LDFLAGS test-psm2.cpp -o test-psm2

INPUT=$INPUT COMPUTE=$COMPUTE  mpirun -np $SLURM_JOB_NUM_NODES -ppn 1 ./srun.sh
wait



