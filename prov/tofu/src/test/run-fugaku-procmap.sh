#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-PROCMAP" # jobname
#PJM -S
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"

#PJM -L "node=48"
#PJM -L "elapse=00:01:00"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsin-r1"
#PJM -L proc-core=unlimited
#------- Program execution -------#

#module load lang
mpiexec ./procmap
#./size_exam
#./size_exam2
