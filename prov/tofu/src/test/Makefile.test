INCLUDE=-I../../../../include -I../../../../ -I../
INCLUDE += -I/mnt2/work2/riken-mpich/include

PROGS = size_exam size_exam2
SRCS = size_exam.c size_exam2.c
OBJS = $(SRCS:.c=.o)

all: $(PROGS)
$(PROGS): $(OBJS)
	cc -g -o $@ $@.o
clean:
	rm -f $(PROGS) $(OBJS)
.c.o:
	cc -c $< $(INCLUDE)

