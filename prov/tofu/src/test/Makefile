#MPICH_ARCH=fugaku
#MPICH_ARCH=x86
include mkdef.$(MPICH_ARCH)
INCLUDE +=-I../../../../include -I../../../../ -I../
MPICH_SRC= ../../../../../mpich/src

PROGS = size_exam size_exam2
SRCS = size_exam.c size_exam2.c procmap.c tofuaddr.c test_tofuaddr.c tofucapability.c
OBJS = $(SRCS:.c=.o)

all: $(PROGS) procmap test_tofuaddr tofucapability commgroup
$(PROGS): $(OBJS)
	$(CC) -g -o $@ $@.o
procmap: procmap.o
#	 $(MPICC) -g -o $@ $@.o -L$(PMIXLIBDIR) -lpmix
	 $(MPICC) -g -o $@ $@.o $(HOME)/riken-mpich/lib/libpmix.so -ltofucom -ltofutop
test_tofuaddr: test_tofuaddr.o tofuaddr.o
	$(CC) -g -o $@ $^
tofucapability: tofucapability.o
	$(MPICC) -g -o $@ $@.o -ltofucom
commgroup: commgroup.c
	$(MPICC) -g -I../ -DCOMMGROUP_TEST -DDEBUG -o commgroup commgroup.c
mpir_test: mpir_test.c
	$(RIKEN_MPICC) -g -c $@.c \
	-I$(MPICH_SRC)/mpid/ch4/include -I$(MPICH_SRC)/include \
	-I$(MPICH_SRC)/mpl/include -I$(MPICH_SRC)/openpa/src	\
	-I$(MPICH_SRC)/mpi/coll/include -I$(MPICH_SRC)/mpi/coll/transports/stubtran \
	-I$(MPICH_SRC)/mpi/coll/transports/gentran/ -I$(MPICH_SRC)/mpi/coll/algorithms/stubalgo/  \
	-I$(MPICH_SRC)/mpi/coll/algorithms/treealgo/ -I$(MPICH_SRC)/mpi/coll/algorithms/treealgo/ \
	-I$(MPICH_SRC)/mpi/coll/allreduce_group/  -I$(MPICH_SRC)/mpid/common/thread/  \
	-I$(MPICH_SRC)/mpid/common/sched/ -I$(MPICH_SRC)/mpid/include/  \
	-I$(MPICH_SRC)/mpid/common/timers/  -I$(MPICH_SRC)/mpid/ch4/src/ \
	-I$(MPICH_SRC)/mpid/ch4/netmod/include -I$(MPICH_SRC)/mpid/ch4/generic
	$(RIKEN_MPICC) -g -o $@ $@.o
clean:
	rm -f $(PROGS) $(OBJS)
run-clean:
	rm -f run-numazu.sh.* run-fugaku.sh.* core.*
.c.o:
	$(CC) -c $< $(INCLUDE)

