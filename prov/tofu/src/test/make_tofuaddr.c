#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utofu.h>
#include <jtofu.h>

struct tofu_coord {
    uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
};

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

int
main()
{
    char	*line;
    size_t	lsz;
    uint8_t coords[8];
    utofu_vcq_id_t  vcqid = 0;
    int	rank, nprocs = 0;
    int sx, sy, sz, x, y, z, a, b, c;

    memset(coords, 0, 8);
    line = NULL; lsz = 0;
    getline(&line, &lsz, stdin);
    sscanf(line, "%d %d %d\n", &sx, &sy, &sz);
    getline(&line, &lsz, stdin);
    sscanf(line, "%d\n", &nprocs);
    rank = 0;
    printf("nprocs=%d\n", nprocs);
    while(1) {
	for (x = sx; x < 24; x++) {
	    for (y = sy; y < 24; y++) {
		for (z = sz; z < 24; z++) {
		    for(a = 0; a < 2; a++) {
			for (b = 0; b < 3; b++) {
			    for (c = 0; c < 2; c++) {
				utofu_tni_id_t	tni, ni;
				utofu_cq_id_t	cq, qi;
				uint16_t	cid = 0x7;
				int	rc;
				coords[0] = x; coords[1] = y; coords[2] = z;
				coords[3] = a; coords[4] = b; coords[5] = c;
				for (ni = 0; ni < 1; ni++) {
				    tni = ni;
				    for (qi = 0; qi < 1; qi++) {
					cq = qi;
					rc = utofu_construct_vcq_id(coords, tni, cq, cid, &vcqid);
					if (rc != 0) { printf("rc = %d\n", rc); }
					printf("\t: [%d] vcqid(%lx) flags(0)\n",
					       rank, vcqid);
					rank++;
					if (rank >= nprocs) goto ext;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }
ext:
    return 0;
}
