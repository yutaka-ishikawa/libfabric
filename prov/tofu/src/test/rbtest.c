#include "ulib_mem.c"

#define INITVAL 0xf00000000
#define INCVAL	2048

int
utofu_reg_mem(utofu_vcq_hdl_t vcq_hdl,
	      void *addr, size_t size, unsigned long int flags,
	      utofu_stadd_t *stadd)
{
    static uint64_t dummy = INITVAL;
    *stadd = dummy;
    dummy += INCVAL;
    return 0;
}

int
utofu_dereg_mem(utofu_vcq_hdl_t vcq_hdl,
		utofu_stadd_t stadd,
		unsigned long int flags)
{
    return 0;
}

uint64_t
test11(void *addr, ssize_t sz)
{
    utofu_stadd_t	stadd;
    stadd = ulib_reg_mem(0, addr, sz);
    printf("addr(%010lx) size(%010lx) stadd(%010lx)\n",
	   (uint64_t) addr, sz, stadd);
    return stadd;
}

void
expect(int i, uint64_t rslt, uint64_t expt)
{
    printf("\t%d: %s\n", i, rslt == expt ? "OK" : "FAIL");
}

void
test1()
{
    void	*addr;
    ssize_t	sz;
    int		i = 0;
    uint64_t	eval = INITVAL;

    expect(i++, test11((void*) 0x0a0000000, 0x000001000), eval);
    expect(i++, test11((void*) 0x0a0000000, 0x000001000), eval);
    eval = INITVAL + INCVAL;
    expect(i++, test11((void*) 0x100000000, 0x000003000), eval);
    expect(i++, test11((void*) 0x100000000, 0x000002000), eval);
    eval = INITVAL + INCVAL*2;
    expect(i++, test11((void*) 0x100000000, 0x000005000), eval);
    expect(i++, test11((void*) 0x100000000, 0x000001000), eval);
    expect(i++, test11((void*) 0x100000000, 0x000004000), eval);
    expect(i++, test11((void*) 0x100000000, 0x000003000), eval);
}

void test0();

int
main()
{
    test1();
    //test0();
    return 0;
}

/*
 * Test code
 */
struct meminfo *
create_meminfo(void *adr, size_t sz)
{
    struct meminfo *mip;
    mip = malloc(sizeof(struct meminfo));
    mip->addr = adr;
    mip->size = sz;
    return mip;
}

void
handler(void *arg, RbtIterator it)
{
    void	  *tmp;
    struct meminfo *mp;
    rbtKeyValue(NULL, it, &tmp, (void**) &mp);
    printf("addr(%p) size(%ld)\n", mp->addr, mp->size);
}


void	*key[100];

void
test0()
{
    RbtHandle	rhndl;
    RbtStatus	rstat;
    RbtIterator	ri;
    size_t	size;
    void	*tmp, *mykey;
    struct meminfo *mp, *mp2;
    int		i;
    
    rhndl = rbtNew(comp);

    mykey = malloc(128);
    printf("mykey=%p\n", mykey);
    mp2 = create_meminfo(mykey, 128);

    size = 0;
    for (i = 0; i < 10; i++) {
	size += 1024;
	key[i] = malloc(size);
	mp = create_meminfo(key[i], size);
	printf("key=%p\n", key[i]);
	rstat = rbtInsert(rhndl, key[i], mp);
	if (rstat != RBT_STATUS_OK) {
	    printf("rstat = %d\n", rstat);
	}
    }
    printf("mykey=%p\n", mykey);
    rstat = rbtInsert(rhndl, mykey, mp2);
    if (rstat != RBT_STATUS_OK) {
	printf("rstat = %d\n", rstat);
    }
	
    printf("key[9]=%p\n", key[9]);
    ri = rbtFind(rhndl, key[9]);
    if (ri) {
	rbtKeyValue(NULL, ri, &tmp, (void**) &mp);
	printf("addr(%p) size(%ld)\n", mp->addr, mp->size);
    }
    printf("traverse\n");
    ri = rbtRoot(rhndl);
    rbtTraversal(rhndl, ri, 0, handler);

    printf("using rbtBegin, rbtNext, rbtEnd\n");
    for (ri = rbtBegin(rhndl); ri != rbtEnd(rhndl); ri = rbtNext(rhndl, ri)) {
	rbtKeyValue(NULL, ri, &tmp, (void**) &mp);
	printf("addr(%p) size(%ld)\n", mp->addr, mp->size);
    }
}

