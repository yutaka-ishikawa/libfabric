#!/bin/bash
#------ pjsub option --------#
#PJM -N "MPICH-COMGROUP" # jobname
#PJM -S		# output statistics
#PJM --spath "results/%n.%j.stat"
#PJM -o "results/%n.%j.out"
#PJM -e "results/%n.%j.err"
#
#PJM -L "node=12"
#PJM --mpi "max-proc-per-node=1"
#PJM -L "elapse=00:00:5"
#PJM -L "rscunit=rscunit_ft01,rscgrp=dvsin"
#	PJM -L "rscunit=rscunit_ft01,rscgrp=dvall"
#PJM -L proc-core=unlimited
#------- Program execution -------#

mpiexec ./commgroup
