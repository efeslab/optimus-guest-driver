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
#include "vai_utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>

/* Port UAFU */
#define AFU_PERMISSION (FPGA_REGION_READ | FPGA_REGION_WRITE | FPGA_REGION_MMAP)
#define AFU_SIZE	0x40000
#define AFU_OFFSET	0

fpga_result __FPGA_API__ fpgaWriteMMIO32(fpga_handle handle,
					 uint32_t mmio_num,
					 uint64_t offset,
					 uint32_t value)
{
    NOIMPL
}

fpga_result __FPGA_API__ fpgaReadMMIO32(fpga_handle handle,
					uint32_t mmio_num,
					uint64_t offset,
					uint32_t *value)
{
}

fpga_result __FPGA_API__ fpgaWriteMMIO64(fpga_handle handle,
					 uint32_t mmio_num,
					 uint64_t offset,
					 uint64_t value)
{
	int err;
	struct _fpga_handle *_handle = (struct _fpga_handle *) handle;
	fpga_result result = FPGA_OK;
    UNUSED_PARAM(mmio_num);

	if (offset % sizeof(uint64_t) != 0) {
		FPGA_ERR("Misaligned MMIO access");
		return FPGA_INVALID_PARAM;
	}
    if (offset > MMIO_SPACE_LENGTH) {
        FPGA_ERR("Offset > MMIO_SPACE");
        return FPGA_INVALID_PARAM;
    }
    if (handle == NULL) {
        FPGA_ERR("handle is NULL");
        return FPGA_INVALID_PARAM;
    }

	result = handle_check_and_lock(_handle);
	if (result)
		return result;

    *((volatile uint64_t *) ((volatile uint8_t *)_handle->mmio_bar + offset)) = value;

out_unlock:
	err = pthread_mutex_unlock(&_handle->lock);
	if (err) {
		FPGA_ERR("pthread_mutex_unlock() failed: %s", strerror(err));
	}
	return result;
}

fpga_result __FPGA_API__ fpgaReadMMIO64(fpga_handle handle,
					uint32_t mmio_num,
					uint64_t offset,
					uint64_t *value)
{
	int err;
	struct _fpga_handle *_handle = (struct _fpga_handle *) handle;
	fpga_result result = FPGA_OK;
    UNUSED_PARAM(mmio_num);

	if (offset % sizeof(uint64_t) != 0) {
		FPGA_MSG("Misaligned MMIO access");
		return FPGA_INVALID_PARAM;
	}
    if (offset > MMIO_SPACE_LENGTH) {
        FPGA_ERR("Offset > MMIO_SPACE");
        return FPGA_INVALID_PARAM;
    }
    if (handle == NULL) {
        FPGA_ERR("handle is NULL");
        return FPGA_INVALID_PARAM;
    }

	result = handle_check_and_lock(_handle);
	if (result)
		return result;

	*value = *((volatile uint64_t *) ((volatile uint8_t *)_handle->mmio_bar + offset));

out_unlock:
	err = pthread_mutex_unlock(&_handle->lock);
	if (err) {
		FPGA_ERR("pthread_mutex_unlock() failed: %s", strerror(err));
	}
	return result;
}

fpga_result __FPGA_API__ fpgaMapMMIO(fpga_handle handle,
				     uint32_t mmio_num,
				     uint64_t **mmio_ptr)
{
    NOIMPL
}

fpga_result __FPGA_API__ fpgaUnmapMMIO(fpga_handle handle,
				       uint32_t mmio_num)
{
    NOIMPL
}
