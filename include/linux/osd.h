/*
 * Copyright (c) 2015 Los Alamos Nat. Security, LLC. All rights reserved.
 * Copyright (c) 2018 Amazon.com, Inc. or its affiliates. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _LINUX_OSD_H_
#define _LINUX_OSD_H_

/*#define _GNU_SOURCE*/

#include <byteswap.h>
#include <endian.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#include <ifaddrs.h>
#include "unix/osd.h"
#include "rdma/fi_errno.h"

static inline int ofi_shm_remap(struct util_shm *shm,
				size_t newsize, void **mapped)
{
	shm->ptr = mremap(shm->ptr, shm->size, newsize, 0);
	shm->size = newsize;
	*mapped = shm->ptr;
	return shm->ptr == MAP_FAILED ? -FI_EINVAL : FI_SUCCESS;
}

ssize_t ofi_get_hugepage_size(void);

#ifdef FX100
static inline int ofi_alloc_hugepage_buf(void **memptr, size_t size)
{
	return -1;
}

#else
static inline int ofi_alloc_hugepage_buf(void **memptr, size_t size)
{
	*memptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

	if (*memptr == MAP_FAILED)
		return -errno;

	return FI_SUCCESS;
}
#endif /* FX100 */

static inline int ofi_free_hugepage_buf(void *memptr, size_t size)
{
	return munmap(memptr, size);
}

size_t ofi_ifaddr_get_speed(struct ifaddrs *ifa);

#endif /* _LINUX_OSD_H_ */
