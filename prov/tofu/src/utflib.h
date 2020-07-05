#define UTF_TX_CTX	1
#define UTF_RX_CTX	2

extern int	utf_init_1(void *av, void *ctx, int class, void *tinfo, size_t pigsz);
extern int	utf_init_2(void *av, void *tinfo, int nprocs);
//extern int	utf_progress(struct tni_info*);
extern int	utf_progress(void*);
extern int	utf_send(utofu_vcq_hdl_t, int dst,
			 utofu_vcq_id_t rvcqid, uint64_t flgs, int myrank,
			 size_t size, int tag, void *buf, int *req);
extern int	utf_recv(int src, size_t size, int tag, void *buf, int *req);
extern int	utf_test(void*, int reqid);
extern int	utf_wait(void*, int reqid);
extern void	utf_finalize(void*);
extern void	utf_chnclean(void*);
struct tofu_vname;
extern void	utf_vname_vcqid(struct tofu_vname *);
struct tni_info;
extern void	tofu_dom_setuptinfo(struct tni_info *tinfo, struct tofu_vname *vnmp);
