/*
 * Copyright (c) 2010, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */
/*
 * tiler.h
 *
 * TILER driver support functions for TI OMAP processors.
 */

#ifndef TILERUSR_H
#define TILERUSR_H

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <atomic.h>
#include <stdbool.h>
#include <devctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <hw/inout.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/procmgr.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <sys/ioctl.h>

#include "tiler/tiler_devctl.h"

#define TILER_DEVICE_PATH "/dev/tiler"

#define TILERUSR_ERR_NONE (0)
#define TILERUSR_ERR_GENERIC (-1)



/**
 * Reserves a 1D or 2D TILER block area and memory for the
 * current process with group ID 0.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 *
 * @return error status
 */
int32_t tiler_alloc(enum tiler_fmt fmt, uint32_t width, uint32_t height, uint32_t *sys_addr);

/**
 * Reserves a 1D or 2D TILER block area and memory with extended
 * arguments.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param align 	block alignment (default: PAGE_SIZE)
 * @param offs 		block offset
 * @param gid 		group ID
 * @param pid 		process ID
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 *
 * @return error status
 */
int32_t tiler_allocx(enum tiler_fmt fmt, uint32_t width, uint32_t height,
			uint32_t align, uint32_t offs, uint32_t gid, pid_t pid, uint32_t *sys_addr);

/**
 * Maps an existing buffer to a 1D or 2D TILER area for the
 * current process with group ID 0.
 *
 * Currently, only 1D area mapping is supported.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param usr_addr	user space address of existing buffer.
 *
 * @return error status
 */
int32_t tiler_map(enum tiler_fmt fmt, uint32_t width, uint32_t height, uint32_t *sys_addr,
								uint32_t usr_addr);

/**
 * Maps an existing buffer to a 1D or 2D TILER area with
 * extended arguments.
 *
 * Currently, only 1D area mapping is supported.
 *
 * NOTE: alignment is always PAGE_SIZE and offset is 0
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param gid 		group ID
 * @param pid 		process ID
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param usr_addr	user space address of existing buffer.
 *
 * @return error status
 */
int32_t tiler_mapx(enum tiler_fmt fmt, uint32_t width, uint32_t height,
			uint32_t gid, pid_t pid, uint32_t *sys_addr, uint32_t usr_addr);

/**
 * Free TILER memory.
 *
 * @param sys_addr system space (L3) address.
 *
 * @return an error status.
 */
int32_t tiler_free(uint32_t sys_addr);

/**
 * Reserves tiler area for n identical set of blocks (buffer)
 * for the current process.  Use this method to get optimal
 * placement of multiple related tiler blocks; however, it may
 * not reserve area if tiler_alloc is equally efficient.
 *
 * @param n 	number of identical set of blocks
 * @param b	information on the set of blocks (ptr, ssptr and
 *  		stride fields are ignored)
 *
 * @return error status
 */
int32_t tiler_reserve(uint32_t n, struct tiler_buf_info *b);

/**
 * Reserves tiler area for n identical set of blocks (buffer) fo
 * a given process. Use this method to get optimal placement of
 * multiple related tiler blocks; however, it may not reserve
 * area if tiler_alloc is equally efficient.
 *
 * @param n 	number of identical set of blocks
 * @param b	information on the set of blocks (ptr, ssptr and
 *  		stride fields are ignored)
 * @param pid   process ID
 *
 * @return error status
 */
int32_t tiler_reservex(uint32_t n, struct tiler_buf_info *b, pid_t pid);

uint32_t tiler_reorient_addr(uint32_t tsptr, struct tiler_view_orient orient);

uint32_t tiler_get_natural_addr(void *sys_ptr);

uint32_t tiler_reorient_topleft(uint32_t tsptr, struct tiler_view_orient orient,
				uint32_t width, uint32_t height);

uint32_t tiler_stride(uint32_t tsptr);

void tiler_rotate_view(struct tiler_view_orient *orient, uint32_t rotation);

void tiler_alloc_packed(int32_t *count, enum tiler_fmt fmt, uint32_t width, uint32_t height,
			void **sysptr, void **allocptr, int32_t aligned);

void tiler_alloc_packed_nv12_opt(int32_t *count, uint32_t width, uint32_t height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, int32_t aligned);

void tiler_alloc_packed_nv12(int32_t *count, uint32_t width, uint32_t height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, int32_t aligned);

int tiler_reg_notifier(char *name, uint32_t cmd);

int tiler_unreg_notifier(char *name, uint32_t cmd);

int32_t tiler_alloc_block_area(enum tiler_fmt fmt, uint32_t width,
                               uint32_t height, uint32_t *sys_addr,
                               uint32_t *num_pages);
int32_t tiler_free_block_area(uint32_t sys_addr);
int32_t tiler_map_block(uint32_t sys_addr, uint32_t num_pages, uint32_t *pages);
int32_t tiler_unmap_block(uint32_t sys_addr);

int tiler_close(void);
int tiler_open(void);

#endif
