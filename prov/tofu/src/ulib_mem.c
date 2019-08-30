/*
 *
 *	2019.05.24 Created
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <rbtree.h>
#include "utofu.h"

extern void *mremap(void *old_address, size_t old_size,
                    size_t new_size, int flags, ... /* void *new_address */);
#include <ofi_list.h>

/*
 * Memory Registration
 */
struct meminfo {
    struct dlist_entry	dent; /* MUST BE TOP */
    void	  *addr;
    size_t	  size;
    int		  refc;
    utofu_stadd_t stadd;
};

static RbtHandle memhndl;	/* key is memory address */
static RbtHandle stdhndl;	/* key is stadd */

static int
comp(void *a, void *b)
{
    int	rc;
    if (a == b) rc = 0;
    else if (a < b) rc = -1;
    else rc = 1;
    return rc;
}

void
ulib_init_mem()
{
    memhndl = rbtNew(comp);
    stdhndl = rbtNew(comp);
}

int
ulib_dereg_mem(utofu_vcq_hdl_t vcq_hdl, utofu_stadd_t stadd,
	       unsigned long int flags)
{
    int	uc;
    RbtIterator	  ri;
    void	*tmp;
    struct meminfo *mp;

    ri = rbtFind(stdhndl, (void*) stadd);
    if (ri) {
	rbtKeyValue(NULL, ri, &tmp, (void**) &mp);
	if (--mp->refc == 0) {
	    utofu_dereg_mem(vcq_hdl, mp->stadd, 0);
	    /* remove entry */
	    rbtErase(stdhndl, ri);
	    ri = rbtFind(memhndl, mp->addr);
	    rbtErase(memhndl, ri);
	    free(mp);
	}
    } else {
	uc = -1; goto ext;
    }
ext:
    return uc;
}

/*
 * return value: stadd
 */
uint64_t
ulib_reg_mem(utofu_vcq_hdl_t hndl, void *adr, size_t size)
{
    utofu_stadd_t stadd;
    RbtIterator	  ri;
    void	  *tmp;
    struct meminfo *mp;
    struct dlist_entry *item;
    struct dlist_entry *dentp = NULL;

    if (!memhndl) {
	ulib_init_mem();
    }
    ri = rbtFind(memhndl, adr);
    if (ri) {
	rbtKeyValue(NULL, ri, &tmp, (void**) &mp);
	if (mp->size >= size) {
	    stadd = mp->stadd;
	    mp->refc++;
	    goto ext;
	} else {
	    dentp = &mp->dent;
	    dlist_foreach(&mp->dent, item) {
		struct meminfo *tp = (struct meminfo*) item;
		if (tp->size >= size) {
		    stadd = tp->stadd;
		    tp->refc++;
		    goto ext;
		}
	    }
	    dentp = item;
	}
    }
    mp = malloc(sizeof(struct meminfo));
    if (mp == NULL) { stadd = -1; goto ext; }
    utofu_reg_mem(hndl, adr, size, 0, &stadd);
    dlist_init(&mp->dent);
    mp->addr = adr;
    mp->size = size;
    mp->stadd = stadd;
    mp->refc = 0;
    if (dentp) {
	dlist_insert_after(&mp->dent, dentp);
    } else {
	rbtInsert(memhndl, adr, mp);
	rbtInsert(stdhndl, (void*) stadd, mp);
    }

ext:
    return stadd;
}

/*
 * IOVEC Handling
 */
#define TOFU_IOV_LIMIT	8
#define TOFU_IOV_POOL_SIZE	8
#define IOVEC_MAGIC	"ulib_iov"

struct ulib_iovec_entry {
    struct slist_entry	sent;
    char		magic[8];
    struct iovec	iov[TOFU_IOV_LIMIT];
};

struct slist iovlist;

struct iovec *
ulib_iovec_alloc()
{
    struct ulib_iovec_entry *uiep;
    if (slist_empty(&iovlist)) {
	int	i;
	uiep = calloc(sizeof(struct ulib_iovec_entry), TOFU_IOV_POOL_SIZE);
	if (uiep == NULL) {
	    fprintf(stderr, "Cannot allocate memory\n");
	    exit(-1);
	}
	for (i = 0; i < TOFU_IOV_POOL_SIZE; i++) {
	    memcpy(uiep[i].magic, IOVEC_MAGIC, sizeof(IOVEC_MAGIC));
	    printf("uiep[i] = %p magic = %s\n", &uiep[i], uiep[i].magic);
	    slist_insert_tail(&uiep[i].sent, &iovlist);
	}
    }
    uiep = (struct ulib_iovec_entry*) slist_remove_head(&iovlist);
    return uiep->iov;
}

void
ulib_iovec_free(struct iovec *iovp)
{
    struct ulib_iovec_entry *uiep;
    uiep = container_of(iovp, struct ulib_iovec_entry, iov);
    if (strncmp(uiep->magic, IOVEC_MAGIC, sizeof(IOVEC_MAGIC)) != 0) {
	fprintf(stderr, "ulib_iovec_free: memory corrupted ? (%p)\n", iovp);
	printf("uiep(%p) uiep->magic = (%s)\n", uiep, uiep->magic);
	exit(-1);
    }
    slist_insert_head(&uiep->sent, &iovlist);
}

void
ulib_iovec_init()
{
    struct iovec *iovp;
    slist_init(&iovlist);
    iovp = ulib_iovec_alloc();
    ulib_iovec_free(iovp);
}
