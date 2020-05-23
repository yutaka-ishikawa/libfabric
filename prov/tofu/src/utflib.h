#define UTF_TX_CTX	1
#define UTF_RX_CTX	2

extern int	utf_init_1(void *ctx, int class, utofu_vcq_hdl_t vcqh, size_t pigsz);
extern int	utf_init_2(utofu_vcq_hdl_t, int nprocs);
extern int	utf_progress(void*, utofu_vcq_hdl_t);
extern int	utf_rma_progress();
extern int	utf_send(utofu_vcq_hdl_t, int dst,
			 utofu_vcq_id_t rvcqid, uint64_t flgs, int myrank,
			 size_t size, int tag, void *buf, int *req);
extern int	utf_recv(int src, size_t size, int tag, void *buf, int *req);
extern int	utf_test(void*, utofu_vcq_hdl_t, int reqid);
extern int	utf_wait(void*, utofu_vcq_hdl_t, int reqid);
extern void	utf_finalize(void*, utofu_vcq_hdl_t);
extern void	utf_chnclean(void*, utofu_vcq_hdl_t);
