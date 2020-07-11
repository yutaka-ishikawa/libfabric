/*
 * Memory Registration
 */
#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_list.h"

struct meminfo {
    utfdlist	dent;
    void	*addr;
    size_t	size;
    int		refc;
    utofu_stadd_t stadd;
};

static utfdlist		utf_memalloc;
static utfdlist		utf_memiffree;
static struct meminfo	*utf_memifpool;
static int		utf_memifsize;
static int		utf_memccsize; /* cache size */
static int		utf_memccent; /* cached entries */
static int		utf_memtopthr; /* if the entry resued is lower than this threshold, the meminfo entry is moved to the top */

/*
 *	entries = 128, ccsize = 64
 */
void
utf_init_mem(int entries, int ccsize, int thr)
{
    int	i;
    utf_memifpool = malloc(sizeof(struct meminfo)*entries);
    SYSERRCHECK_EXIT(utf_memifpool, ==, NULL, "Not enough memory");
    utfdlist_init(&utf_memalloc);
    utfdlist_init(&utf_memiffree);
    for (i = entries - 1; i >= 0; --i) {
	utfdlist_insert(&utf_memiffree, &utf_memifpool[i].dent);
    }
    utf_memifsize = entries;
    if (ccsize > entries) ccsize = entries;
    utf_memccsize = ccsize;
    utf_memtopthr = thr;
}

static void
utf_remove_memcache(utofu_vcq_hdl_t hndl, int nkeeps)
{
    int	uc;
    utfdlist	   *ent;
    struct meminfo *mp;

    utfdlist_foreach_rev(&utf_memalloc, ent) {
	mp = container_of(ent, struct meminfo, dent);
	if (mp->refc == 0) {
	    uc = utofu_dereg_mem(hndl, mp->stadd, 0);
	    UTOFU_ERRCHECK_EXIT(uc);
	    utfdlist_remove(ent); /* remove entry */
	    utfdlist_insert(&utf_memiffree, ent); /* push it to free list */
	    --utf_memccent;
	    if (utf_memccent < nkeeps) {
		break;
	    }
	}
    }
}

int
utf_dereg_mem(utofu_vcq_hdl_t vcq_hdl, utofu_stadd_t stadd,
	       unsigned long int flags)
{
    int	uc;
    utfdlist	   *ent;
    struct meminfo *mp;

    utfdlist_foreach(&utf_memalloc, ent) {
	mp = container_of(ent, struct meminfo, dent);
	if (mp->stadd == stadd) goto find;
    }
    /* not found */
    utf_printf("%s: something wrong\n", __func__);
    uc = -1;
    goto ext;
find:
    --mp->refc;
ext:
    return uc;
}


/*
 * return value: stadd
 */
uint64_t
utf_reg_mem(utofu_vcq_hdl_t hndl, void *addr, size_t size)
{
    int	rc, nth;
    utfdlist	   *ent;
    struct meminfo *mp;
    utofu_stadd_t stadd;

    nth = 0;
    utfdlist_foreach(&utf_memalloc, ent) {
	mp = container_of(ent, struct meminfo, dent);
	if (mp->addr == addr && mp->size >= size) {
	    stadd = mp->stadd;
	    if (mp->refc == 0) --utf_memccent; /* now its not cache, busy */
	    mp->refc++;
	    if (nth > utf_memtopthr) {
		/* move to top entry */
		utfdlist_remove(ent); /* remove entry */
		utfdlist_insert(&utf_memalloc, ent); /* inert it into top */
	    }
	    goto find;
	}
	nth++;
    }
    /* not found */
    ent = utfdlist_get(&utf_memiffree);
    if (ent == NULL) {
	int	nremove = utf_memccent/2;
	if (nremove == 0) nremove = utf_memccent;
	if (nremove > 0) utf_remove_memcache(hndl, nremove);
	ent = utfdlist_get(&utf_memiffree);
#ifdef UMEM_TEST
	ERRCHECK_RETURN(ent, ==, NULL, 0);
#else
	LIBERRCHECK_EXIT(ent, ==, NULL, "No more room keeping address info.\n");
#endif /* UMEM_TEST */
    }
    mp = container_of(ent, struct meminfo, dent);
    rc = utofu_reg_mem(hndl, addr, size, 0, &stadd);
    UTOFU_ERRCHECK_EXIT(rc);
    mp->addr = addr;
    mp->size = size;
    mp->refc++;
    mp->stadd = stadd;
    utfdlist_insert(&utf_memalloc, ent);
find:
    return stadd;
}

void
utf_dump_meminfo(FILE *fp)
{
    utfdlist	   *ent;
    struct meminfo *mp;

    fprintf(fp, "      " "     Address      " "      Size    "
	    "Ref-C" "     ST Address    " "\n");
    utfdlist_foreach(&utf_memalloc, ent) {
	mp = container_of(ent, struct meminfo, dent);
	fprintf(fp, "[%4d] 0x%016lx, 0x%08lx, %03d, 0x%016lx\n",
		myrank, (uint64_t) mp->addr, mp->size, mp->refc, mp->stadd);
	fflush(fp);
    }
}

int
utf_vrfy_mem(void *addr, int expct, int vfy)
{
    utfdlist	*ent;
    struct meminfo *mp;
    int		nth = 0;
	
    utfdlist_foreach(&utf_memalloc, ent) {
	mp = container_of(ent, struct meminfo, dent);
	if (mp->addr == addr) {
	    if (nth == expct) {
		return 0;
	    } else {
		if (vfy) {
		    printf("[%4d] mem info for %p is registered at #%d, but at #%d\n",
			   myrank, mp->addr, nth, expct);
		}
		return 1;
	    }
	}
	nth++;
    }
    if (vfy) {
	printf("[%4d] cannot find the address %p in mem info\n",
	       myrank, addr);
    }
    return 1;
}
