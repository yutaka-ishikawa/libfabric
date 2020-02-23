#!/bin/bash
#------ pjsub option --------#
#PJM -S
#PJM -L "node=2:noncont"
#PJM --mpi "proc=4"
#PJM -L "elapse=00:00:10"
#PJM -L "rscunit=rscunit_ft02,rscgrp=mckernel-dev02b,jobenv=linux"
#PJM -L proc-core=unlimited
#------- Program execution -------#

module load lang
mpiexec -np 4 ./procmap
#./size_exam
#./size_exam2
