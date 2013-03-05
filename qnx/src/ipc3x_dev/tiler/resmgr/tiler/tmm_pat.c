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
 * tmm_pat.c
 *
 * DMM driver support functions for TI OMAP processors.
 */
#include "proto.h"
#include "tmm.h"
#include "tiler.h"
#include <dlfcn.h>

/**
 * Number of pages to allocate when
 * refilling the free page stack.
 */
#define TILER_GROW_SIZE 16
#define DMM_PAGE 0x1000
static unsigned long grow_size = TILER_GROW_SIZE;

/* Max pages in free page stack */
#define PAGE_CAP (256 * 128)

/* Number of pages currently allocated */
static unsigned long count;

struct dmm *(*dmm_pat_init_sym) (u32);
void (*dmm_pat_release_sym)(struct dmm *);
s32 (*dmm_pat_refill_sym)(struct dmm *, struct pat *, enum pat_mode);
extern void * tiler_pat_lib;

/**
  * Used to keep track of mem per
  * dmm_get_pages call.
  */
struct fast {
	struct list_head list;
	struct mem **mem;
	u32 *pa;
	u32 num;
};

/**
 * Used to keep track of the page struct ptrs
 * and physical addresses of each page.
 */
struct mem {
	struct list_head list;
	//struct page *pg;
	void *pg;
	u32 pa;
};

/**
 * TMM PAT private structure
 */
struct dmm_mem {
	struct fast fast_list;
	struct mem free_list;
	struct mem used_list;
	//struct mutex mtx;
	pthread_mutex_t mtx;
	struct dmm *dmm;
};

static void dmm_free_fast_list(struct fast *fast)
{
	struct list_head *pos = NULL, *q = NULL;
	struct fast *f = NULL;
	s32 i = 0;

	/* mutex is locked */
	list_for_each_safe(pos, q, &fast->list) {
		f = list_entry(pos, struct fast, list);
		for (i = 0; i < f->num; i++) {
			munmap(f->mem[i]->pg, PAGE_SIZE);
			kfree(f->mem[i]);
		}
		kfree(f->pa);
		kfree(f->mem);
		list_del(pos);
		kfree(f);
	}
}

static u32 fill_page_stack(struct mem *mem, pthread_mutex_t *mtx, int npages)
{
	s32 i = 0;
	struct mem *m = NULL;
	s32 ret = 0;
	u32 len = 0;
	s64 offset = 0;
	void *chunk_va = NULL;
	u32 chunk_pa = 0;
	u32 page_size = PAGE_SIZE;
	u32 bytes = npages * page_size;

	chunk_va = mmap64(NULL, bytes, PROT_NOCACHE | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, NOFD, 0);
	if (chunk_va == MAP_FAILED) {
		return -ENOMEM;
	}

	do {
		ret = mem_offset64(chunk_va, NOFD, bytes, &offset, &len);
		if (ret) {
			fprintf(stderr, "Tiler: Error from mem_offset [%d]\n", errno);
			return -ENOMEM;
		}
		else {
			chunk_pa = (u32)offset;
		}
		bytes -= len;

		for (i = 0; i < (len/page_size); i++) {
			m = kmalloc(sizeof(*m), GFP_KERNEL);
			if (!m)
				return -ENOMEM;
			memset(m, 0x0, sizeof(*m));

			m->pg = chunk_va;
			m->pa = chunk_pa;

			mutex_lock(mtx);
			count++;
			list_add(&m->list, &mem->list);
			mutex_unlock(mtx);

			chunk_va += page_size;
			chunk_pa += page_size;
		}
	} while(bytes > 0);

	return 0x0;
}

static void dmm_free_page_stack(struct mem *mem)
{
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;

	/* mutex is locked */
	list_for_each_safe(pos, q, &mem->list) {
		m = list_entry(pos, struct mem, list);
		munmap(m->pg, PAGE_SIZE);
		list_del(pos);
		kfree(m);
		count--;
	}
}

static void tmm_pat_deinit(struct tmm *tmm)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;

	mutex_lock(&pvt->mtx);
	dmm_free_fast_list(&pvt->fast_list);
	dmm_free_page_stack(&pvt->free_list);
	dmm_free_page_stack(&pvt->used_list);
	mutex_destroy(&pvt->mtx);
}

/* free up memory that is currently only on the free lists */
static void tmm_pat_purge(struct tmm *tmm)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;

	mutex_lock(&pvt->mtx);

	dmm_free_page_stack(&pvt->free_list);

	mutex_unlock(&pvt->mtx);
}

static u32 *tmm_pat_get_pages(struct tmm *tmm, s32 n)
{
	s32 i = 0;
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;
	struct fast *f = NULL;
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;

	if (n <= 0 || n > 0x8000)
		return NULL;

	if (list_empty_careful(&pvt->free_list.list))
		if (fill_page_stack(&pvt->free_list, &pvt->mtx, n))
			return NULL;

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return NULL;
	memset(f, 0x0, sizeof(*f));

	/* array of mem struct pointers */
	f->mem = kmalloc(n * sizeof(*f->mem), GFP_KERNEL);
	if (!f->mem) {
		kfree(f); return NULL;
	}
	memset(f->mem, 0x0, n * sizeof(*f->mem));

	/* array of physical addresses */
	f->pa = kmalloc(n * sizeof(*f->pa), GFP_KERNEL);
	if (!f->pa) {
		kfree(f->mem); kfree(f); return NULL;
	}
	memset(f->pa, 0x0, n * sizeof(*f->pa));

	/*
	 * store the number of mem structs so that we
	 * know how many to free later.
	 */
	f->num = n;

	for (i = 0; i < n; i++) {
		if (list_empty_careful(&pvt->free_list.list))
			if (fill_page_stack(&pvt->free_list, &pvt->mtx, (n - i)))
				goto cleanup;

		mutex_lock(&pvt->mtx);
		pos = NULL;
		q = NULL;
		m = NULL;

		/*
		 * remove one mem struct from the free list and
		 * add the address to the fast struct mem array
		 */
		list_for_each_safe(pos, q, &pvt->free_list.list) {
			m = list_entry(pos, struct mem, list);
			list_del(pos);
			break;
		}
		mutex_unlock(&pvt->mtx);

		if (m != NULL) {
			f->mem[i] = m;
			f->pa[i] = m->pa;
		}
		else {
			goto cleanup;
		}
	}

	mutex_lock(&pvt->mtx);
	list_add(&f->list, &pvt->fast_list.list);
	mutex_unlock(&pvt->mtx);

	if (f != NULL)
		return f->pa;
cleanup:
	for (; i > 0; i--) {
		mutex_lock(&pvt->mtx);
		list_add(&f->mem[i - 1]->list, &pvt->free_list.list);
		mutex_unlock(&pvt->mtx);
	}
	kfree(f->pa);
	kfree(f->mem);
	kfree(f);
	return NULL;
}

static void tmm_pat_free_pages(struct tmm *tmm, u32 *list)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;
	struct list_head *pos = NULL, *q = NULL;
	struct fast *f = NULL;
	s32 i = 0;

	mutex_lock(&pvt->mtx);
	pos = NULL;
	q = NULL;
	list_for_each_safe(pos, q, &pvt->fast_list.list) {
		f = list_entry(pos, struct fast, list);
		if (f->pa[0] == list[0]) {
			for (i = 0; i < f->num; i++) {
				if (count < PAGE_CAP && !tiler_islowmem()) {
					memset(((struct mem *)f->mem[i])->pg, 0x0, PAGE_SIZE);
					list_add(
					&((struct mem *)f->mem[i])->list,
					&pvt->free_list.list);
				} else {
					munmap(
						((struct mem *)f->mem[i])->pg, PAGE_SIZE);
					kfree(f->mem[i]);
					count--;
				}
			}
			list_del(pos);
			kfree(f->pa);
			kfree(f->mem);
			kfree(f);
			break;
		}
	}
	mutex_unlock(&pvt->mtx);
}

static s32 tmm_pat_map(struct tmm *tmm, struct pat_area area, u32 page_pa)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;
	struct pat pat_desc = {0};

	/* send pat descriptor to dmm driver */
	pat_desc.ctrl.dir = 0;
	pat_desc.ctrl.ini = 0;
	pat_desc.ctrl.lut_id = 0;
	pat_desc.ctrl.start = 1;
	pat_desc.ctrl.sync = 0;
	pat_desc.area = area;
	pat_desc.next = NULL;

	/* must be a 16-byte aligned physical address */
	pat_desc.data = page_pa;
	return (*dmm_pat_refill_sym)(pvt->dmm, &pat_desc, MANUAL);
}

struct tmm *tmm_pat_init(u32 pat_id, u32 size)
{
	struct tmm *tmm = NULL;
	struct dmm_mem *pvt = NULL;
	struct dmm *dmm = NULL;

	dmm_pat_init_sym = dlsym(tiler_pat_lib, "dmm_pat_init");
	if (dmm_pat_init_sym == NULL) {
		fprintf(stderr, "tmm_pat_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}

	dmm_pat_release_sym = dlsym(tiler_pat_lib, "dmm_pat_release");
	if (dmm_pat_release_sym == NULL) {
		fprintf(stderr, "tmm_pat_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}

	dmm_pat_refill_sym = dlsym(tiler_pat_lib, "dmm_pat_refill");
	if (dmm_pat_refill_sym == NULL) {
		fprintf(stderr, "tmm_pat_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}

	dmm = (*dmm_pat_init_sym)(pat_id);
	if (dmm)
		tmm = kmalloc(sizeof(*tmm), GFP_KERNEL);
	if (tmm)
		pvt = kmalloc(sizeof(*pvt), GFP_KERNEL);
	if (pvt) {
		/* private data */
		pvt->dmm = dmm;
		INIT_LIST_HEAD(&pvt->free_list.list);
		INIT_LIST_HEAD(&pvt->used_list.list);
		INIT_LIST_HEAD(&pvt->fast_list.list);
		mutex_init(&pvt->mtx);

		if (size)
			grow_size = size;

		fprintf(stderr, "configured grow size is %d\n", (int)grow_size);

		count = 0;
		if (list_empty_careful(&pvt->free_list.list))
			if (fill_page_stack(&pvt->free_list, &pvt->mtx, grow_size))
				goto error;

		/* public data */
		tmm->pvt = pvt;
		tmm->deinit = tmm_pat_deinit;
		tmm->purge = tmm_pat_purge;
		tmm->get = tmm_pat_get_pages;
		tmm->free = tmm_pat_free_pages;
		tmm->map = tmm_pat_map;

		return tmm;
	}

error:
	kfree(pvt);
	kfree(tmm);
	if (dmm_pat_release_sym)
		(*dmm_pat_release_sym)(dmm);
	return NULL;
}

