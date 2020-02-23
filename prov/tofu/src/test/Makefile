#MPICH_ARCH=fugaku
#MPICH_ARCH=x86
include mkdef.$(MPICH_ARCH)
INCLUDE +=-I../../../../include -I../../../../ -I../

PROGS = size_exam size_exam2
SRCS = size_exam.c size_exam2.c procmap.c
OBJS = $(SRCS:.c=.o)

all: $(PROGS) procmap
$(PROGS): $(OBJS)
	$(CC) -g -o $@ $@.o
procmap: procmap.o
#	 $(MPICC) -g -o $@ $@.o -L$(PMIXLIBDIR) -lpmix
	 $(MPICC) -g -o $@ $@.o $(HOME)/riken-mpich/lib/libpmix.so -ltofucom
clean:
	rm -f $(PROGS) $(OBJS)
run-clean:
	rm -f run-numazu.sh.* run-fugaku.sh.* core.*
.c.o:
	$(CC) -c $< $(INCLUDE)

