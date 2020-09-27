#include "utf/src/include/utf_conf.h"
#include "utf/src/include/utf_tofu.h"
#include "utf/src/include/utf_cqmgr.h"
#include "utf/src/include/utf_externs.h"
#include "utf/src/include/utf_errmacros.h"
#include "utf/src/include/utf_debug.h"
#include "utf/src/include/utf_queue.h"
#include "utf/src/include/utf_timer.h"
#include "utf/src/include/utf_msgmacros.h"
#include "utf/src/include/utf.h"

#define TFI_UTF_TX_CTX	1
#define TFI_UTF_RX_CTX	2
#define TFI_FIFLGS_TAGGED	MSGHDR_FLGS_FI_TAGGED /* 0x02: see utf_queue.h */
#define TFI_FIFLGS_CQDATA	0x04	/* FI_REMOTE_CQ_DATA */

extern int tfi_progress_compl_pending;

struct tofu_av;
extern int	tfi_utf_init_1(struct tofu_av*, struct tofu_ctx*, int cls, struct tni_info*, size_t pigsz);
extern void	tfi_utf_init_2(struct tofu_av *av, struct tni_info *tinfo, int nprocs);
extern void	tfi_utf_finalize(struct tni_info*);
extern void	tfi_dom_setuptinfo(struct tni_info *tinfo, struct tofu_vname *vnmp);
extern int	tfi_utf_cancel(struct tofu_ctx*, void *context);

struct tni_info;
static inline void
tfi_utf_progress(struct tni_info*tinfo)
{
    utf_progress();
}
