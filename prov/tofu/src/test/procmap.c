#include <pmix.h>
#include <pmix_fjext.h>
#include <process_map_info.h>
#include <utofu.h>
#include <stdio.h>

pmix_proc_t	pmix_proc[1];
pmix_value_t	*pval;
struct tlib_process_mapinfo	*minfo;
char	buf1[128], buf2[128], buf3[128];
int	myrank, nprocs;

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

extern void utofu_addr(int rank);

int
main(int argc, char **argv)
{
    int rc;
    
    rc = PMIx_Init(pmix_proc, NULL, 0);
    if (rc != PMIX_SUCCESS) {
	printf("rc = %d\n", rc); fflush(stdout);
	exit(-1);
    }
    utofu_addr(pmix_proc->rank);
    myrank = pmix_proc->rank;
    pmix_proc->rank= PMIX_RANK_WILDCARD;
    rc = PMIx_Get(pmix_proc, PMIX_JOB_SIZE, NULL, 0, &pval);
    if (rc != PMIX_SUCCESS) goto err;
    nprocs = pval->data.uint32;
    // PMIx_Fence(pmix_proc, nprocs, NULL, 0);
    if (myrank != 0) goto finalize;

    usleep(10000);
    printf("job size = %d\n", nprocs); fflush(stdout);
    rc = PMIx_Get(pmix_proc, FJPMIX_RANKMAP, NULL, 0, &pval);
    if (rc != PMIX_SUCCESS | pval->type != PMIX_BYTE_OBJECT) {
	printf("pval->type = %d mustbe 27\n", pval->type); goto err;
    }
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
    printf("offset_hostlist=0x%lx\n",  minfo->offset_hostlist);
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
    pmix_proc->rank= myrank; 
    // PMIx_Fence(pmix_proc, nprocs, NULL, 0);
    goto ext;
err:
    printf("rc = %d\n", rc); fflush(stdout);
ext:
    PMIx_Finalize(NULL, 0);
    return 0;
}

#define CONF_TOFU_CMPID         0x7
void
utofu_addr(int rank)
{
    int			uc;
    utofu_vcq_hdl_t     vcqh;
    utofu_tni_id_t      tni_id;
    const utofu_cmp_id_t c_id = CONF_TOFU_CMPID;
    const unsigned long     flags;

    uc = utofu_create_vcq_with_cmp_id(tni_id, c_id, flags, &vcqh);
    {
	utofu_vcq_id_t vcqi = -1UL;
	uint8_t abcxyz[8];
	uint16_t tni[1], tcq[1], cid[1];

	uc = utofu_query_vcq_id(vcqh, &vcqi);
	if (uc != UTOFU_SUCCESS) {
	    fprintf(stderr, "%s: utofu_query_vcq_id error (%d)\n", __func__, uc);
	    goto bad;
	}
	uc = utofu_query_vcq_info(vcqi, abcxyz, tni, tcq, cid);
	if (uc != UTOFU_SUCCESS) {
	    fprintf(stderr, "%s: utofu_query_vcq_id error (%d)\n", __func__, uc);
	    goto bad;
	}
	printf("%d : xyzabc(%d:%d:%d:%d:%d:%d), tni(%d), cqid(%d), cid(0x%x)\n",
	       rank, abcxyz[0], abcxyz[1], abcxyz[2], abcxyz[3], abcxyz[4], abcxyz[5],
	       tni[0], tcq[0], cid[0]); fflush(stdout);
bad:
	;
    }
    return;
}
