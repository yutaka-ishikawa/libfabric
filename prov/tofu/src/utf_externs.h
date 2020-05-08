#include "utflib.h"
extern void	*utf_malloc(size_t);
extern void	utf_free(void*);
extern void	utf_engine_init();
extern void	utf_egrsbuf_init(utofu_vcq_hdl_t, int);
extern struct utf_egr_sbuf *utf_egrsbuf_alloc();
extern void	utf_egrsbuf_free(struct utf_egr_sbuf *);
struct utf_send_cntr;
extern struct utf_send_cntr *utf_scntr_alloc(int dst, utofu_vcq_id_t rvcid,
					     uint64_t flgs);
extern void utf_scntr_free(int dst);
extern struct utf_send_msginfo *utf_sndminfo_alloc();
extern void utf_sndminfo_free(struct utf_send_msginfo *smp);
extern void	utf_vcqidtab_init();
extern void	utf_scntr_init(utofu_vcq_hdl_t, int nprocs, int scntr_entries, int rmacntr_entries);
extern void	utf_egrsbuf_fin(utofu_vcq_hdl_t);
extern int	utf_send_start(utofu_vcq_hdl_t, struct utf_send_cntr *usp);
extern int	utf_rmwrite_engine(utofu_vcq_id_t, struct utf_send_cntr *usp);
extern void	utf_recvbuf_init(utofu_vcq_id_t, int nprocs);
extern void	utf_sndminfo_init(utofu_vcq_hdl_t, int entries);
extern int	is_scntr(utofu_stadd_t, int*);
extern int	is_recvbuf(utofu_stadd_t val);

extern int	utf_remote_add(utofu_vcq_hdl_t vcqh,
			       utofu_vcq_id_t rvcqid, unsigned long flgs,
			       uint64_t val, utofu_stadd_t rstadd,
			       uint64_t edata, void *cbdata);
extern int	utf_remote_armw4(utofu_vcq_hdl_t vcqh,
				 utofu_vcq_id_t rvcqid, unsigned long flgs,
				 enum utofu_armw_op op, uint64_t val,
				 utofu_stadd_t rstadd,
				 uint64_t edata, void *cbdata);

extern int	remote_piggysend(utofu_vcq_hdl_t vcqh,
				 utofu_vcq_id_t rvcqid, void *data,
				 utofu_stadd_t rstadd, size_t len,
				 uint64_t edata, unsigned long flgs, void *cb);
extern int	remote_put(utofu_vcq_hdl_t vcqh,
			   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd,
			   utofu_stadd_t rstadd, size_t len, uint64_t edata,
			   unsigned long flgs, void *cbdata);
extern int	remote_get(utofu_vcq_hdl_t vcqh,
			   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd,
			   utofu_stadd_t rstadd, size_t len, uint64_t edata,
			   unsigned long flgs, void *cbdata);
extern struct utf_send_cntr *utf_idx2scntr(int idx);
extern struct utf_msgbdy *utf_recvbuf_get(int idx);
extern void	utf_msgreq_init();
extern void	utf_msglst_init();
extern int	utf_progress();
extern utofu_stadd_t	utf_mem_reg(utofu_vcq_hdl_t, void *buf, size_t size);
extern void		utf_mem_dereg(utofu_vcq_id_t, utofu_stadd_t stadd);
extern void	utf_stadd_free();
extern void	utf_rmacq_init();
extern struct utf_rma_cq *utf_rmacq_alloc();
extern void	utf_rmacq_free(struct utf_rma_cq *cq);
extern void	utf_setmsgmode(int mode);

/* for debugging */
extern int	utf_getenvint(char *);
extern void	utf_show_msgmode(FILE *fp);
extern void	utf_show_data(char *msg, char *data, size_t len);
extern void	utf_redirect();
extern int	utf_printf(const char *fmt, ...);
extern int	utf_fprintf(FILE*, const char *fmt, ...);
extern void	mrq_notice_show(struct utofu_mrq_notice *ntcp);
extern void	erecvbuf_dump(int, int);
extern void	erecvbuf_printcntr();
extern int	get_keyval(int rank, char *key, unsigned long *val);
extern void	vcq_info_dump(const char *msg, utofu_vcq_id_t rvcqid,
			      uint8_t *coords, utofu_tni_id_t tniid,
			      utofu_cq_id_t cqid);
struct utf_msgbdy;
extern void	utf_showpacket(char *msg, struct utf_msgbdy *mbp);
extern void	utf_setmsgmode(int);
extern void	utf_show_recv_cntr(FILE*);

extern int	utf_dflag;
extern int	mypid, myrank;
extern int	utf_initialized;
extern size_t	utf_pig_size;	/* piggyback size */
extern utofu_stadd_t	erbstadd;
extern utofu_stadd_t	egrmgtstadd;
extern struct msg_q_check	*utf_expq_chk;
extern struct msg_q_check	*utf_uexpq_chk;
extern utofu_stadd_t	rmacmplistadd;
