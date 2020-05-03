/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _TOFU_DEBUG_H
#define _TOFU_DEBUG_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <utofu.h>

extern char	*tofu_fi_flags_string(uint64_t flags);

extern int mypid, myrank, nprocs;
extern int rdbgf, rdbgl;

#define RDEBUG 1
#ifdef RDEBUG
/*
 * rdbgf is set in the fi_tofu_setdopt function, exported symbol for Tofu
 * The user calls this function or setenv
 */

/* setenv("TOFU_DEBUG_FD", value, 1); */
#define RDBG_STDOUT     0x1     /* output to stdout */
#define RDBG_STDERR     0x2     /* output to stderr */
/* setenv("TOFU_DEBUG_VVLL", value, 1); */
#define RDBG_LEVEL1     0x1     /* general use */
#define RDBG_LEVEL2     0x2     /* selective */
#define RDBG_LEVEL3     0x2     /* general use */
#define RDBG_LEVEL4     0x4     /* state machine */
#define RDBG_LEVEL8     0x8     /* for finding DLST bug */

#define R_DBG(format, ...)                                              \
   do {                                                                 \
       fprintf(stderr, "[%d]:%d:%s:%d " format " in %s\n",              \
               myrank, mypid, __func__, __LINE__, __VA_ARGS__, __FILE__);\
       fflush(stderr);                                                  \
   } while (0)

#define R_DBGMSG(format)                                                \
   do {									\
       fprintf(stderr, "[%d]:%d:%s:%d " format " in %s\n",              \
               myrank, mypid, __func__, __LINE__, __FILE__);            \
       fflush(stderr);                                                  \
   } while (0)

#define R_IFDBG(level) if (rdbgf & 0x01 && level & rdbgl)

#define R_DBG0(level, format, ...)                                      \
   do {									\
	   if (rdbgf & 0x01 && level & rdbgl) {                         \
		   printf("\t[%d]:%d: " format " in %s:%d\n",		\
			  myrank, mypid, __VA_ARGS__, __func__, __LINE__); \
		   fflush(stdout);					\
           }                                                            \
	   if (rdbgf & 0x02 && level & rdbgl) {                         \
		   fprintf(stderr, "[%d]:%d: " format " in %s:%d\n",	\
			   myrank, mypid, __VA_ARGS__, __func__, __LINE__); \
		   fflush(stderr);					\
	   }								\
   } while (0)

#define R_DBG1(level, format, ...)                                      \
    do {								\
	    char buf1[128];						\
            if (rdbgf & 0x01 && level & rdbgl) {                        \
		    printf("\t[%d]:%d: " format " in %s:%d\n",		\
			   myrank, mypid, __VA_ARGS__, __func__, __LINE__); \
		    fflush(stdout);					\
            }                                                           \
            if (rdbgf & 0x02 && level & rdbgl) {                        \
	    fprintf(stderr, "[%d]:%d: " format " in %s:%d\n",		\
                    myrank, mypid, __VA_ARGS__, __func__, __LINE__);	\
		    fflush(stderr);					\
	    }								\
    } while (0)

#define R_DBG2(level, format, ...)                                      \
   do {									\
	   char buf1[128], buf2[128];					\
           if (rdbgf & 0x01 && level & rdbgl) {                         \
		   printf("\t[%d]:%d:" format " in %s:%d\n",		\
			  myrank, mypid, __VA_ARGS__, __func__, __LINE__); \
		   fflush(stdout);					\
           }                                                            \
           if (rdbgf & 0x02 && level & rdbgl) {                         \
		   fprintf(stderr, "[%d]:%d:" format " in %s:%d\n",	\
                           myrank, mypid, __VA_ARGS__, __func__, __LINE__); \
		   fflush(stderr);					\
	   }								\
   } while (0)

#else
#define R_DBG0(format, ...)
#define R_DBG1(format, ...)
#define R_DBG2(format, ...)
static inline void get_doption() { return; }
#endif

static inline void desc_dump(void *desc, size_t sz)
{
    int		i;
    unsigned long	*ui = desc;
    printf("[%d] desc(%2ld) ", mypid, sz);
    for (i = 0; i < sz; i += 8) {
	printf(":%016lx", *ui++);
    }
    printf("\n");
}

extern void rdbg_mpich_cntrl(const char *func, int lno, void *ptr);
extern void rdbg_iovec(const char *func, int lno, size_t, const void *iovec);
extern char *tank2string(char *buf, size_t sz, uint64_t ui64);
extern char *vcqid2string(char *buf, size_t sz, utofu_vcq_id_t vcqi);
extern void dbg_show_utof_vcqh(utofu_vcq_hdl_t vcqh);

/* Copy from mpich/src/mpid/ch4/netmod/ofi/ofi_pre.h */
enum {
    MPIDI_AMTYPE_SHORT_HDR = 0,
    MPIDI_AMTYPE_SHORT,
    MPIDI_AMTYPE_LMT_REQ,
    MPIDI_AMTYPE_LMT_ACK
};

#define MPIDI_OFI_MAX_AM_HDR_SIZE    128
#define MPIDI_OFI_AM_HANDLER_ID_BITS   8
#define MPIDI_OFI_AM_TYPE_BITS         8
#define MPIDI_OFI_AM_HDR_SZ_BITS       8
#define MPIDI_OFI_AM_DATA_SZ_BITS     48
#define MPIDI_OFI_AM_CONTEXT_ID_BITS  16
#define MPIDI_OFI_AM_RANK_BITS        32
#define MPIDI_OFI_AM_MSG_HEADER_SIZE (sizeof(MPIDI_OFI_am_header_t))
#include <rdma/fabric.h>

struct MPIDI_OFI_am_header {
    uint64_t handler_id:MPIDI_OFI_AM_HANDLER_ID_BITS;
    uint64_t am_type:MPIDI_OFI_AM_TYPE_BITS;
    uint64_t am_hdr_sz:MPIDI_OFI_AM_HDR_SZ_BITS;
    uint64_t data_sz:MPIDI_OFI_AM_DATA_SZ_BITS;
    uint16_t seqno;/* Sequence number of this message.
		    * Number is unique to (fi_src_addr, fi_dest_addr) pair. */
    fi_addr_t fi_src_addr;      /* OFI address of the sender */
    uint64_t payload[0];
} *head;
#endif	/* _TOFU_DEBUG_H */
