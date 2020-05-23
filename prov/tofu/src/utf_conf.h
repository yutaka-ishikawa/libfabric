#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef UTF_NATIVE
#include <sys/uio.h>
#endif

#define DEBUG(mask) if (utf_dflag&(mask))
#define DLEVEL_PMIX		0x1
#define DLEVEL_UTOFU		0x2
#define DLEVEL_PROTOCOL		0x4
#define DLEVEL_PROTO_EAGER	0x8
#define DLEVEL_PROTO_RENDEZOUS	0x10	/* 16 */
#define DLEVEL_PROTO_RMA	0x20	/* 32 */
#define DLEVEL_PROTO_VBG	0x40	/* 64 */
#define DLEVEL_ADHOC		0x80	/* 128 */
#define DLEVEL_CHAIN		0x100
#define DLEVEL_ALL		0xffff

/* 
 * MTU 1920 Byte, Piggyback size 32 Byte, Assume payload(1888 Byte)
 * 6.8 GB/s, 0.147 nsec/B
 * Rendezvous
 *	Time(B) = 2 + 0.147*B nsec, 
 *	e.g., T(1MiB) = 154.143 usec, 1MiB/154.143usec = 6.487 GiB/s
 *	    actual: 6.35 GiB/s/MB
 * Eager
 *	T(1888 B) = 1 + 0.147*1920 = 184.2 nsec, 1888B/184.2nsec = 10.2 MB/s
 */
/*
 *   165,888 node = 24 x 24 x 24 x 12
 *   663,552 process = 648 Ki, 20 bit
 * 7,962,624 process = 7.6 Mi, 23 bit
 */
/*
 *  Cache Line Size (https://www.7-cpu.com/)
 *	Skylake&KNL: 64 Byte
 *	A64FX: 512 Byte
 */
#define TOFU_PUTGET_SIZE	16777215
#define MSG_SIZE	1920
#define RMA_MAX_SIZE	(8*1024*1024)	/* 8 MiB (8388608 B) */
//#define MSG_PEERS	12	/* must be smaller than 2^7 (edata) */
#define MSG_PEERS	120	/* must be smaller than 2^7 (edata) */
#define EGRCHAIN_RECVPOS (MSG_PEERS - 1) /* used for chain mode */
#define TOFU_ALIGN	256
/* Stearing tag */
#define TAG_SNDCTR	12
#define TAG_ERBUF	13
#define TAG_EGRMGT	14
/**/
#define MAX_NODE	158976	/* Fugaku max nodes */
#define SND_EGR_BUFENT	128	/* max peer's */
#define SND_CNTR_SIZE	128
#define REQ_SIZE	512
#define RMACQ_SIZE	128
#define RMA_MDAT_ENTSIZE 16	/* not used, will be removed 2020/05/06 */
#define PND_LISTSIZE	128	/* pending command list */

/*
 * num_cmp_ids:		  8
 * cache_line_size:	  256 B
 * stag_address_alignment:256 B
 * max_toq_desc_size:	  64 B
 * max_putget_size:	  15 MiB (16777215 B)
 * max_piggyback_size:	  32 B
 * max_edata_size:	   1 B
 * max_mtu:		1920 B
 * max_gap:		 255 B
 */
