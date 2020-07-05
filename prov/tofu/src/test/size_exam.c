#define _GNU_SOURCE
#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include <strings.h>
#include "tofu_impl.h"
#include "tofu_macro.h"
//#include "../tofu_debug.c"

int mypid, myrank;

union jtofu_phys_coords jt_xyzabc;
uint8_t		xyzabc[6];

int
main()
{
    printf("sizeof(struct tofu_fabric) = %ld\n", sizeof(struct tofu_fabric));
    printf("sizeof(struct tofu_domain) = %ld\n", sizeof(struct tofu_domain));
    printf("sizeof(struct tofu_ctx) = %ld\n", sizeof(struct tofu_ctx));
    printf("sizeof(struct tofu_sep) = %ld\n", sizeof(struct tofu_sep));
    printf("sizeof(struct tofu_cq) = %ld\n", sizeof(struct tofu_cq));
    printf("sizeof(struct tofu_cntr) = %ld\n", sizeof(struct tofu_cntr));
    printf("sizeof(struct tofu_cntr) = %ld\n", sizeof(struct tofu_cntr));
    printf("sizeof(struct MPIDI_OFI_am_header) = %ld\n",
	   sizeof(struct MPIDI_OFI_am_header));
    printf("sizeof(struct tofu_vname) = %ld\n", sizeof(struct tofu_vname));

    jt_xyzabc.s.x = 10; jt_xyzabc.s.y = 11;  jt_xyzabc.s.z = 12;
    jt_xyzabc.s.a = 1; jt_xyzabc.s.b = 0;  jt_xyzabc.s.c = 1;
    bcopy(&jt_xyzabc, xyzabc, 6);
    printf("jtofu_phys_coords: x(%d) y (%d) z (%d) a(%d) b (%d) c (%d)\n",
	   jt_xyzabc.s.x, jt_xyzabc.s.y, jt_xyzabc.s.z,
	   jt_xyzabc.s.a, jt_xyzabc.s.b, jt_xyzabc.s.c);
    printf("uint8_t[6]: x(%d) y (%d) z (%d) a(%d) b (%d) c (%d)\n",
	   xyzabc[0], xyzabc[1], xyzabc[2], xyzabc[3], xyzabc[4], xyzabc[5]);
   
}
