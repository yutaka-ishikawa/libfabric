#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utofu.h>
#include <jtofu.h>

#pragma pack(1)
struct tofu_coord {
    union {
	uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
	uint32_t	dat;
    };
};
#define MAX_COORD	(24*24*23*12)
struct tofu_coord coord[MAX_COORD];

#define TOFU_BITS_X  8
#define TOFU_BITS_Y  8
#define TOFU_BITS_Z  8
#define TOFU_BITS_A  1
#define TOFU_BITS_B  2
#define TOFU_BITS_C  2
#define TOFU_BITS_CQ  8   /* <  6 */
#define TOFU_BITS_CID  3
#define TOFU_BITS_TNI  3

union tofu_addr {
    struct tofu_struct {
	uint64_t    cq : TOFU_BITS_CQ; /* 0 + 8  */
	uint64_t    rsv0: 16;		/* 8 + 16 */
	uint64_t    cid : TOFU_BITS_CID; /* 24 +8 */
	uint64_t    rsv1: 5;
	uint64_t    x : TOFU_BITS_X; /* 32 +8 */
	uint64_t    y : TOFU_BITS_Y; /* 40 +8 */
	uint64_t    z : TOFU_BITS_Z; /* 48 +8 */
	uint64_t    a : TOFU_BITS_A; /* 56 +1 */
	uint64_t    b : TOFU_BITS_B; /* 57 +2 */
	uint64_t    c : TOFU_BITS_C; /* 59 +2 */
	uint64_t    tni : TOFU_BITS_TNI; /* 61 +3 */
    } tofu_a;
    uint64_t	tofu_u64;
};

/*
 * 8+8+4=20, 20*6=120 Byte (cache line size is 128 Byte)
 */
struct tofu_nic {
    union tofu_addr	vcqh;
    size_t		fly_len;
    uint32_t		flgs;
};

#define MAX_NIC		6
struct tofu_nic	tofu_nic[MAX_NIC];

struct tofu_coord
rank2tofu_coord(int rank)
{
    if (rank >= MAX_COORD) {
	struct tofu_coord tc;
	tc.dat = (uint32_t) -1;
	return tc;
    }
    return coord[rank];
}

int
main(int argc, char **argv)
{
    FILE	*fp;
    char	*bp;
    size_t	sz, n;
    uint64_t	vcqid;
    int		rc, rank, flgs;
    int		nprocs;
    union tofu_addr	taddr;
    struct tofu_coord	tc;

    if (argc < 2) {
	fprintf(stderr, "%s: <vcqid file>\n", argv[0]);
	return -1;
    }
    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Cannot open file %s\n", argv[1]);
	return -1;
    }
    bp = 0; n = 0; nprocs = 0;
    printf("start\n");
    while ((sz = getline(&bp, &n, fp)) != (ssize_t) -1) {
	//printf("line=(%s)\n", bp);
	rc = sscanf(bp, "\t: [%d] vcqid(%lx) flags(%d)", &rank, &vcqid, &flgs);
	if (rc != 3) continue;
	taddr.tofu_u64 = vcqid;
	printf("[%d] vcqid(%lx) = %02d:%02d:%02d:%02d:%02d:%02d cq(0x%x) rsv0(0x%x) rsv1(0x%x) tni(0x%x)\n",
	       rank, vcqid, taddr.tofu_a.x, taddr.tofu_a.y, taddr.tofu_a.z,
	       taddr.tofu_a.a, taddr.tofu_a.b, taddr.tofu_a.c,
	       taddr.tofu_a.cq, taddr.tofu_a.rsv0, taddr.tofu_a.rsv1, (unsigned) taddr.tofu_a.tni);
	coord[rank].x = taddr.tofu_a.x;	coord[rank].y = taddr.tofu_a.y;
	coord[rank].z = taddr.tofu_a.z;	coord[rank].a = taddr.tofu_a.a;
	coord[rank].b = taddr.tofu_a.b;	coord[rank].c = taddr.tofu_a.c;
	nprocs++;
	{
	    uint8_t abcxyz[8];
	    uint16_t tni[1], tcq[1], cid[1];
	    rc = utofu_query_vcq_info(vcqid, abcxyz, tni, tcq, cid);
	    if (rc != 0) {
		printf("Someting wrong\n");
		continue;
	    }
	    printf("\t\t: xyzabc(%d:%d:%d:%d:%d:%d), tni(%d), cqid(%d), cid(0x%x)\n",
		   abcxyz[0], abcxyz[1], abcxyz[2], abcxyz[3], abcxyz[4], abcxyz[5],
		   tni[0], tcq[0], cid[0]); fflush(stdout);
	}
    }
    free(bp);
    fclose(fp);

    tc = rank2tofu_coord(10);
    printf("rank(10): %02d:%02d:%02d:%02d:%02d:%02d\n",
	   tc.x, tc.y, tc.z, tc.a, tc.b, tc.c);
    return 0;
}
