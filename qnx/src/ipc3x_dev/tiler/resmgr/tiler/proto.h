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

#ifndef _PROTO_H_INCLUDED
#define _PROTO_H_INCLUDED

struct _iofunc_attr;
#define RESMGR_HANDLE_T struct _iofunc_attr
struct tiler_ocb;
#define IOFUNC_OCB_T struct tiler_ocb
#define RESMGR_OCB_T struct tiler_ocb
#define THREAD_POOL_PARAM_T dispatch_context_t

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

#include "tiler/tiler_devctl.h"
#include "List.h"

void *
mmap64_peer(pid_t pid, void *addr, size_t len, int prot, int flags, int fd, off64_t off);
void *
mmap_peer(pid_t pid, void *addr, size_t len, int prot, int flags, int fd, off_t off);
int
munmap_peer(pid_t pid, void *addr, size_t len);
int
mem_offset64_peer(pid_t pid, const uintptr_t addr, size_t len,
				off64_t *offset, size_t *contig_len);

typedef struct tiler_ocb {
    iofunc_ocb_t        hdr;
    void               *pi;
} tiler_ocb_t;

typedef struct tiler_dev {
    iofunc_attr_t       hdr;
    dispatch_t          *dpp;
    dispatch_context_t  *ctp;
    int                 id;
    iofunc_notify_t     notify[3];
    void                *hdl;
	//struct blocking_notifier_head notifier;
} tiler_dev_t;

s32 tiler_init(u32 size);
void tiler_exit(void);
int tiler_islowmem(void);
void tiler_purge(void);
tiler_ocb_t * ocb_calloc (resmgr_context_t * ctp, IOFUNC_ATTR_T * device);
void ocb_free (tiler_ocb_t * ocb);

/* Over-ride alloc/free implementation */
#define kmalloc(a,b) malloc(a)
#define kzalloc(a,b) calloc(1,a)
#define kfree(a) free(a)

/* Over-ride mutex implementation */
#define mutex_init(x) pthread_mutex_init(x, NULL)
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock
#define mutex_destroy pthread_mutex_destroy

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define ALIGN(x, a) (((x) + ((typeof(x))(a) - 1)) & ~((typeof(x))(a) - 1))
#define PAGE_ALIGN(a) (((a)+((PAGE_SIZE)-1))&(~((PAGE_SIZE)-1)))

/* Over-ride list implementation */
struct list_head {
	struct list_head *prev;
	struct list_head *next;
};
#define list_add_tail(x, head) List_enqueue((List_Handle)head, (List_Elem *)x)
#define list_add(x, head) List_enqueueHead((List_Handle)head, (List_Elem *)x)
#define list_for_each_safe(a,b,c) List_traverse_safe(a,b,c)
#define list_for_each_entry_safe(a,b,c,d) List_traverse_elem(a,b,c,d)
#define list_entry(x,y,z) List_elem(x,y,z)
#define list_del(x) List_remove(NULL,(List_Elem *)x)
#define list_is_singular(x) List_is_singular((List_Handle)x)
#define INIT_LIST_HEAD(x) List_elemClear((List_Elem *)x)
#define LIST_HEAD(x) struct list_head x = { &(x), &(x) }
#define list_empty(x) List_empty((List_Handle)x)
#define list_empty_careful(x) List_empty((List_Handle)x)
#define list_first_entry(ptr, type, member) List_elem((ptr)->next, type, member)
#define list_move(x,y) List_move((List_Handle)y,(List_Elem *)x)

/* Over-ride register read/write implementation */
#define __raw_readl(x) *((volatile u32 *) (x))
#define __raw_writel(x, y) *((volatile u32 *)(y))=x

/* Over-ride mem barrier implementation */
#define wmb __cpu_membarrier

#endif
