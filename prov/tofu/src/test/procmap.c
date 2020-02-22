#include <pmix.h>
#include <process_map_info.h>
#include <stdio.h>

pmix_proc_t	pmix_proc[1];
pmix_value_t	*pval;

int
main(int argc, char **argv)
{
    int rc;
    
    rc = PMIx_Init(pmix_proc, NULL, 0);
    printf("rc = %d\n", rc);
    pmix_proc->rank= PMIX_RANK_WILDCARD;
    rc = PMIx_Get(pmix_proc, PMIX_FJ_RANKMAP, NULL, 0, &pval);
    printf("rc = %d pval->type = %d mustbe 27\n", rc, pval->type);
    printf("pval->bo.bytes(%p) pval->bo.size(%ld)\n",
	   pval->data.bo.bytes, pval->data.bo.size);
    
    PMIx_Finalize(NULL, 0);
    return 0;
}
