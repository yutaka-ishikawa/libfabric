#ifndef _SNDMGT_DEF
#define _SNDMGT_DEF

#include <stdint.h>
#include <string.h>

#define SNDMGT_MAX	(160*1000)

/* must be 256 byte align */
typedef struct sndmgt { /* 4 byte */
    uint8_t	examed:1,
		sndok:1,
		mode:6; /* 1B */
    uint8_t	index;	/* 1B: idx: index of receiver's buf array */
    utofu_path_id_t pathid;	/* 1 B */
    uint8_t	count;		/* 1 B */
} sndmgt;

static inline sndmgt*
sndmgt_alloc(int size)
{
    sndmgt	*smp;
    int		rc;
    long algn = sysconf(_SC_PAGESIZE);
    rc = posix_memalign((void*) &smp, algn, sizeof(struct sndmgt)*size);
    SYSERRCHECK_EXIT(rc, !=, 0, "Cannot allocate aligned memory");
    return smp;
}

static inline void sndmgt_clr_examed(int pos, sndmgt *bp)
{
    bp[pos].examed = 0;
}

static inline void sndmgt_clr_sndok(int pos, sndmgt *bp)
{
    bp[pos].sndok = 0;
}

static inline int sndmgt_isset_examed(int pos, sndmgt *bp)
{
    return bp[pos].examed == 1;
}

static inline int sndmgt_isset_sndok(int pos, sndmgt *bp)
{
    return bp[pos].sndok == 1;
}

static inline void sndmgt_set_examed(int pos, sndmgt *bp)
{
    bp[pos].examed = 1;
}

static inline void sndmgt_set_sndok(int pos, sndmgt *bp)
{
    bp[pos].sndok = 1;
}

static inline void sndmgt_set_index(int pos, sndmgt *bp, unsigned index)
{
    bp[pos].index = index;
}

static inline unsigned sndmgt_get_index(int pos, sndmgt *bp)
{
    return bp[pos].index;
}

static inline void sndmgt_set_pathid(int pos, sndmgt *bp, uint8_t pathid)
{
    bp[pos].pathid = pathid;
}

static inline uint8_t sndmgt_get_pathid(int pos, sndmgt *bp)
{
    return bp[pos].pathid;
}

static inline int
sndmgt_get_smode(int pos, sndmgt *bp)
{
    return bp[pos].mode;
}

static inline int
sndmg_update_chainmode(int pos, sndmgt *bp)
{
    bp[pos].count++;
    if (bp[pos].count >= MSGMODE_THR) {
	bp[pos].mode = MSGMODE_CHND;
    }
    return bp[pos].mode;
}

#ifdef SNDMGT_TEST
#include <stdio.h>

sndmgt	*smgtp;

int
main()
{
    smgtp = (sndmgt
    sndmgt_zero(tbits);
    printf("Is set 32 = %d\n", sndmgt_isset(32, tbits));
    sndmgt_set(32, tbits);
    printf("Is set 32 = %d\n", sndmgt_isset(32, tbits));
    printf("Is set 64 = %d\n", sndmgt_isset(64, tbits));
    sndmgt_set(64, tbits);
    printf("Is set 64 = %d\n", sndmgt_isset(64, tbits));
    printf("[0] = %0lx [1] = %0lx\n", tbits[0], tbits[1]);

    sndmgt_clr(32, tbits);
    sndmgt_clr(64, tbits);
    printf("[0] = %0lx [1] = %0lx\n", tbits[0], tbits[1]);
    return 0;
}
#endif /* SNDMGT_TEST */

#endif /* ~_SNDMGT_DEF */
