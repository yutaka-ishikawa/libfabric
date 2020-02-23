#include <pmix.h>
#include <pmix_fjext.h>
#include <process_map_info.h>
#include <stdio.h>

pmix_proc_t	pmix_proc[1];
pmix_value_t	*pval;
struct tlib_process_mapinfo	*minfo;
char	buf1[128], buf2[128], buf3[128];

/*
 * 31             24 23           16 15            8 7 6 5  2 1 0
 * +---------------+---------------+---------------+---+----+---+
 * |              x|              y|              z|  a|   b|  c|
 * +---------------+---------------+---------------+---+----+---+
 */
struct tofu_coord {
    uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
};

char *
string_tofu6d(uint32_t coord, char *buf)
{
    struct tofu_coord tc = *(struct tofu_coord*)(&coord);
    snprintf(buf, 128, "x(%02d) y(%02d) z(%02d) a(%02d) b(%02d) c(%02d)",
	     tc.x, tc.y, tc.z, tc.a, tc.b, tc.c);
    return buf;
}

char *
string_8bit(uint32_t u32, char *buf)
{
    snprintf(buf, 128, "%02x:%02x:%02x:%02x",
	     u32>>24, (u32>>16)&0xff, (u32>>8)&0xff, u32&0xff);
    return buf;
}

char *
string_tofu3d(tlib_tofu3d host, char *buf)
{
    snprintf(buf, 128, "x(%02d) y(%02d) z(%02d)", host.x, host.y, host.z);
    return buf;
}


int
main(int argc, char **argv)
{
    int rc, myrank;
    
    rc = PMIx_Init(pmix_proc, NULL, 0);
    if (rc != PMIX_SUCCESS) {
	printf("rc = %d\n", rc); fflush(stdout);
	exit(-1);
    }
    if (pmix_proc->rank != 0) goto finalize;
    myrank = pmix_proc->rank;
    pmix_proc->rank= PMIX_RANK_WILDCARD;
    rc = PMIx_Get(pmix_proc, FJPMIX_RANKMAP, NULL, 0, &pval);
    if (rc != PMIX_SUCCESS | pval->type != PMIX_BYTE_OBJECT) {
	printf("rc = %d pval->type = %d mustbe 27\n", rc, pval->type);
	exit(-1);
    }
    pmix_proc->rank= myrank; 
    printf("pval->data.bo.bytes(%p) pval->data.bo.size(%ld)\n",
	   pval->data.bo.bytes, pval->data.bo.size);
    minfo = (struct tlib_process_mapinfo*) pval->data.bo.bytes;

    printf("ver_major=%d ver_minor=%d\n", minfo->ver_major, minfo->ver_minor);
    printf("system_size=%x %s\n", minfo->system_size, string_8bit(minfo->system_size, buf1));
    printf("\t%s\n", string_tofu6d(minfo->system_size, buf1));
    printf("system_torus=%s\n", string_tofu6d(minfo->system_torus, buf1));
    printf("dimension=0x%x\n", minfo->dimension);
    printf("logical_size=%s\n", string_tofu3d(minfo->logical_size, buf1));
    printf("ref_addr=%s\n", string_tofu6d(minfo->ref_addr, buf1));
    printf("max_size=%s\n", string_tofu6d(minfo->max_size, buf1));
    printf("broken_icc=%d\n", minfo->broken_icc);
    printf("broken_node=%d\n", minfo->broken_node);
    printf("universe_size=%d\n", minfo->universe_size);
    printf("num_node_assign_bulk=%d\n", minfo->num_node_assign_bulk);
    printf("max_proc_per_node=%d\n", minfo->max_proc_per_node);
    printf("proc_per_node=%d\n", minfo->proc_per_node);
    printf("stack_sort=%d\n", minfo->stack_sort);
    printf("world_size=%d\n", minfo->world_size);
    printf("static_proc_shape=%s\n", string_tofu3d(minfo->static_proc_shape, buf1));
    printf("order=0x%x\n", minfo->order);

    printf("num_hostlist=%d\n",  minfo->num_hostlist);
    printf("offset_hostlist=0x%x\n",  minfo->offset_hostlist);
    {
	tlib_tofu3d	*hostp = (tlib_tofu3d*) TLIB_OFFSET2PTR(minfo->offset_hostlist);
	int	i;
	for (i = 0; i < minfo->num_hostlist; i++) {
	    printf("\thost=%s\n", string_tofu3d(*(hostp + i), buf1));
	}
    }
    printf("num_proc=%d\n",  minfo->num_proc);
    {
	struct tlib_ranklist	*rankp = (struct tlib_ranklist*) TLIB_OFFSET2PTR(minfo->offset_node_list);
	int	i;
	for(i = 0; i < minfo->num_proc; i++) {
	    printf("\tphys=%s virt=%s\n",
		   string_tofu6d((rankp + i)->physical_addr, buf1),
		   string_tofu3d((rankp + i)->virtual_addr, buf2));
	}
    }

    printf("mapinfo_size=0x%lx\n", minfo->mapinfo_size);
    printf("total_size=0x%lx\n", minfo->total_size);
    printf("allocation_mode=%d\n", minfo->allocation_mode);
    printf("system_size3d=%s\n", string_tofu3d(minfo->system_size3d, buf1));
    printf("system_torus3d=%s\n", string_tofu3d(minfo->system_torus3d, buf1));

finalize:
    PMIx_Fence(pmix_proc, 1, NULL, 0);
    PMIx_Finalize(NULL, 0);
    return 0;
}
