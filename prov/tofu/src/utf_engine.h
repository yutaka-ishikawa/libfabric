static inline void
utf_reset_chain_info(struct utf_send_cntr *usp)
{
    usp->chn_ready.data = 0; 
    usp->chn_next.rank = RANK_ALL1;
    usp->evtupdt = 0;
    usp->expevtupdt = 0;
    usp->chn_informed = 0;
}
