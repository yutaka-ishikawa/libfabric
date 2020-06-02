#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utofu.h>
#include <jtofu.h>

struct tofu_coord {
    uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
};

#define TOFA_BITS_X  8
#define TOFA_BITS_Y  8
#define TOFA_BITS_Z  8
#define TOFA_BITS_A  1
#define TOFA_BITS_B  2
#define TOFA_BITS_C  2
#define TOFA_BITS_CQ  8   /* <  6 */
#define TOFA_BITS_CID  3
#define TOFA_BITS_TNI  3

union tofu_addr {
    struct tofu_struct { /* compact format : network address + memory address */
	uint64_t    cq : TOFA_BITS_CQ; /* 0 + 8  */
	uint64_t    rsv0: 16;		/* 8 + 16 */
	uint64_t    cid : TOFA_BITS_CID; /* 24 +8 */
	uint64_t    rsv1: 5;
	uint64_t    x : TOFA_BITS_X; /* 32 +8 */
	uint64_t    y : TOFA_BITS_Y; /* 40 +8 */
	uint64_t    z : TOFA_BITS_Z; /* 48 +8 */
	uint64_t    a : TOFA_BITS_A; /* 56 +1 */
	uint64_t    b : TOFA_BITS_B; /* 57 +2 */
	uint64_t    c : TOFA_BITS_C; /* 59 +2 */
	uint64_t    tni : TOFA_BITS_TNI; /* 61 +3 */
    } tofu_a;
    uint64_t	tofu_u64;
};

int
main()
{
    //struct tofu_coord	tc;
    uint8_t coords[8];
    int	rc;
    utofu_tni_id_t	tni;
    utofu_cq_id_t	cq;
    uint16_t	cid = 0x6;
    utofu_vcq_id_t  vcqid = 0;
    union tofu_addr	addr;
    int x;

    memset(coords, 0, 8);
    for (x = 0; x < 5; x++) {
	int ni, qi;
	coords[0] = x; coords[1] = 2; coords[2] = 1; coords[3] = 1; coords[4] = 0;
	coords[5] = 1;
	for (ni = 0; ni < 5; ni++) {
	    tni = ni;
	    for (qi = 0; qi < 5; qi++) {
		cq = qi;
		rc = utofu_construct_vcq_id(coords, tni, cq, cid, &vcqid);
		if (rc != 0) { printf("rc = %d\n", rc); }
		printf("xyzabc(%02d:%02d:%02d:%02d:%02d:%02d) "
		       "tni(%d) cq(%d) cid(%d) vcqid = 0x%lx\n",
		       coords[0], coords[1], coords[2], coords[3], coords[4], coords[5],
		       tni, cq, cid, vcqid);
		addr.tofu_u64 = vcqid;
		printf("xyzabc(%02d:%02d:%02d:%02d:%02d:%02d), "
		       "tni(%d), cq(%d), cid(0x%x) rsv0(0x%x) rsv1(0x%x)\n",
		       addr.tofu_a.x, addr.tofu_a.y, addr.tofu_a.z,
		       addr.tofu_a.a, addr.tofu_a.b, addr.tofu_a.c,
		       addr.tofu_a.tni, addr.tofu_a.cq, addr.tofu_a.cid,
		       addr.tofu_a.rsv0, addr.tofu_a.rsv1);
	    }
	}
    }
    return 0;
}
