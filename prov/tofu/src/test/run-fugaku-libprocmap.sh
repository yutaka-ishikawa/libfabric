#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-TESTPROCMAP" # jobname
#PJM -S
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"

#	PJM -L "node=48"
#	PJM -L "node=6"
#PJM -L "node=2"
#PJM -L "elapse=00:0:10"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvsin-r2"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsin-r1"
#	PJM --mpi "max-proc-per-node=1"
#	PJM --mpi "max-proc-per-node=4"
#	PJM --mpi "max-proc-per-node=6"
#	PJM --mpi "max-proc-per-node=8"
#	PJM --mpi "max-proc-per-node=12"
#	PJM --mpi "max-proc-per-node=16"
#	PJM --mpi "max-proc-per-node=24"
#PJM --mpi "max-proc-per-node=48"
#PJM -L proc-core=unlimited
#------- Program execution -------#

export LD_LIBRARY_PATH=${HOME}/riken-mpich/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1

#export FI_LOG_PROV=tofu
#export FI_LOG_LEVEL=Debug
export TOFULOG_DIR=./results

export TOFU_NAMED_AV=0
#echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
#mpiexec  ./testprocmap

#echo "*********************************************************************************"
#export PMIX_DEBUG=1
export TOFU_NAMED_AV=1
echo "TOFU_NAMED_AV = " $TOFU_NAMED_AV
mpiexec  ./testprocmap
