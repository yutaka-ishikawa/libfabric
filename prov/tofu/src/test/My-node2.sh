#!/bin/bash
#
#
# sudo echo 1 > /proc/sys/kernel/core_uses_pid

rm -f *.bc *.-allgather-*
echo -n > barrier.lock-0
echo -n > barrier.lock-1
sync

export LD_LIBRARY_PATH=/mnt2/riken-mpich/lib
export UTOFU_WORLD_ID=100
export UTOFU_TCP_SERVER_NAME=192.168.222.100
export UTOFU_TCP_SERVER_PORT=60000
export UTOFU_TCP_INTERFACE=ens33
export UTOFU_TCP_PORT=60100
export JTOFU_MAPINFO_NODE=1x2x1
#export JTOFU_MAPINFO_NODE=1x2x2

##tsim_server &
##server_pid=$!
##echo server_pid=$server_pid
MYJOBID=$$

run_on_tsim()
{
  echo MYJOBID=$MYJOBID
  echo $1
  /mnt2/riken-mpich/bin/tsim_server \
      mpiexec.hydra -np 2  -f ./myhosts \
        -genv LD_LIBRARY_PATH       ${LD_LIBRARY_PATH} \
	-genv MPIR_CVAR_OFI_USE_PROVIDER tofu \
	-genv MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS 1 \
	-genv MYJOBID ${MYJOBID} \
	-genv UTOFU_WORLD_ID        ${UTOFU_WORLD_ID} \
	-genv UTOFU_TCP_SERVER_NAME ${UTOFU_TCP_SERVER_NAME} \
	-genv UTOFU_TCP_SERVER_PORT ${UTOFU_TCP_SERVER_PORT} \
	-genv UTOFU_TCP_INTERFACE   ${UTOFU_TCP_INTERFACE} \
	-genv UTOFU_TCP_PORT        ${UTOFU_TCP_PORT} \
	-genv JTOFU_MAPINFO_NODE    ${JTOFU_MAPINFO_NODE} \
	-genv UTF_TRANSMODE	0 \
	$1
  #    rm -f *.bc
  rm -f *.bc *.-allgather-*
  echo -n > barrier.lock-0
  echo -n > barrier.lock-1
  sync
    MYJOBID=$(($MYJOBID + 1))
}

#run_on_tsim "./procmap"
export TOFU_NAMED_AV=0
export LIBFJPMIX_PATH="/mnt2/riken-mpich/lib/libpmix.so"
run_on_tsim "./testprocmap"
