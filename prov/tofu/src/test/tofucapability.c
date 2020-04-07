#include <stdio.h>
#include <utofu.h>

int
main(int argc, char **argv)
{
    static utofu_tni_id_t	tniid;
    struct utofu_onesided_caps *tni_caps;
    int	rc;
    
    rc = utofu_query_onesided_caps(tniid, &tni_caps);
    if (rc != 0) {
	printf("utofu_query_onesided_caps error: %d\n", rc);
	return -1;
    }
    printf("flags:\t\t\t %lx\n", tni_caps->flags);
    printf("armw_ops:\t\t %lx\n", tni_caps->armw_ops);
    printf("num_cmp_ids:\t\t %d\n", tni_caps->num_cmp_ids);
    printf("cache_line_size:\t%ld B\n", tni_caps->cache_line_size);
    printf("stag_address_alignment: %ld B\n", tni_caps->stag_address_alignment);
    printf("max_toq_desc_size:\t %ld B\n", tni_caps->max_toq_desc_size);
    printf("max_putget_size:\t %ld MiB (%ld B)\n", tni_caps->max_putget_size/(1024*1024), tni_caps->max_putget_size);
    printf("max_piggyback_size:\t %ld B\n", tni_caps->max_piggyback_size);
    printf("max_edata_size:\t\t %ld\n", tni_caps->max_edata_size);
    printf("max_mtu:\t\t %ld B\n", tni_caps->max_mtu);
    printf("max_gap:\t\t %ld B\n", tni_caps->max_gap);
    fflush(stdout);
    return 0;
}
