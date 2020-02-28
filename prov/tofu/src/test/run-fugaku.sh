#!/bin/bash
#------ pjsub option --------#
#PJM -S
#	#PJM -L "node=144"
#	#PJM --mpi "proc=576"
#	#PJM -L "node=12"
#	#PJM --mpi "proc=48"
#	#PJM -L "node=64"
#	#PJM --mpi "proc=64"
#	#PJM -L "node=192"
#	#PJM --mpi "proc=192"
#PJM -L "node=4608"
#PJM --mpi "proc=4608"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dv27k"
#PJM -L proc-core=unlimited
#------- Program execution -------#

module load lang
mpiexec -np 4608 ./procmap
#./size_exam
#./size_exam2
