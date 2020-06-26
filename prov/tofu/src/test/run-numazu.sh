#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-TESTPROCMAP" # jobname
#PJM -S
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"

#	PJM -L "node=1"
#PJM -L "node=4"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=dvsys-spack2,jobenv=linux"
#PJM -L proc-core=unlimited
#PJM --mpi "max-proc-per-node=4"
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
#mpiexec  -n 1 ./testprocmap
mpiexec ./testprocmap
