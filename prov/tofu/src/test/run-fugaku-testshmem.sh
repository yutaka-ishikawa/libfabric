#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-SHMEM" # jobname
#PJM -S
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"

#PJM -L "node=2"
#PJM --mpi "max-proc-per-node=4"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsin-r1"
#PJM -L proc-core=unlimited
#------- Program execution -------#

export LD_LIBRARY_PATH=${HOME}/riken-mpich/lib:$LD_LIBRARY_PATH
export MPIR_CVAR_OFI_USE_PROVIDER=tofu
export MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1

export TOFU_NAMED_AV=1
export TOFULOG_DIR=./results
mpiexec ./test_shmem
#./size_exam
#./size_exam2
