#include <pmix.h>
#include <pmix_fjext.h>
#include <process_map_info.h>
#include <stdio.h>

#define MOD(a, m)	((a) % (m))
struct tofu_coord {
    uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
};

char	buf1[128], buf2[128];
char *
str_tofu6d(uint32_t coord, char *buf)
{
    struct tofu_coord tc = *(struct tofu_coord*)(&coord);
    snprintf(buf, 128, "x(%02d) y(%02d) z(%02d) a(%02d) b(%02d) c(%02d)",
	     tc.x, tc.y, tc.z, tc.a, tc.b, tc.c);
    return buf;
}

void
tofuaddr(tlib_tofu6d start, tlib_tofu6d size)
{
    int	sx, sy, sz, sa, sb, sc;
    int	lx, ly, lz, la, lb, lc;
    int	cx, cy, cz, ca, cb, cc;
    tlib_tofu6d	addr;
    int rank = 0;
    
    printf("start = %s size = %s\n", str_tofu6d(start, buf1),
	   str_tofu6d(size, buf2));
    sx = TLIB_GET_X(start); sy = TLIB_GET_Y(start); sz = TLIB_GET_Z(start);
    sa = TLIB_GET_A(start); sb = TLIB_GET_B(start); sc = TLIB_GET_C(start);
    printf("sb=%d\n", sb);
    for (lc = 0; lc < TLIB_GET_C(size); lc++) {
	for (lb = 0; lb < TLIB_GET_B(size); lb++) {
	    for (la = 0; la < TLIB_GET_A(size); la++) {
		for (lz = 0; lz < TLIB_GET_Z(size); lz++) {
		    for (ly = 0; ly < TLIB_GET_Y(size); ly++) {
			for (lx = 0; lx < TLIB_GET_X(size); lx++) {
			    cx = MOD(sx + lx, 24);
			    cy = MOD(sy + ly, 24);
			    cz = MOD(sz + lz, 24);
			    ca = sa + la; cb = sb + lb; cc = sc + lc;
			    addr = 0;
			    addr =  TLIB_SET_X(addr, cx) | TLIB_SET_Y(addr, cy)
				| TLIB_SET_Z(addr, cz) |  TLIB_SET_A(addr, ca)
				| TLIB_SET_B(addr, cb) | TLIB_SET_C(addr, cc);
			    printf("[%d]: %s 0x%x\n", rank,
				   str_tofu6d(addr, buf1), addr);
			    rank++;
			}
		    }
		}
	    }
	}
    }
}
