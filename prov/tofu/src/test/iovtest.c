#include "ulib_mem.c"

int utofu_reg_mem(utofu_vcq_hdl_t vcq_hdl,
	      void *addr, size_t size, unsigned long int flags,
		  utofu_stadd_t *stadd) { return 0;}
int utofu_dereg_mem(utofu_vcq_hdl_t vcq_hdl,
		utofu_stadd_t stadd,
		    unsigned long int flags) { return 0; }

int
main()
{
    struct iovec *ivp[TOFU_IOV_POOL_SIZE*2];
    int		i, err = 0;

#define TMAX (TOFU_IOV_POOL_SIZE + 2)
    ulib_iovec_init();
    for (i = 0; i < TMAX; i++) {
	ivp[i] = ulib_iovec_alloc();
	printf("iovec(%p)\n", ivp[i]);
    }
    for (i = 0; i < TMAX; i++) {
	ulib_iovec_free(ivp[i]);
    }
    for (i = 0; i < TMAX; i++) {
	struct iovec *itmp;
	itmp = ulib_iovec_alloc();
	if (ivp[TMAX - i - 1] != itmp) {
	    printf("Wrong: %p != %p\n", ivp[TMAX - i - 1], itmp);
	    err = 1;
	}
    }
    if(err == 0) printf("PASS\n");
    return 0;
}

#if 0
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

extern void *mremap(void *old_address, size_t old_size,
                    size_t new_size, int flags, ... /* void *new_address */);
#include <ofi.h>
#include <ofi_mem.h>

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

#endif /* 0 */
