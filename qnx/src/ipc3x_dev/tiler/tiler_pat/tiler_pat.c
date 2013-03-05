/*
 * Copyright (c) 2012, Texas Instruments Incorporated
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
 * tiler_pat.c
 *
 * TILER PAT restore support functions for TI OMAP processors.
 */
#include "proto.h"

#include "tiler.h"
#include "dmm.h"

#ifdef TILER_PLATFORM_OMAP5
#define NUM_CONTAINERS 2
#else
#define NUM_CONTAINERS 1
#endif

static struct dmm *dmm = NULL;
static u32 *tiler_restore_shm = MAP_FAILED;
static u32 *tiler_restore_va = MAP_FAILED;
static u32 tiler_restore_pa = NULL;
static int tiler_fd = -1;
static int init_count = 0;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;


/* Function to initialize the PAT entry update/restore mechanism */

int tiler_pat_init()
{
	s32 ret = 0;
	u32 len = 0;
	s64 offset = 0;

	pthread_mutex_lock(&mtx);
	if (init_count++ == 0) {
		if ((tiler_fd = shm_open("/tiler_pat_info", O_RDWR | O_CREAT, 0600)) == -1) {
			fprintf(stderr, "shm_open : %s", strerror(errno));
			ret = -errno;
			goto error;
		}

		/* Set the memory object's size */
		if (ftruncate(tiler_fd,
				TILER_WIDTH*TILER_HEIGHT*NUM_CONTAINERS*sizeof(*tiler_restore_shm)) == -1) {
			fprintf(stderr, "ftruncate : %s\n", strerror(errno));
			ret = -errno;
			goto error;
		}

		tiler_restore_shm = mmap64(NULL,
			TILER_WIDTH*TILER_HEIGHT*NUM_CONTAINERS*sizeof(*tiler_restore_shm),
			PROT_NOCACHE | PROT_READ | PROT_WRITE,
			MAP_SHARED, tiler_fd, 0);
		if (tiler_restore_shm == MAP_FAILED) {
			perror("tiler_pat_init() tiler_restore_shm failed\n");
			ret = -ENOMEM;
			goto error;
		}

		tiler_restore_va = mmap64(NULL, TILER_WIDTH * TILER_HEIGHT *
			NUM_CONTAINERS * sizeof(*tiler_restore_va),
			PROT_NOCACHE | PROT_READ | PROT_WRITE,
			MAP_ANON | MAP_PHYS | MAP_PRIVATE, NOFD, 0);
		if (tiler_restore_va == MAP_FAILED) {
			perror("tiler_pat_init() tiler_restore_va failed\n");
			ret = -ENOMEM;
			goto error;
		}
		ret = mem_offset64(tiler_restore_va, NOFD,
				TILER_WIDTH * TILER_HEIGHT * NUM_CONTAINERS * sizeof(*tiler_restore_va),
				&offset, &len);
		if (ret) {
			perror("tiler_pat_init() mem_offset failed\n");
			ret = -ENOMEM;
			goto error;
		}
		else {
			tiler_restore_pa = (u32)offset;
		}

	    dmm = dmm_pat_init(0);
	    if (dmm == NULL) {
			ret = -ENOMEM;
			goto error;
	    }
	}
	pthread_mutex_unlock(&mtx);
	return 0;

error:
	init_count--;
	if (tiler_restore_va != MAP_FAILED) {
		munmap(tiler_restore_va, TILER_WIDTH * TILER_HEIGHT *
				NUM_CONTAINERS * sizeof(*tiler_restore_va));
		tiler_restore_va = MAP_FAILED;
	}
	if (tiler_restore_shm != MAP_FAILED) {
		munmap(tiler_restore_shm, TILER_WIDTH * TILER_HEIGHT *
				NUM_CONTAINERS * sizeof(*tiler_restore_shm));
		tiler_restore_shm = MAP_FAILED;
	}
	if (tiler_fd != -1) {
		close(tiler_fd);
		tiler_fd = -1;
	}
	pthread_mutex_unlock(&mtx);
	return ret;
}

/* Function to de-initialize the PAT entry update/restore mechanism */

int tiler_pat_deinit()
{
	pthread_mutex_lock(&mtx);
	if (init_count <= 0) {
		goto exit;
	}
	else if (--init_count == 0) {
		if (dmm) {
			dmm_pat_release(dmm);
			dmm = NULL;
		}
		if (tiler_restore_va) {
			munmap(tiler_restore_va, TILER_WIDTH * TILER_HEIGHT *
						NUM_CONTAINERS * sizeof(*tiler_restore_va));
			tiler_restore_va = MAP_FAILED;
			tiler_restore_pa = NULL;
		}
		if (tiler_restore_shm) {
			munmap(tiler_restore_shm, TILER_WIDTH * TILER_HEIGHT *
					NUM_CONTAINERS * sizeof(*tiler_restore_shm));
			tiler_restore_shm = MAP_FAILED;
		}
		if (tiler_fd != -1) {
			close(tiler_fd);
			tiler_fd = -1;
		}
	}

exit:
	pthread_mutex_unlock(&mtx);
	return 0;
}

/* Function to save the PAT entry updates */

int tiler_save_pat(struct pat_area *area, u32 *pages)
{
	unsigned int i, j;
	int entries = 0;

	if (tiler_restore_shm == MAP_FAILED) {
        return -ENOMEM;
	}
	entries = (area->x1 - area->x0 + 1);
	for (i = area->y0, j = 0; i < area->y1 + 1; i++, j++) {
		memcpy(&tiler_restore_shm[i*TILER_WIDTH + area->x0],
				&pages[j*entries], entries * sizeof(u32));
	}
	return 0;
}

/* Function to restore the PAT entries after Core OFF*/

int tiler_restore_pat()
{
	struct pat pat_desc = {0};
	struct pat_area area;

	if (!tiler_restore_pa) {
        return -ENOMEM;
	}

	/* set up area */
	area.x0 = 0;
	area.y0 = 0;
	area.x1 = TILER_WIDTH - 1;
	area.y1 = (TILER_HEIGHT * NUM_CONTAINERS) - 1;

	/* send pat descriptor to dmm driver */
	pat_desc.ctrl.dir = 0;
	pat_desc.ctrl.ini = 0;
	pat_desc.ctrl.lut_id = 0;
	pat_desc.ctrl.start = 1;
	pat_desc.ctrl.sync = 0;
	pat_desc.area = area;
	pat_desc.next = NULL;

	/* must be a 16-byte aligned physical address */
	memcpy(tiler_restore_va, tiler_restore_shm,
			TILER_WIDTH*TILER_HEIGHT*NUM_CONTAINERS*sizeof(*tiler_restore_shm));
	pat_desc.data = tiler_restore_pa;
	return dmm_pat_refill(dmm, &pat_desc, MANUAL);
}
