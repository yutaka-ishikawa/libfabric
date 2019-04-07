/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_ULIB_TICK_H
#define _ULIB_TICK_H

#include <stdint.h>

extern double	ulib_tick_helz(uint64_t *p_helz);
extern double	ulib_tick_conv(uint64_t p_time_i8);

/*
 * rdtsc (read time stamp counter)
 */
static inline uint64_t ulib_tick_time( void )
{
#if	defined(__GNUC__) && (defined(__sparcv9__) || defined(__sparc_v9__))
    uint64_t rval;
    asm volatile("rd %%tick,%0" : "=r"(rval));
    return rval;
#elif	defined(__x86_64__)
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo) | ((uint64_t)hi)<<32 ;
#elif	defined(__GNUC__) && defined(__aarch64__)
    uint64_t tsc;
    asm volatile("mrs %0, cntvct_el0" : "=r" (tsc));
    return tsc;
#else
#error	"unsupported platform for ulib_tick_time()"
    return 0;
#endif
}

#endif	/* _ULIB_TICK_H */
