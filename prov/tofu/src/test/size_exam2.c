#define _GNU_SOURCE
#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include <stdint.h>
#include <utofu.h>
int utf_dflag;
extern int	utf_printf(const char *fmt, ...);

#include "utf_conf.h"
#include "utf_queue.h"


int
main()
{
    printf("MSG_EAGER_SIZE = %ld\n", MSG_EAGER_SIZE);
    printf("MSG_PAYLOAD_SIZE = %ld\n", MSG_PAYLOAD_SIZE);
    printf("sizeof(struct utf_msghdr) = %ld\n", sizeof(struct utf_msghdr));
    printf("sizeof(struct utf_hpacket) = %ld\n", sizeof(struct utf_hpacket));
    printf("sizeof(struct utf_msgbdy) = %ld\n", sizeof(struct utf_msgbdy));
}
