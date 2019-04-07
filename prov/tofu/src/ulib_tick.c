/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <stdio.h>              /* for fopen() */
#include <stdint.h>             /* for uint64_t */
#include <stdlib.h>		/* for getenv() */

#include "ulib_tick.h"


static uint64_t ulib_tick_helz_dflt(void)
{
    uint64_t helz;
    /*
     * KCOM : 2000 * 1000 * 1000
     */
    helz = 1848 * 1000 * 1000; /* FX10 */
    return helz;
}

/* from an environment variable */
static uint64_t ulib_tick_helz_envv(void)
{
    const char *cp;
    char *p = 0;
    long lv = 0;

    cp = getenv("PROG_HELZ");
    if (cp != 0) {
	lv = strtol(cp, &p, 10);
	if ((p == 0) || (p <= cp) || (p[0] != '\0')) {
	    lv = 0;
	}
    }
    if (lv < 0) {
	lv = 0;
    }
    return (uint64_t)lv;
}
#if	defined(__GNUC__) && defined(__aarch64__)

static inline uint64_t ulib_tick_helz_arm64(void)
{
    static uint32_t helz = 0;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (helz));
    return helz;
}
#endif

static uint64_t ulib_tick_helz_from_slat_proc(void)
{
    FILE *fp;
    uint64_t helz = 0;
#if	defined(__x86_64__)
#define MHZ_F2HZ_L(FV)  (long)((FV) * 1000.0 * 1000.0)
#endif	/* defined(__x86_64__) */

    fp = fopen("/proc/cpuinfo", "r");
    if (fp == 0) {
	helz = 0;
    }
    else {
	long ls = 0;
	const char *fmt1 = "Cpu0ClkTck\t: %lx\n";
#if	defined(__x86_64__)
	const char *fmt2 = "cpu MHz\t\t: %f\n";
	float fv = 0.0;
#endif	/* defined(__x86_64__) */
	char buf[1024];

	while (fgets(buf, sizeof (buf), fp) != 0) {
	    int rc = sscanf(buf, fmt1, &ls);
	    if (rc == 1) { break; }
#if	defined(__x86_64__)
	    rc = sscanf(buf, fmt2, &fv);
	    if (rc == 1) { ls = MHZ_F2HZ_L(fv); break; }
#endif	/* defined(__x86_64__) */
	}
	helz = (ls <= 0)? 0: ls;
	fclose(fp); fp = 0;
    }

    return helz;
}

double ulib_tick_helz(uint64_t *p_helz)
{
    static double d_helz = 0.0;
    static uint64_t l_helz = 0;
    uint64_t helz;

    if (d_helz != 0.0) {
	goto skip;
    }
    helz = ulib_tick_helz_envv(); /* environment variable */
    if (helz == 0) {
#if	defined(__GNUC__) && defined(__aarch64__)
	helz = ulib_tick_helz_arm64();
#else
	helz = ulib_tick_helz_from_slat_proc();
#endif
    }
    if (helz == 0) {
	helz = ulib_tick_helz_dflt(); /* default */
    }
    l_helz = helz;
    d_helz = (double)helz;

skip:
    if (p_helz != 0) {
	p_helz[0] = l_helz;
    }
    return d_helz;
}

double ulib_tick_conv(uint64_t p_time_i8)
{
    static double hz = 0.0;
    uint64_t lv = p_time_i8;
    double dv;

    if (hz == 0.0) {
	hz = ulib_tick_helz( 0 );
	/* printf("hz %.0f\n", hz); */
    }
    dv = (double)lv;
    if (hz > 0.0) {
	dv /= hz;
    }
    return dv;
}
