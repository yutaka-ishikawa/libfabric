#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utofu.h>
#include <jtofu.h>

#pragma pack(1)
struct tofu_coord {
    union {
	struct {
	    uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
	};
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

static int		nprocs;

struct tofu_coord
rank2tofu_coord(int rank)
{
    if (rank > nprocs) {
	struct tofu_coord tc;
	tc.dat = -1;
	return tc;
    }
    return coord[rank];
}

char	tnipos[3][5][3];
#define N_TNI 6

void
tni_initwhich()
{
    int i, j, k;
    int which = 0;
    for (i = 0; i < 3; i++) {
	for (j = 0; j < 5; j++) {
	    for (k = 0; k < 3; k++) {
		tnipos[i][j][k] = which++ % N_TNI;
	    }
	}
    }
    printf("max which = %d\n", which);
}
    

int
tni_which(struct tofu_coord src, struct tofu_coord dst)
{
    int	a = dst.a - src.a + 1;
    int	b = dst.b - src.b + 2;
    int c = dst.c - src.c + 1;
    printf("%s: [%d][%d][%d]\n", __func__, a, b, c);
    return tnipos[a][b][c];
}

int
main(int argc, char **argv)
{
    FILE	*fp;
    char	*bp;
    size_t	sz, n;
    uint64_t	vcqid;
    int		rc, rank, flgs;
    union tofu_addr	taddr;

    if (argc < 2) {
	fprintf(stderr, "%s: <vcqid file>\n", argv[0]);
	return -1;
    }
    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Cannot open file %s\n", argv[1]);
	return -1;
    }
    bp = 0; n = 0; nprocs = 0;
    while ((sz = getline(&bp, &n, fp)) != -1) {
	//printf("line=(%s)\n", bp);
	rc = sscanf(bp, "\t: [%d] vcqid(%lx) flags(%d)", &rank, &vcqid, &flgs);
	if (rc != 3) continue;
	taddr.tofu_u64 = vcqid;
	coord[rank].x = taddr.tofu_a.x;	coord[rank].y = taddr.tofu_a.y;
	coord[rank].z = taddr.tofu_a.z;	coord[rank].a = taddr.tofu_a.a;
	coord[rank].b = taddr.tofu_a.b;	coord[rank].c = taddr.tofu_a.c;
	nprocs++;
#if 0
	printf("[%d] vcqid(%lx) = %02d:%02d:%02d:%02d:%02d:%02d cq(0x%x) rsv0(0x%x) rsv1(0x%x) tni(0x%x)\n",
	       rank, vcqid, taddr.tofu_a.x, taddr.tofu_a.y, taddr.tofu_a.z,
	       taddr.tofu_a.a, taddr.tofu_a.b, taddr.tofu_a.c,
	       taddr.tofu_a.cq, taddr.tofu_a.rsv0, taddr.tofu_a.rsv1, (unsigned) taddr.tofu_a.tni);
#endif
    }
    free(bp);
    fclose(fp);
    tni_initwhich();
    printf("nprocs = %d\n", nprocs);
    {
	char	*line = NULL;
	size_t	lsz = 0, rc;
	int	src, dst;
	int	tni;
	struct tofu_coord	src_tc, dst_tc;
	for(;;) {
	    printf("src >"); fflush(stdout);
	    rc = getline(&line, &lsz, stdin);
	    if (rc == -1) break;
	    sscanf(line, "%d\n", &src);
	    printf("dst >"); fflush(stdout);
	    rc = getline(&line, &lsz, stdin);
	    if (rc == -1) break;
	    sscanf(line, "%d\n", &dst);
	    src_tc = rank2tofu_coord(src);
	    dst_tc = rank2tofu_coord(dst);
	    printf("src(%d): %02d:%02d:%02d:%02d:%02d:%02d ==> "
		   "dst(%d): %02d:%02d:%02d:%02d:%02d:%02d\n",
		   src, src_tc.x, src_tc.y, src_tc.z, src_tc.a, src_tc.b, src_tc.c,
		   dst, dst_tc.x, dst_tc.y, dst_tc.z, dst_tc.a, dst_tc.b, dst_tc.c);
	    printf("dst - src: %02d:%02d:%02d:%02d:%02d:%02d\n",
		   dst_tc.x - src_tc.x, dst_tc.y - src_tc.y, dst_tc.z - src_tc.z,
		   dst_tc.a - src_tc.a, dst_tc.b - src_tc.b, dst_tc.c - src_tc.c);
	    tni = tni_which(src_tc, dst_tc);
	    printf("#tni = %d\n", tni);
	}
	free(line);
    }
    printf("\n");
    return 0;
}
