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
 * tilerusr.c
 *
 * TilerUsr Interface functions for TI OMAP processors.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>

#include <tilerusr/tiler.h>

#define __DEBUG__
#undef  __DEBUG_ENTRY__
#define __DEBUG_ASSERT__

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif
#include "tiler_utils/debug_utils.h"


#define TILERUSR_ERROR() \
	fprintf(stderr, "%s()::%d: errno(%d) - \"%s\"\n", \
			__FUNCTION__, __LINE__, errno, strerror(errno)); \
	fflush(stderr);

static int fd;

int tiler_close(void)
{
    close(fd);
    return TILERUSR_ERR_NONE;
}

int tiler_open(void)
{
    fd = open(TILER_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        TILERUSR_ERROR();
        return TILERUSR_ERR_GENERIC;
    }

    return TILERUSR_ERR_NONE;
}



int32_t tiler_mapx(enum tiler_fmt fmt, uint32_t width, uint32_t height, uint32_t gid,
		       pid_t pid, uint32_t *sys_addr, uint32_t usr_addr)
{
    int ret = -1;
    struct tiler_mapx_info info = {0};

    info.fmt = fmt;
    info.width = width;
    info.height = height;
    info.gid = gid;
    info.pid = pid;
    info.usr_addr = usr_addr;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRMX, &info);

    *sys_addr = info.sys_addr;

    return ret;
}

int32_t tiler_map(enum tiler_fmt fmt, uint32_t width, uint32_t height, uint32_t *sys_addr,
								uint32_t usr_addr)
{
	return tiler_mapx(fmt, width, height, 0, getgid(), sys_addr,
								usr_addr);
}

int32_t tiler_free(uint32_t sys_addr)
{
	int ret = -1;
	uint32_t addr = sys_addr;

	ret = ioctl(fd, TILIOC_USRF, &addr);

	return ret;
}

int32_t tiler_allocx(enum tiler_fmt fmt, uint32_t width, uint32_t height,
		 uint32_t align, uint32_t offs, uint32_t gid, pid_t pid, uint32_t *sys_addr)
{
    int ret = -1;
    struct tiler_allocx_info info = {0};

    info.fmt = fmt;
    info.width = width;
    info.height = height;
    info.align = align;
    info.offs = offs;
    info.gid = gid;
    info.pid = pid;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRGX, (unsigned long)(&info));

    *sys_addr = info.sys_addr;

    return ret;
}

int32_t tiler_alloc(enum tiler_fmt fmt, uint32_t width, uint32_t height, uint32_t *sys_addr)
{
	return tiler_allocx(fmt, width, height, 0, 0,
			    0, getgid(), sys_addr);
}

int32_t tiler_alloc_block_area(enum tiler_fmt fmt, uint32_t width,
				uint32_t height, uint32_t *sys_addr,
				uint32_t *num_pages)
{
    int ret = -1;
    struct tiler_alloc_block_area_info info = {0};

    info.fmt = fmt;
    info.width = width;
    info.height = height;
    info.gid = getgid();

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRGB, (unsigned long)(&info));

    *sys_addr = info.sys_addr;
    *num_pages = info.num_pages;

    return ret;
}

int32_t tiler_free_block_area(uint32_t sys_addr)
{
	int ret = -1;
	uint32_t addr = sys_addr;

	ret = ioctl(fd, TILIOC_USRFB, &addr);

	return ret;
}

int32_t tiler_map_block(uint32_t sys_addr, uint32_t num_pages, uint32_t *pages)
{
    int ret = -1;
	iov_t til_iov[2];
    struct tiler_map_block_info info = {0};
	uint32_t pages_info[num_pages];

    info.sys_addr = sys_addr;
    info.num_pages = num_pages;

	/* make ioctl call to Tiler resmgr. */
	memcpy(pages_info, pages, sizeof(uint32_t) * num_pages);

	SETIOV(&til_iov[0], &info, sizeof(struct tiler_map_block_info));
	SETIOV(&til_iov[1], pages_info, sizeof(uint32_t) * num_pages);
	ret = -devctlv(fd, TILIOC_USRMB, 2, 0, til_iov, NULL, NULL);

    return ret;
}

int32_t tiler_unmap_block(uint32_t sys_addr)
{
	int ret = -1;
	uint32_t addr = sys_addr;

	ret = ioctl(fd, TILIOC_USRUMB, &addr);

	return ret;
}

/* reserve area for n identical buffers */
int32_t tiler_reservex(uint32_t n, struct tiler_buf_info *b, pid_t pid)
{
    int ret = -1;
    struct tiler_reservex_info info = {0};

    info.n = n;
    memcpy(&info.b, b, sizeof(*b));
    info.pid = pid;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRRX, (unsigned long)(&info));

    return ret;
}

int32_t tiler_reserve(uint32_t n, struct tiler_buf_info *b)
{
	return tiler_reservex(n, b, getgid());
}

void tiler_alloc_packed(int32_t *count, enum tiler_fmt fmt, uint32_t width, uint32_t height,
			void **sysptr, void **allocptr, int32_t aligned)
{
	int ret = -1;
	iov_t til_iov[3];
	struct tiler_allocp_info info = {0};
	void *sysptr_info[*count];
	void *allocptr_info[*count];

	info.count = *count;
	info.fmt = fmt;
	info.width = width;
	info.height = height;
	info.aligned = aligned;

	memcpy(sysptr_info, sysptr, sizeof(void*) * *count);
	memcpy(allocptr_info, allocptr, sizeof(void*) * *count);

	SETIOV(&til_iov[0], &info, sizeof(struct tiler_allocp_info));
	SETIOV(&til_iov[1], sysptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[2], allocptr_info, sizeof(void*) * *count);
	ret = -devctlv(fd, TILIOC_USRGP, 3, 3, til_iov, til_iov, NULL);

	*count = info.count;
	memcpy(sysptr, sysptr_info, sizeof(void*) * *count);
	memcpy(allocptr, allocptr_info, sizeof(void*) * *count);

	return;
}

void tiler_alloc_packed_nv12_opt(int32_t *count, uint32_t width, uint32_t height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, int32_t aligned)
{
	int ret = -1;
	iov_t til_iov[5];
	struct tiler_allocpnv12_info info = {0};
	void *y_sysptr_info[*count];
	void *uv_sysptr_info[*count];
	void *y_allocptr_info[*count];
	void *uv_allocptr_info[*count];

	info.count = *count;
	info.width = width;
	info.height = height;
	info.aligned = aligned;

	memcpy(y_sysptr_info, y_sysptr, sizeof(void*) * *count);
	memcpy(uv_sysptr_info, uv_sysptr, sizeof(void*) * *count);
	memcpy(y_allocptr_info, y_allocptr, sizeof(void*) * *count);
	memcpy(uv_allocptr_info, uv_allocptr, sizeof(void*) * *count);

	SETIOV(&til_iov[0], &info, sizeof(struct tiler_allocpnv12_info));
	SETIOV(&til_iov[1], y_sysptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[2], uv_sysptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[3], y_allocptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[4], uv_allocptr_info, sizeof(void*) * *count);
	ret = -devctlv(fd, TILIOC_USRGPNV12OPT, 5, 5, til_iov, til_iov, NULL);

	*count = info.count;
	memcpy(y_sysptr, y_sysptr_info, sizeof(void*) * *count);
	memcpy(uv_sysptr, uv_sysptr_info, sizeof(void*) * *count);
	memcpy(y_allocptr, y_allocptr_info, sizeof(void*) * *count);
	memcpy(uv_allocptr, uv_allocptr_info, sizeof(void*) * *count);

	return;
}

void tiler_alloc_packed_nv12(int32_t *count, uint32_t width, uint32_t height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, int32_t aligned)
{
	int ret = -1;
	iov_t til_iov[5];
	struct tiler_allocpnv12_info info = {0};
	void *y_sysptr_info[*count];
	void *uv_sysptr_info[*count];
	void *y_allocptr_info[*count];
	void *uv_allocptr_info[*count];

	info.count = *count;
	info.width = width;
	info.height = height;
	info.aligned = aligned;

	memcpy(y_sysptr_info, y_sysptr, sizeof(void*) * *count);
	memcpy(uv_sysptr_info, uv_sysptr, sizeof(void*) * *count);
	memcpy(y_allocptr_info, y_allocptr, sizeof(void*) * *count);
	memcpy(uv_allocptr_info, uv_allocptr, sizeof(void*) * *count);

	SETIOV(&til_iov[0], &info, sizeof(struct tiler_allocpnv12_info));
	SETIOV(&til_iov[1], y_sysptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[2], uv_sysptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[3], y_allocptr_info, sizeof(void*) * *count);
	SETIOV(&til_iov[4], uv_allocptr_info, sizeof(void*) * *count);
	ret = -devctlv(fd, TILIOC_USRGPNV12, 5, 5, til_iov, til_iov, NULL);

	*count = info.count;
	memcpy(y_sysptr, y_sysptr_info, sizeof(void*) * *count);
	memcpy(uv_sysptr, uv_sysptr_info, sizeof(void*) * *count);
	memcpy(y_allocptr, y_allocptr_info, sizeof(void*) * *count);
	memcpy(uv_allocptr, uv_allocptr_info, sizeof(void*) * *count);

	return;
}

uint32_t tiler_reorient_addr(uint32_t tsptr, struct tiler_view_orient orient)
{
	int ret = -1;
	struct tiler_reorient_info info;

	info.tsptr = tsptr;
	info.orient = orient;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRROA, (unsigned long)(&info));

    return ret;
}

uint32_t tiler_get_natural_addr(void *sys_ptr)
{
	int ret = -1;
	uint32_t addr = (uint32_t)sys_ptr;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRGNA, &addr);

    return ret;
}

uint32_t tiler_reorient_topleft(uint32_t tsptr, struct tiler_view_orient orient,
				uint32_t width, uint32_t height)
{
	int ret = 1;
	struct tiler_reorient_tl_info info;

	info.tsptr = tsptr;
	info.orient = orient;
	info.width = width;
	info.height = height;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRROTL, (unsigned long)(&info));

    return ret;
}

uint32_t tiler_stride(uint32_t tsptr)
{
	int ret = 1;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRSTRIDE, &tsptr);

    return ret;
}

void tiler_rotate_view(struct tiler_view_orient *orient, uint32_t rotation)
{
	int ret = -1;
	struct tiler_rotate_view_info info;
	info.orient = *orient;
	info.rotation = rotation;

	/* make ioctl call to Tiler resmgr. */
    ret = ioctl(fd, TILIOC_USRRV, &info);

    *orient = info.orient;

    return;
}

int tiler_reg_notifier(char *name, uint32_t cmd)
{
	int ret = -1;
	iov_t til_iov[2];
	struct tiler_regnotify_info info;
	info.cmd = cmd;
	info.name = name;
	info.name_len = strlen (name);

	SETIOV(&til_iov[0], &info, sizeof(struct tiler_regnotify_info));
	SETIOV(&til_iov[1], name, sizeof(char) * info.name_len);
	ret = -devctlv(fd, TILIOC_REGNOTIFY, 2, 1, til_iov, til_iov, NULL);

	return ret;
}

int tiler_unreg_notifier(char *name, uint32_t cmd)
{
	int ret = -1;
	iov_t til_iov[2];
	struct tiler_regnotify_info info;
	info.cmd = cmd;
	info.name = name;
	info.name_len = strlen (name);

	SETIOV(&til_iov[0], &info, sizeof(struct tiler_regnotify_info));
	SETIOV(&til_iov[1], name, sizeof(char) * info.name_len);
	ret = -devctlv(fd, TILIOC_UNREGNOTIFY, 2, 1, til_iov, til_iov, NULL);

	return ret;
}
