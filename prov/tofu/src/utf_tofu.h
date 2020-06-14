/* nrank is rank within node */
inline static void
utf_tni_select(int ppn, int nrnk, utofu_tni_id_t *tni, utofu_cq_id_t *cq)
{
    switch (ppn) {
    case 1: *tni = 0; if (cq) *cq = 0; break;
    case 2: /* 0, 6 */
	*tni = nrnk; if (cq) *cq = nrnk*6; break;
    case 3: /* 0, 3, 6 */
	*tni = nrnk; if (cq) *cq = nrnk*3; break;
    case 4: /* 0, 3, 6, 9 */
	*tni = nrnk; if (cq) *cq = nrnk*3; break;
    case 5: case 6: case 7: case 8:
	*tni = (nrnk%2)*3; if (cq) *cq = (nrnk/2)*3; break;
    case 9: case 10: case 11: case 12:
	*tni = (nrnk%3)*2; if (cq) *cq = (nrnk/3)*3; break;
    default:
	if (ppn >= 13 && ppn <=24) {
	    *tni = nrnk%6; if (cq) *cq = (nrnk/6)*3;
	} else if (ppn >= 25 && ppn <= 42) {
	    *tni = nrnk%6; if (cq) *cq = ((nrnk/6)*3)%10;
	} else {
	    *tni = nrnk%6; if (cq) *cq = ((nrnk/6)*3)%10;
	    if (cq && nrnk >= 42) *cq = 10;
	}
    }
}
