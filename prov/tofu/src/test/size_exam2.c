#define _GNU_SOURCE
#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include <stdint.h>
#include <utofu.h>
int utf_dflag;
extern int	utf_printf(const char *fmt, ...);

#include "utf_conf.h"
#include "utf_queue.h"
#include "tofu_conf.h"

int mypid;

#include "utf_errmacros.h"
#include "utf_sndmgt.h"

struct utf_msghdr2 { /* 27 Byte */
    uint32_t	src;
    uint64_t	tag;
    uint64_t	size:40,
		flgs:24;
    uint64_t	data;
};

int
main()
{
    printf("sizeof(utf_msghdr2) = %ld\n", sizeof(struct utf_msghdr2));
    printf("MSG_EAGER_SIZE = %ld\n", MSG_EAGER_SIZE);
    printf("MSG_SIZE = %ld\n", MSG_SIZE);
    printf("MSGBUF_SIZE = %ld\n", MSGBUF_SIZE);
    printf("CONF_TOFU_INJECTSIZE = %ld\n", CONF_TOFU_INJECTSIZE);
    if (MSG_EAGER_SIZE < CONF_TOFU_INJECTSIZE) {
	printf("\t CONF_TOFU_INJECTSIZE(%ld) in tofu_conf.h must be changed\n", CONF_TOFU_INJECTSIZE);
    }
    printf("MSG_PAYLOAD_SIZE = %ld\n", MSG_PAYLOAD_SIZE);
    printf("sizeof(struct utf_msghdr) = %ld\n", sizeof(struct utf_msghdr));
    printf("sizeof(struct utf_hpacket) = %ld\n", sizeof(struct utf_hpacket));
    printf("sizeof(struct utf_msgbdy) = %ld\n", sizeof(struct utf_msgbdy));
    printf("sizeof(struct sndmgt) = %ld\n", sizeof(struct sndmgt));
}
