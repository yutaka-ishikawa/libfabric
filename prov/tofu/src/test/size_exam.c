#define _GNU_SOURCE
#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include "tofu_impl.h"
#include "tofu_macro.h"
//#include "../tofu_debug.c"

int mypid, myrank;

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
}
