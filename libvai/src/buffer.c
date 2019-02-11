// Copyright(c) 2017, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include "opae/access.h"
#include "opae/utils.h"
#include "common_int.h"
#include "intel-fpga.h"
#include "dlmalloc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

/* Others */
#define KB 1024
#define MB (1024 * KB)
#define GB (1024UL * MB)

#define PROTECTION (PROT_READ | PROT_WRITE)

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif
#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif

#define MAP_1G_HUGEPAGE	(0x1e << MAP_HUGE_SHIFT) /* 2 ^ 0x1e = 1G */

#ifdef __ia64__
#define ADDR (void *)(0x8000000000000000UL)
#define FLAGS_4K (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)
#define FLAGS_2M (FLAGS_4K | MAP_HUGETLB)
#define FLAGS_1G (FLAGS_2M | MAP_1G_HUGEPAGE)
#else
#define ADDR (void *)(0x0UL)
#define FLAGS_4K (MAP_PRIVATE | MAP_ANONYMOUS)
#define FLAGS_2M (FLAGS_4K | MAP_HUGETLB)
#define FLAGS_1G (FLAGS_2M | MAP_1G_HUGEPAGE)
#endif


/*
 * Allocate (mmap) new buffer
 */
static fpga_result buffer_allocate(void **addr, uint64_t len, int flags)
{
	void *addr_local = NULL;

	UNUSED_PARAM(flags);

	ASSERT_NOT_NULL(addr);

	/* ! FPGA_BUF_PREALLOCATED, allocate memory using huge pages
	   For buffer > 2M, use 1G-hugepage to ensure pages are
	   contiguous */
	if (len > 2 * MB)
		addr_local = mmap(ADDR, len, PROTECTION, FLAGS_1G, 0, 0);
	else if (len > 4 * KB)
		addr_local = mmap(ADDR, len, PROTECTION, FLAGS_2M, 0, 0);
	else
		addr_local = mmap(ADDR, len, PROTECTION, FLAGS_4K, 0, 0);
	if (addr_local == MAP_FAILED) {
		if (errno == ENOMEM) {
			if (len > 2 * MB)
				FPGA_MSG("Could not allocate buffer (no free 1 "
					 "GiB huge pages)");
			if (len > 4 * KB)
				FPGA_MSG("Could not allocate buffer (no free 2 "
					 "MiB huge pages)");
			else
				FPGA_MSG("Could not allocate buffer (out of "
					 "memory)");
			return FPGA_NO_MEMORY;
		}
		FPGA_MSG("FPGA buffer mmap failed: %s", strerror(errno));
		return FPGA_INVALID_PARAM;
	}

	*addr = addr_local;
	return FPGA_OK;
}

/*
 * Release (unmap) allocated buffer
 */
static fpga_result buffer_release(void *addr, uint64_t len)
{
	/* If the buffer allocation was backed by hugepages, then
	 * len must be rounded up to the nearest hugepage size,
	 * otherwise munmap will fail.
	 *
	 * Buffer with size larger than 2MB is backed by 1GB page(s),
	 * round up the size to the nearest GB boundary.
	 *
	 * Buffer with size smaller than 2MB but larger than 4KB is
	 * backed by a 2MB pages, round up the size to 2MB.
	 *
	 * Buffer with size smaller than 4KB is backed by a 4KB page,
	 * and its size is already 4KB aligned.
	 */

	if (len > 2 * MB)
		len = (len + (1 * GB - 1)) & (~(1 * GB - 1));
	else if (len > 4 * KB)
		len = 2 * MB;

	if (munmap(addr, len)) {
		FPGA_MSG("FPGA buffer munmap failed: %s",
			 strerror(errno));
		return FPGA_INVALID_PARAM;
	}

	return FPGA_OK;
}

fpga_result __FPGA_API__ fpgaPrepareBuffer(fpga_handle handle, uint64_t len,
					   void **buf_addr, uint64_t *wsid,
					   int flags)
{
	void *addr = NULL;
	fpga_result result = FPGA_OK;
	struct _fpga_handle *_handle = (struct _fpga_handle *) handle;
	int err;

	bool preallocated = (flags & FPGA_BUF_PREALLOCATED);
	bool quiet = (flags & FPGA_BUF_QUIET);

	uint64_t pg_size;

	result = handle_check_and_lock(_handle);
	if (result)
		return result;

	/* Assure wsid is a valid pointer */
	if (!wsid) {
		FPGA_MSG("WSID is NULL");
		result = FPGA_INVALID_PARAM;
		goto out_unlock;
	}

	if (flags & (~(FPGA_BUF_PREALLOCATED | FPGA_BUF_QUIET))) {
		FPGA_MSG("Unrecognized flags");
		result = FPGA_INVALID_PARAM;
		goto out_unlock;
	}

    if (!buf_addr) {
        FPGA_MSG("buf_addr is NULL");
        result = FPGA_INVALID_PARAM;
        goto out_unlock;
    }

    if (preallocated) {
        FPGA_MSG("doesn't support preallocated PrepareBuffer");
        result = FPGA_INVALID_PARAM;
        goto out_unlock;
    }
    
    *buf_addr = mspace_malloc(_handle->mspace, len);
    if (*buf_addr == NULL) {
        *wsid = 0;
        result = FPGA_NO_MEMORY;
    }
    else {
        *wsid = (uint64_t) *buf_addr;
        result = FPGA_OK;
    }

out_unlock:
	err = pthread_mutex_unlock(&_handle->lock);
	if (err) {
		FPGA_ERR("pthread_mutex_unlock() failed: %s", strerror(err));
	}
	return result;
}

fpga_result __FPGA_API__ fpgaReleaseBuffer(fpga_handle handle, uint64_t wsid)
{
	void *buf_addr;
	uint64_t iova;
	uint64_t len;
    int err;

	struct _fpga_handle *_handle = (struct _fpga_handle *)handle;
	fpga_result result = FPGA_NOT_FOUND;

	result = handle_check_and_lock(_handle);
	if (result)
		return result;
    mspace_free(_handle->mspace, (void *)wsid);
	/* Return */
	result = FPGA_OK;

out_unlock:
	err = pthread_mutex_unlock(&_handle->lock);
	if (err) {
		FPGA_ERR("pthread_mutex_unlock() failed: %s", strerror(err));
	}
	return result;
}

fpga_result __FPGA_API__ fpgaGetIOAddress(fpga_handle handle, uint64_t wsid,
					  uint64_t *ioaddr)
{
    *ioaddr = wsid;
	return FPGA_OK;
}