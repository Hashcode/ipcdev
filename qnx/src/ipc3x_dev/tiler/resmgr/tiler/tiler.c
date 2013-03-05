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
 * tiler.c
 *
 * TILER driver support functions for TI OMAP processors.
 */
#include "proto.h"
#include "List.h"

#include "tiler.h"
#include "dmm.h"
#include "tmm.h"
#include "tiler_def.h"
#include "tcm/tcm_sita.h"	/* Algo Specific header */
#include <dlfcn.h>
#include <arm/mmu.h>


// TODO: replace with real bug checking
#define BUG_ON

/* per process (thread group) info */
struct process_info {
	struct list_head list;		/* other processes */
	struct list_head groups;	/* my groups */
	struct list_head bufs;		/* my registered buffers */
	pid_t pid;			/* really: thread group ID */
	u32 refs;			/* open tiler devices, 0 for processes
					   tracked via kernel APIs */
	bool kernel;			/* tracking kernel objects */
};

/* per group info (within a process) */
struct gid_info {
	struct list_head by_pid;	/* other groups */
	struct list_head areas;		/* all areas in this pid/gid */
	struct list_head reserved;	/* areas pre-reserved */
	struct list_head onedim;	/* all 1D areas in this pid/gid */
	u32 gid;			/* group ID */
	struct process_info *pi;	/* parent */
};

static struct list_head blocks;
static struct list_head procs;
static struct list_head orphan_areas;
static struct list_head orphan_onedim;

struct area_info {
	struct list_head by_gid;	/* areas in this pid/gid */
	struct list_head blocks;	/* blocks in this area */
	u32 nblocks;			/* # of blocks in this area */

	struct tcm_area area;		/* area details */
	struct gid_info *gi;		/* link to parent, if still alive */
};

struct til_mem_info {
	struct list_head global;	/* reserved / global blocks */
	u32 sys_addr;          /* system space (L3) tiler addr */
	u32 num_pg;            /* number of pages in page-list */
	u32 usr;               /* user space address */
	u32 *pg_ptr;           /* list of mapped struct page pointers */
	struct tcm_area area;
	u32 *mem;              /* list of alloced phys addresses */
	u32 refs;              /* number of times referenced */
	bool alloced;			/* still alloced */

	struct list_head by_area;	/* blocks in the same area / 1D */
	void *parent;			/* area info for 2D, else group info */
};

struct __buf_info {
	struct list_head by_pid;		/* list of buffers per pid */
	struct tiler_buf_info buf_info;
	struct til_mem_info *mi[TILER_MAX_NUM_BLOCKS];	/* blocks */
};

struct reg_notif_info {
	struct list_head elem;		/* list elem */
	tiler_notifier_cb cb_ptr;	/* Callback fxn pointer. */
	void *arg;					/* Usr private info */
};

struct list_head reg_notif;

struct reg_notif_usr_info {
	struct list_head elem;	/* list elem */
	int cmd;				/* Command to send to user. */
	char *name;				/* Name of user */
	pid_t pid;              /* pid */
	int name_len;           /* Length of Name of User */
};

struct list_head reg_notif_usr;

#define TILER_FORMATS 4

#define TILER_REGNOTIFY_MAX_NAME_LEN 16

#define TILER_REGNOTIFY_MAX_REGISTERED 3

void *tiler_pat_lib = NULL;
int (*tiler_pat_init)(void);
int (*tiler_pat_deinit)(void);
int (*tiler_save_pat)(struct pat_area *, u32 *);

#define MAX_CONTAINERS 2

static u32 id;
static pthread_mutex_t mtx;
static struct tcm *tcm[TILER_FORMATS];
static struct tmm *tmm[TILER_FORMATS];
static u32 *dmac_va;
static u32 dmac_pa;

static u32 *dummy_va;
static u32 dummy_pa;

#define TCM(fmt)        tcm[(fmt) - TILFMT_8BIT]
#define TCM_SS(ssptr)   TCM(TILER_GET_ACC_MODE(ssptr))
#define TCM_SET(fmt, i) tcm[(fmt) - TILFMT_8BIT] = i
#define TMM(fmt)        tmm[(fmt) - TILFMT_8BIT]
#define TMM_SS(ssptr)   TMM(TILER_GET_ACC_MODE(ssptr))
#define TMM_SET(fmt, i) tmm[(fmt) - TILFMT_8BIT] = i

#ifdef TILER_PLATFORM_OMAP5
char tiler_alloc_debug_buffer[34842];
#else
char tiler_alloc_debug_buffer[17421];
#endif
char *tiler_alloc_debug_buffer_ptr;

static void fill_map(u16 **map, int div, struct tcm_area *a, u8 c, bool ovw,
			u8 col)
{
	u16 val = c | ((u16) col << 8);
	int x, y;
	for (y = a->p0.y; y <= a->p1.y; y++)
		for (x = a->p0.x / div; x <= a->p1.x / div; x++)
			if (map[y][x] == ' ' || ovw)
				map[y][x] = val;
}

static void fill_map_pt(u16 **map, int div, struct tcm_pt *p, u8 c)
{
	map[p->y][p->x / div] = (map[p->y][p->x / div] & 0xff00) | c;
}

static u8 read_map_pt(u16 **map, int div, struct tcm_pt *p)
{
	return map[p->y][p->x / div] & 0xff;
}

static int map_width(int div, int x0, int x1)
{
	return (x1 / div) - (x0 / div) + 1;
}

static void text_map(u16 **map, int div, char *nice, int y, int x0, int x1,
								u8 col)
{
	u16 *p = map[y] + (x0 / div);
	int w = (map_width(div, x0, x1) - strlen(nice)) / 2;
	if (w >= 0) {
		p += w;
		while (*nice)
			*p++ = ((u16) col << 8) | (u8) *nice++;
	}
}

static void map_1d_info(u16 **map, int div, char *nice, struct tcm_area *a,
						u8 col)
{
	sprintf(nice, "%dK", tcm_sizeof(*a) * 4);
	if (a->p0.y + 1 < a->p1.y) {
		text_map(map, div, nice, (a->p0.y + a->p1.y) / 2, 0,
							TILER_WIDTH - 1, col);
	} else if (a->p0.y < a->p1.y) {
		if (strlen(nice) < map_width(div, a->p0.x, TILER_WIDTH - 1))
			text_map(map, div, nice, a->p0.y, a->p0.x + div,
							TILER_WIDTH - 1, col);
		else if (strlen(nice) < map_width(div, 0, a->p1.x))
			text_map(map, div, nice, a->p1.y, 0, a->p1.y - div,
									col);
	} else if (strlen(nice) + 1 < map_width(div, a->p0.x, a->p1.x)) {
		text_map(map, div, nice, a->p0.y, a->p0.x, a->p1.x, col);
	}
}

static void map_2d_info(u16 **map, int div, char *nice, struct til_mem_info *mi,
							u8 col)
{
	struct tcm_area *a = &mi->area;
	int y = (a->p0.y + a->p1.y) / 2;
	sprintf(nice, "(%d*%d)", tcm_awidth(*a), tcm_aheight(*a));
	if (strlen(nice) + 1 < map_width(div, a->p0.x, a->p1.x))
		text_map(map, div, nice, y, a->p0.x, a->p1.x, col);

	sprintf(nice, "<%s%d>", mi->alloced ? "a" : "", mi->refs);
	if (a->p1.y > a->p0.y + 1 &&
	    strlen(nice) + 1 < map_width(div, a->p0.x, a->p1.x))
		text_map(map, div, nice, y + 1, a->p0.x, a->p1.x, col);
}

static void write_out(u16 **map, char *fmt, int y, bool color, char *nice)
{
	u8 current_col = '\x0f';
	char *o = nice;
	u16 *d = map[y];

	/* boundary */
	if (color)
		o += sprintf(o, "\e[0;%d%sm", (y & 8) ? 34 : 36,
						(y & 16) ? ";1" : "");
	o += sprintf(o, fmt, y);
	o += sprintf(o, "%s:", color ? "\e[0;1m" : "");

	/* text */
	do {
		u16 p = *d ?: (':' | 0x0f00);
		u8 col_chg = current_col ^ (p >> 8);
		if (col_chg && color) {
			o += sprintf(o, "\e[");
			if ((col_chg & 0x88) && (p & 0x0800) == 0) {
				o += sprintf(o, "0;");
				col_chg = 0x07 ^ (p >> 8);
			}
			if (col_chg & 0x7)
				o += sprintf(o, "%d;", 30 + ((p >> 8) & 0x07));
			if (col_chg & 0x70)
				o += sprintf(o, "%d;", 40 + ((p >> 12) & 0x07));
			if (p & 0x0800)
				o += sprintf(o, "1;");
			o[-1] = 'm';
		}
		*o++ = p & 0xff;
		current_col = p >> 8;
	} while (*d++);

	if (color && current_col != 0x07)
		o += sprintf(o, "\e[0m");
	*o = 0;
	tiler_alloc_debug_buffer_ptr += sprintf(tiler_alloc_debug_buffer_ptr, "%s\n", nice);
}

static void print_allocation_map(bool color)
{
	int div = 2;
	int i, j;
	u16 **map, *global_map;
	struct area_info *ai;
	struct til_mem_info *mi, *tmp;
	struct tcm_area a, p;
	static char *m2d = "abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static char *a2d = ".,:;'\"`~!^-+";
	static char *m1d_c = "\x47\x57\x67\xc7\xd7\xe7";
	static char *m2d_c = "\x17\x27\x37\x97\xa7\xb7";
	static char *a2d_c = "\x01\x02\x03\x09\x0a\x0b";

	char *m2dp = m2d, *a2dp = a2d;
	char *m2dp_c = m2d_c, *m1dp_c = m1d_c, *a2dp_c = a2d_c;
	char *nice;
#ifdef TILER_PLATFORM_OMAP5
	u16 **map1d;
	char *nice1d;
#endif

	/* allocate map */
	nice = kzalloc(TILER_WIDTH / div * 16, GFP_KERNEL);
	map = kzalloc((TILER_HEIGHT + 1) * sizeof(*map), GFP_KERNEL);
#ifdef TILER_PLATFORM_OMAP5
	nice1d = kzalloc(TILER_WIDTH / div * 16, GFP_KERNEL);
	map1d = kzalloc((TILER_HEIGHT + 1) * sizeof(*map), GFP_KERNEL);

	global_map = kzalloc((TILER_WIDTH / div + 1)
			* (TILER_HEIGHT + 1) * 2 * sizeof(*global_map), GFP_KERNEL);
#else
	global_map = kzalloc((TILER_WIDTH / div + 1)
			* (TILER_HEIGHT + 1) * sizeof(*global_map), GFP_KERNEL);
#endif

#ifdef TILER_PLATFORM_OMAP5
	if (!map || !map1d || !global_map || !nice || !nice1d) {
#else
	if (!map || !global_map || !nice) {
#endif
		fprintf(stderr, "could not allocate map for debug print\n");
		goto error;
	}
	for (i = 0; i <= TILER_HEIGHT; i++) {
		map[i] = global_map + i * (TILER_WIDTH / div + 1);
		for (j = 0; j < TILER_WIDTH / div; j++)
			map[i][j] = ' ';
		map[i][j] = 0;
	}
#ifdef TILER_PLATFORM_OMAP5
	for (i = 0; i <= TILER_HEIGHT; i++) {
		map1d[i] = global_map + ((TILER_HEIGHT + 1) * (TILER_WIDTH / div + 1))
					+ i * (TILER_WIDTH / div + 1);
		for (j = 0; j < TILER_WIDTH / div; j++)
			map1d[i][j] = ' ';
		map1d[i][j] = 0;
	}
#endif

	/* get all allocations */
	mutex_lock(&mtx);

	list_for_each_entry_safe(mi, tmp, &blocks, global) {
		if (mi->area.is2d) {
			ai = mi->parent;
			fill_map(map, div, &ai->area, *a2dp, false, *a2dp_c);
			fill_map(map, div, &mi->area, *m2dp, true, *m2dp_c);
			map_2d_info(map, div, nice, mi, *m2dp_c | 0xf);
			if (!*++a2dp)
				a2dp = a2d;
			if (!*++a2dp_c)
				a2dp_c = a2d_c;
			if (!*++m2dp)
				m2dp = m2d;
			if (!*++m2dp_c)
				m2dp_c = m2d_c;
		} else {
#ifdef TILER_PLATFORM_OMAP5
			bool start = read_map_pt(map1d, div, &mi->area.p0) == ' ';
			bool end = read_map_pt(map1d, div, &mi->area.p1) == ' ';
			tcm_for_each_slice(a, mi->area, p)
				fill_map(map1d, div, &a, '=', true, *m1dp_c);
			fill_map_pt(map1d, div, &mi->area.p0, start ? '<' : 'X');
			fill_map_pt(map1d, div, &mi->area.p1, end ? '>' : 'X');
			map_1d_info(map1d, div, nice1d, &mi->area, *m1dp_c | 0xf);
			if (!*++m1dp_c)
				m1dp_c = m1d_c;
#else
			bool start = read_map_pt(map, div, &mi->area.p0) == ' ';
			bool end = read_map_pt(map, div, &mi->area.p1) == ' ';
			tcm_for_each_slice(a, mi->area, p)
				fill_map(map, div, &a, '=', true, *m1dp_c);
			fill_map_pt(map, div, &mi->area.p0, start ? '<' : 'X');
			fill_map_pt(map, div, &mi->area.p1, end ? '>' : 'X');
			map_1d_info(map, div, nice, &mi->area, *m1dp_c | 0xf);
			if (!*++m1dp_c)
				m1dp_c = m1d_c;
#endif
		}
	}

	for (i = 0; i < TILER_WIDTH / div; i++)
		map[TILER_HEIGHT][i] = ':' | 0x0f00;
#ifdef TILER_PLATFORM_OMAP5
	text_map(map, div, " BEGIN TILER 2D MAP ", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
#else
	text_map(map, div, " BEGIN TILER MAP ", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
#endif

	tiler_alloc_debug_buffer_ptr += sprintf(tiler_alloc_debug_buffer_ptr, "\n");
	write_out(map, "   ", TILER_HEIGHT, color, nice);
	for (i = 0; i < TILER_HEIGHT; i++)
		write_out(map, "%03d", i, color, nice);

#ifdef TILER_PLATFORM_OMAP5
	text_map(map, div, ": END TILER 2D MAP :", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
#else
	text_map(map, div, ": END TILER MAP :", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
#endif
	write_out(map, "   ", TILER_HEIGHT, color, nice);

#ifdef TILER_PLATFORM_OMAP5
	for (i = 0; i < TILER_WIDTH / div; i++)
		map1d[TILER_HEIGHT][i] = ':' | 0x0f00;
	text_map(map1d, div, " BEGIN TILER 1D MAP ", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
	tiler_alloc_debug_buffer_ptr += sprintf(tiler_alloc_debug_buffer_ptr, "\n");
	write_out(map1d, "   ", TILER_HEIGHT, color, nice);
	for (i = 0; i < TILER_HEIGHT; i++)
		write_out(map1d, "%03d", i, color, nice);
	text_map(map1d, div, ": END TILER 1D MAP :", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
	write_out(map1d, "   ", TILER_HEIGHT, color, nice);
#endif

	mutex_unlock(&mtx);

error:
	kfree(map);
#ifdef TILER_PLATFORM_OMAP5
	kfree(map1d);
#endif
	kfree(global_map);
	kfree(nice);
#ifdef TILER_PLATFORM_OMAP5
	kfree(nice1d);
#endif
}

static uint tiler_alloc_debug;
int tiler_read(resmgr_context_t *ctp, io_read_t *msg, tiler_ocb_t *ocb)
{
	int nbytes;
	int nparts;
	int status;
	int nleft;

	if ((status = iofunc_read_verify (ctp, msg, &ocb->hdr, NULL)) != EOK)
		return (status);

	if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
		return (ENOSYS);

	if (ocb->hdr.offset > sizeof(tiler_alloc_debug_buffer))
		return (EINVAL);

	nleft = sizeof(tiler_alloc_debug_buffer) - ocb->hdr.offset;
	nbytes = min (msg->i.nbytes, nleft);

	/* Make sure the user has supplied a big enough buffer */
	if (nbytes > 0) {
		if (nleft == sizeof(tiler_alloc_debug_buffer)) {
			/* User hasn't tried to read yet. Refill the buffer with the latest info. */
			tiler_alloc_debug_buffer_ptr = tiler_alloc_debug_buffer;
			print_allocation_map(tiler_alloc_debug & 4);
		}

		/* set up the return data IOV */
		SETIOV (ctp->iov, tiler_alloc_debug_buffer + ocb->hdr.offset, nbytes);

		/* set up the number of bytes (returned by client's read()) */
		_IO_SET_READ_NBYTES (ctp, nbytes);

		ocb->hdr.offset += nbytes;

		nparts = 1;
	}
	else {
		_IO_SET_READ_NBYTES (ctp, 0);

		nparts = 0;
	}

	/* mark the access time as invalid (we just accessed it) */

	if (msg->i.nbytes > 0)
		ocb->hdr.attr->flags |= IOFUNC_ATTR_ATIME;

	return (_RESMGR_NPARTS (nparts));
}

/* get process info, and increment refs for device tracking */
static struct process_info *__get_pi(pid_t pid, bool kernel)
{
	struct process_info *pi, *tmp;

	/* find process context */
	mutex_lock(&mtx);
	list_for_each_entry_safe(pi, tmp, &procs, list) {
		if (pi->pid == pid && pi->kernel == kernel)
			goto done;
	}

	/* create process context */
	pi = kmalloc(sizeof(*pi), GFP_KERNEL);
	if (!pi)
		goto done;

	memset(pi, 0, sizeof(*pi));
	pi->pid = pid;
	pi->kernel = kernel;
	INIT_LIST_HEAD(&pi->groups);
	INIT_LIST_HEAD(&pi->bufs);
	list_add(&pi->list, &procs);
done:
	if (pi && !kernel)
		pi->refs++;
	mutex_unlock(&mtx);
	return pi;
}

/* allocate an reserved area of size, alignment and link it to gi */
static struct area_info *area_new(u16 width, u16 height, u16 align,
				  struct tcm *tcm, struct gid_info *gi)
{
	struct area_info *ai = kmalloc(sizeof(*ai), GFP_KERNEL);
	if (!ai)
		return NULL;

	/* set up empty area info */
	memset(ai, 0x0, sizeof(*ai));
	INIT_LIST_HEAD(&ai->blocks);

	/* reserve an allocation area */
	if (tcm_reserve_2d(tcm, width, height, align, &ai->area)) {
		kfree(ai);
		return NULL;
	}

	ai->gi = gi;
	mutex_lock(&mtx);
	list_add_tail(&ai->by_gid, &gi->areas);
	mutex_unlock(&mtx);
	return ai;
}

/* (must have mutex) free an area and return NULL */
static inline void _m_area_free(struct area_info *ai)
{
	if (ai) {
		list_del(&ai->by_gid);
		kfree(ai);
	}
}

static s32 __analyze_area(enum tiler_fmt fmt, u32 width, u32 height,
			  u16 *x_area, u16 *y_area, u16 *band,
			  u16 *align, u16 *offs)
{
	/* input: width, height is in pixels, align, offs in bytes */
	/* output: x_area, y_area, band, align, offs in slots */

	/* slot width, height, and row size */
	u32 slot_w, slot_h, slot_row, bpp;

	/* align must be 2 power */
	if (*align & (*align - 1))
		return -1;

	/* width and height must be greater than 0 */
	if (!width || !height)
		return -1;

	switch (fmt) {
	case TILFMT_8BIT:
		slot_w = DMM_PAGE_DIMM_X_MODE_8;
		slot_h = DMM_PAGE_DIMM_Y_MODE_8;
		break;
	case TILFMT_16BIT:
		slot_w = DMM_PAGE_DIMM_X_MODE_16;
		slot_h = DMM_PAGE_DIMM_Y_MODE_16;
		break;
	case TILFMT_32BIT:
		slot_w = DMM_PAGE_DIMM_X_MODE_32;
		slot_h = DMM_PAGE_DIMM_Y_MODE_32;
		break;
	case TILFMT_PAGE:
		/* adjust size to accomodate offset, only do page alignment */
		*align = PAGE_SIZE;
		width += *offs & (PAGE_SIZE - 1);

		/* for 1D area keep the height (1), width is in tiler slots */
		*x_area = DIV_ROUND_UP(width, TILER_PAGE);
		*y_area = *band = 1;

		if (*x_area * *y_area > TILER_WIDTH * TILER_HEIGHT)
			return -1;
		return 0;
	default:
		return -EINVAL;
	}

	/* get the # of bytes per row in 1 slot */
	bpp = tilfmt_bpp(fmt);
	slot_row = slot_w * bpp;

	/* how many slots are can be accessed via one physical page */
	*band = PAGE_SIZE / slot_row;

	/* minimum alignment is 1 slot, default alignment is page size */
	*align = ALIGN(*align ? : PAGE_SIZE, slot_row);

	/* offset must be multiple of bpp */
	if (*offs & (bpp - 1))
		return -EINVAL;

	/* round down the offset to the nearest slot size, and increase width
	   to allow space for having the correct offset */
	width += (*offs & (*align - 1)) / bpp;
	*offs &= ~(*align - 1);

	/* adjust to slots */
	*x_area = DIV_ROUND_UP(width, slot_w);
	*y_area = DIV_ROUND_UP(height, slot_h);
	*align /= slot_row;
	*offs /= slot_row;

	if (*x_area > TILER_WIDTH || *y_area > TILER_HEIGHT)
		return -1;
	return 0x0;
}

/**
 * Find a place where a 2D block would fit into a 2D area of the
 * same height.
 *
 * @author a0194118 (3/19/2010)
 *
 * @param w	Width of the block.
 * @param align	Alignment of the block.
 * @param offs	Offset of the block (within alignment)
 * @param ai	Pointer to area info
 * @param next	Pointer to the variable where the next block
 *              will be stored.  The block should be inserted
 *              before this block.
 *
 * @return the end coordinate (x1 + 1) where a block would fit,
 *         or 0 if it does not fit.
 *
 * (must have mutex)
 */
static u16 _m_blk_find_fit(u16 w, u16 align, u16 offs,
			struct area_info *ai, struct list_head **before)
{
	int x = ai->area.p0.x + w + offs;
	struct til_mem_info *mi, *tmp;

	/* area blocks are sorted by x */
	list_for_each_entry_safe(mi, tmp, &ai->blocks, by_area) {
		/* check if buffer would fit before this area */
		if (x <= mi->area.p0.x) {
			*before = &mi->by_area;
			return x;
		}
		x = ALIGN(mi->area.p1.x + 1 - offs, align) + w + offs;
	}
	*before = &ai->blocks;

	/* check if buffer would fit after last area */
	return (x <= ai->area.p1.x + 1) ? x : 0;
}

/* (must have mutex) adds a block to an area with certain x coordinates */
static inline
struct til_mem_info *_m_add2area(struct til_mem_info *mi, struct area_info *ai,
				u16 x0, u16 x1, struct list_head *before)
{
	mi->parent = ai;
	mi->area = ai->area;
	mi->area.p0.x = x0;
	mi->area.p1.x = x1;
	list_add_tail(&mi->by_area, before);
	ai->nblocks++;
	return mi;
}

static struct til_mem_info *get_2d_area(u16 w, u16 h, u16 align, u16 offs, u16 band,
					struct gid_info *gi, struct tcm *tcm) {
	struct area_info *ai = NULL, *ai_tmp = NULL;
	struct til_mem_info *mi = NULL, *tmp = NULL;
	struct list_head *before = NULL;
	u16 x = 0;   /* this holds the end of a potential area */

	/* allocate map info */

	/* see if there is available prereserved space */
	mutex_lock(&mtx);
	list_for_each_entry_safe(mi, tmp, &gi->reserved, global) {
		if (mi->area.tcm == tcm &&
			tcm_aheight(mi->area) == h &&
			tcm_awidth(mi->area) == w &&
			(mi->area.p0.x & (align - 1)) == offs) {
			/* this area is already set up */

			/* remove from reserved list */
			list_del(&mi->global);
			if (tiler_alloc_debug & 1)
				fprintf(stderr, "(=2d (%d-%d,%d-%d) in (%d-%d,%d-%d) prereserved)\n",
					mi->area.p0.x, mi->area.p1.x,
					mi->area.p0.y, mi->area.p1.y,
			((struct area_info *) mi->parent)->area.p0.x,
			((struct area_info *) mi->parent)->area.p1.x,
			((struct area_info *) mi->parent)->area.p0.y,
			((struct area_info *) mi->parent)->area.p1.y);
			goto done;
		}
	}
	mutex_unlock(&mtx);

	/* if not, reserve a block struct */
	mi = kmalloc(sizeof(*mi), GFP_KERNEL);
	if (!mi)
		return mi;
	memset(mi, 0, sizeof(*mi));

	/* see if allocation fits in one of the existing areas */
	/* this sets x, ai and before */
	mutex_lock(&mtx);
	list_for_each_entry_safe(ai, ai_tmp, &gi->areas, by_gid) {
		if (ai->area.tcm == tcm &&
			tcm_aheight(ai->area) == h) {
			x = _m_blk_find_fit(w, align, offs, ai, &before);
			if (x) {
				_m_add2area(mi, ai, x - w, x - 1, before);
				if (tiler_alloc_debug & 1)
					fprintf(stderr, "(+2d (%d-%d,%d-%d) in (%d-%d,%d-%d) existing)\n",
						mi->area.p0.x, mi->area.p1.x,
						mi->area.p0.y, mi->area.p1.y,
				((struct area_info *) mi->parent)->area.p0.x,
				((struct area_info *) mi->parent)->area.p1.x,
				((struct area_info *) mi->parent)->area.p0.y,
				((struct area_info *) mi->parent)->area.p1.y);
				goto done;
			}
		}
	}
	mutex_unlock(&mtx);

	/* if no area fit, reserve a new one */
	ai = area_new(ALIGN(w + offs, max(band, align)), h,
			max(band, align), tcm, gi);
	if (ai) {
		mutex_lock(&mtx);
		_m_add2area(mi, ai, ai->area.p0.x + offs,
				ai->area.p0.x + offs + w - 1,
				&ai->blocks);
		if (tiler_alloc_debug & 1)
			fprintf(stderr, "(+2d (%d-%d,%d-%d) in (%d-%d,%d-%d) new)\n",
				mi->area.p0.x, mi->area.p1.x,
				mi->area.p0.y, mi->area.p1.y,
				ai->area.p0.x, ai->area.p1.x,
				ai->area.p0.y, ai->area.p1.y);
	} else {
		/* clean up */
		kfree(mi);
		return NULL;
	}

done:
	mutex_unlock(&mtx);
	return mi;
}

/* (must have mutex) */
static void _m_try_free_group(struct gid_info *gi)
{
	if (gi && list_empty(&gi->areas) && list_empty(&gi->onedim)) {
		//WARN_ON(!list_empty(&gi->reserved));
		list_del(&gi->by_pid);

		/* if group is tracking kernel objects, we may free even
		   the process info */
		if (gi->pi->kernel && list_empty(&gi->pi->groups)) {
			list_del(&gi->pi->list);
			kfree(gi->pi);
		}

		kfree(gi);
	}
}

static void clear_pat(struct tmm *tmm, struct tcm_area *area, enum tiler_fmt fmt)
{
	struct pat_area p_area = {0};
	struct tcm_area slice, area_s;
	int i;
	u32 y_offset = 0;

#ifdef TILER_PLATFORM_OMAP5
	if (fmt == TILFMT_PAGE)
		y_offset = 128;
#endif

	tcm_for_each_slice(slice, *area, area_s) {
		p_area.x0 = slice.p0.x;
		p_area.y0 = slice.p0.y + y_offset;
		p_area.x1 = slice.p1.x;
		p_area.y1 = slice.p1.y + y_offset;

		for (i = 0; i<tcm_sizeof(slice); i++) {
			dmac_va[i] = dummy_pa;
		}
		/* save the info to use for context restore */
		(*tiler_save_pat)(&p_area, dmac_va);

		tmm_map(tmm, p_area, dmac_pa);
	}
}

/* (must have mutex) free block and any freed areas */
static s32 _m_free(struct til_mem_info *mi)
{
	struct area_info *ai = NULL;
	s32 res = 0;

	/* release memory */
	if (mi->pg_ptr) {
		kfree(mi->pg_ptr);
	} else if (mi->mem) {
		tmm_free(TMM_SS(mi->sys_addr), mi->mem);
	}

	/* safe deletion as list may not have been assigned */
	if (mi->global.next)
		list_del(&mi->global);
	if (mi->by_area.next)
		list_del(&mi->by_area);

	/* remove block from area first if 2D */
	if (mi->area.is2d) {
		ai = mi->parent;

		/* check to see if area needs removing also */
		if (ai && !--ai->nblocks) {
			if (tiler_alloc_debug & 1)
				fprintf(stderr, "(-2d (%d-%d,%d-%d) in (%d-%d,%d-%d) last)\n",
					mi->area.p0.x, mi->area.p1.x,
					mi->area.p0.y, mi->area.p1.y,
					ai->area.p0.x, ai->area.p1.x,
					ai->area.p0.y, ai->area.p1.y);
			clear_pat(TMM_SS(mi->sys_addr), &ai->area, TILER_GET_ACC_MODE(mi->sys_addr));
			res = tcm_free(&ai->area);
			list_del(&ai->by_gid);
			/* try to remove parent if it became empty */
			_m_try_free_group(ai->gi);
			kfree(ai);
			ai = NULL;
		} else if (tiler_alloc_debug & 1)
			fprintf(stderr, "(-2d (%d-%d,%d-%d) in (%d-%d,%d-%d) remaining)\n",
				mi->area.p0.x, mi->area.p1.x,
				mi->area.p0.y, mi->area.p1.y,
				ai->area.p0.x, ai->area.p1.x,
				ai->area.p0.y, ai->area.p1.y);

	} else {
		if (tiler_alloc_debug & 1)
			fprintf(stderr, "(-1d: %d,%d..%d,%d)\n",
				mi->area.p0.x, mi->area.p0.y,
				mi->area.p1.x, mi->area.p1.y);
		/* remove 1D area */
		clear_pat(TMM_SS(mi->sys_addr), &mi->area, TILER_GET_ACC_MODE(mi->sys_addr));
		res = tcm_free(&mi->area);
		/* try to remove parent if it became empty */
		_m_try_free_group(mi->parent);
	}

	kfree(mi);
	return res;
}

/* (must have mutex) returns true if block was freed */
static bool _m_chk_ref(struct til_mem_info *mi)
{
	/* check references */
	if (mi->refs)
		return 0;

	if (_m_free(mi))
		fprintf(stderr, "error while removing tiler block\n");

	return 1;
}

/* (must have mutex) */
static inline s32 _m_dec_ref(struct til_mem_info *mi)
{
	if (mi->refs-- <= 1)
		return _m_chk_ref(mi);

	return 0;
}

/* (must have mutex) */
static inline void _m_inc_ref(struct til_mem_info *mi)
{
	mi->refs++;
}

/* (must have mutex) returns true if block was freed */
static inline bool _m_try_free(struct til_mem_info *mi)
{
	if (mi->alloced) {
		mi->refs--;
		mi->alloced = false;
	}
	return _m_chk_ref(mi);
}

static s32 register_buf(struct __buf_info *_b, struct process_info *pi)
{
	struct til_mem_info *mi = NULL, *tmp = NULL;
	struct tiler_buf_info *b = &_b->buf_info;
	u32 i, num = b->num_blocks, remain = num;

	/* check validity */
	if (num > TILER_MAX_NUM_BLOCKS)
		return -EINVAL;

	mutex_lock(&mtx);

	/* find each block */
	list_for_each_entry_safe(mi, tmp, &blocks, global) {
		for (i = 0; i < num; i++) {
			if (!_b->mi[i] && mi->sys_addr == b->blocks[i].ssptr) {
				_b->mi[i] = mi;

				/* quit if found all*/
				if (!--remain)
					break;

			}
		}
	}

	/* if found all, register buffer */
	if (!remain) {
		b->offset = id;
		id += 0x1000;

		list_add(&_b->by_pid, &pi->bufs);

		/* using each block */
		for (i = 0; i < num; i++)
			_m_inc_ref(_b->mi[i]);
	}

	mutex_unlock(&mtx);

	return remain ? -EACCES : 0;
}

/* must have mutex */
static void _m_unregister_buf(struct __buf_info *_b)
{
	u32 i;

	/* unregister */
	list_del(&_b->by_pid);

	/* no longer using the blocks */
	for (i = 0; i < _b->buf_info.num_blocks; i++)
		_m_dec_ref(_b->mi[i]);

	kfree(_b);
}

static int tiler_notify_event(int event, void *data)
{
	struct list_head *elem, *tmp;
	struct reg_notif_info *info = NULL;

	/* TODO: need list protection */
	list_for_each_safe(elem, tmp, &reg_notif) {
		info = (struct reg_notif_info *)elem;
		info->cb_ptr(event, data, info->arg);
	}
	/* TODO: need list protection */

	return 0;
}

static int tiler_notify_usr(int event_type, void *data, void *arg);

/**
 * Free all info kept by a process:
 *
 * all registered buffers, allocated blocks, and unreferenced
 * blocks.  Any blocks/areas still referenced will move to the
 * orphaned lists to avoid issues if a new process is created
 * with the same pid.
 *
 * (must have mutex)
 */
static void _m_free_process_info(struct process_info *pi)
{
	struct area_info *ai, *ai_;
	struct til_mem_info *mi, *mi_;
	struct gid_info *gi, *gi_;
	struct __buf_info *_b = NULL, *_b_ = NULL;
	bool ai_autofreed, need2free;
	struct list_head *elem, *tmp;
	struct reg_notif_usr_info *pvtInfo = NULL;

	list_for_each_safe(elem, tmp, &reg_notif_usr) {
		pvtInfo = (struct reg_notif_usr_info *)elem;
		if (pvtInfo->pid == pi->pid) {
			tiler_unreg_notifier(&tiler_notify_usr, pvtInfo);
			list_del(elem);
			kfree(pvtInfo->name);
			kfree(pvtInfo);
		}
	}

	if (!list_empty(&pi->bufs))
		tiler_notify_event(TILER_DEVICE_CLOSE, (void *)pi->pid);

	/* unregister all buffers */
	list_for_each_entry_safe(_b, _b_, &pi->bufs, by_pid)
		_m_unregister_buf(_b);

	//WARN_ON(!list_empty(&pi->bufs));

	/* free all allocated blocks, and remove unreferenced ones */
	list_for_each_entry_safe(gi, gi_, &pi->groups, by_pid) {

		/*
		 * Group info structs when they become empty on an _m_try_free.
		 * However, if the group info is already empty, we need to
		 * remove it manually
		 */
		need2free = list_empty(&gi->areas) && list_empty(&gi->onedim);
		list_for_each_entry_safe(ai, ai_, &gi->areas, by_gid) {
			ai_autofreed = true;
			list_for_each_entry_safe(mi, mi_, &ai->blocks, by_area)
				ai_autofreed &= _m_try_free(mi);

			/* save orphaned areas for later removal */
			if (!ai_autofreed) {
				need2free = true;
				ai->gi = NULL;
				list_move(&ai->by_gid, &orphan_areas);
			}
		}

		list_for_each_entry_safe(mi, mi_, &gi->onedim, by_area) {
			if (!_m_try_free(mi)) {
				need2free = true;
				/* save orphaned 1D blocks */
				mi->parent = NULL;
				list_move(&mi->by_area, &orphan_onedim);
			}
		}

		/* if group is still alive reserved list should have been
		   emptied as there should be no reference on those blocks */
		if (need2free) {
			//WARN_ON(!list_empty(&gi->onedim));
			//WARN_ON(!list_empty(&gi->areas));
			_m_try_free_group(gi);
		}
	}

	//WARN_ON(!list_empty(&pi->groups));
	list_del(&pi->list);
	kfree(pi);
}

static s32 get_area(u32 sys_addr, struct tcm_pt *pt)
{
	enum tiler_fmt fmt;

	sys_addr &= TILER_ALIAS_VIEW_CLEAR;
	fmt = TILER_GET_ACC_MODE(sys_addr);

	switch (fmt) {
	case TILFMT_8BIT:
		pt->x = DMM_HOR_X_PAGE_COOR_GET_8(sys_addr);
		pt->y = DMM_HOR_Y_PAGE_COOR_GET_8(sys_addr);
		break;
	case TILFMT_16BIT:
		pt->x = DMM_HOR_X_PAGE_COOR_GET_16(sys_addr);
		pt->y = DMM_HOR_Y_PAGE_COOR_GET_16(sys_addr);
		break;
	case TILFMT_32BIT:
		pt->x = DMM_HOR_X_PAGE_COOR_GET_32(sys_addr);
		pt->y = DMM_HOR_Y_PAGE_COOR_GET_32(sys_addr);
		break;
	case TILFMT_PAGE:
		pt->x = (sys_addr & 0x7FFFFFF) >> 12;
		pt->y = pt->x / TILER_WIDTH;
		pt->x &= (TILER_WIDTH - 1);
		break;
	default:
		return -EFAULT;
	}
	return 0x0;
}

static u32 __get_alias_addr(enum tiler_fmt fmt, u16 x, u16 y)
{
	u32 acc_mode = -1;
	u32 x_shft = -1, y_shft = -1;

	switch (fmt) {
	case TILFMT_8BIT:
		acc_mode = 0; x_shft = 6; y_shft = 20;
		break;
	case TILFMT_16BIT:
		acc_mode = 1; x_shft = 7; y_shft = 20;
		break;
	case TILFMT_32BIT:
		acc_mode = 2; x_shft = 7; y_shft = 20;
		break;
	case TILFMT_PAGE:
		acc_mode = 3; y_shft = 8;
		break;
	default:
		return 0;
		break;
	}

	if (fmt == TILFMT_PAGE)
		return (u32)TIL_ALIAS_ADDR((x | y << y_shft) << 12, acc_mode);
	else
		return (u32)TIL_ALIAS_ADDR(x << x_shft | y << y_shft, acc_mode);
}

/* must have mutex */
static struct gid_info *_m_get_gi(struct process_info *pi, u32 gid)
{
	struct gid_info *gi, *tmp;

	/* see if group already exist */
	list_for_each_entry_safe(gi, tmp, &pi->groups, by_pid) {
		if (gi->gid == gid)
			return gi;
	}

	/* create new group */
	gi = kmalloc(sizeof(*gi), GFP_KERNEL);
	if (!gi)
		return gi;

	memset(gi, 0, sizeof(*gi));
	INIT_LIST_HEAD(&gi->areas);
	INIT_LIST_HEAD(&gi->onedim);
	INIT_LIST_HEAD(&gi->reserved);
	gi->pi = pi;
	gi->gid = gid;
	list_add(&gi->by_pid, &pi->groups);
	return gi;
}

static struct til_mem_info *__get_area(enum tiler_fmt fmt, u32 width, u32 height,
				   u16 align, u16 offs, struct gid_info *gi)
{
	u16 x, y, band;
	struct til_mem_info *mi = NULL;

	/* calculate dimensions, band, offs and alignment in slots */
	if (__analyze_area(fmt, width, height, &x, &y, &band, &align, &offs))
		return NULL;

	if (fmt == TILFMT_PAGE)	{
		/* 1D areas don't pack */
		mi = kmalloc(sizeof(*mi), GFP_KERNEL);
		if (!mi)
			return NULL;
		memset(mi, 0x0, sizeof(*mi));

		if (tcm_reserve_1d(TCM(fmt), x * y, &mi->area)) {
			kfree(mi);
			return NULL;
		}
		if (tiler_alloc_debug & 1)
			fprintf(stderr, "(+1d: %d,%d..%d,%d)\n",
				mi->area.p0.x, mi->area.p0.y,
				mi->area.p1.x, mi->area.p1.y);
		mutex_lock(&mtx);
		mi->parent = gi;
		list_add(&mi->by_area, &gi->onedim);
	} else {
		mi = get_2d_area(x, y, align, offs, band, gi, TCM(fmt));
		if (!mi)
			return NULL;

		mutex_lock(&mtx);
	}

	list_add(&mi->global, &blocks);
	mi->alloced = true;
	mi->refs++;
	mutex_unlock(&mtx);

	mi->sys_addr = __get_alias_addr(fmt, mi->area.p0.x, mi->area.p0.y);
	return mi;
}

#define PAGE_SHIFT      12

int tiler_mmap(resmgr_context_t *ctp, uint32_t offset, tiler_ocb_t *ocb)
{
	struct __buf_info *_b = NULL;
	struct tiler_buf_info *b = NULL;
	s32 i = 0, j = 0, k = 0, m = 0, p = 0, bpp = 1, size = 0;
	struct list_head *pos = NULL, *tmp = NULL;
	struct process_info *pi = ocb->pi;
	void *va = NULL;
	void *ret = NULL;
	int rc = 0;

	mutex_lock(&mtx);
	list_for_each_safe(pos, tmp, &pi->bufs) {
		_b = list_entry(pos, struct __buf_info, by_pid);
		if (offset == _b->buf_info.offset)
			break;
	}
	mutex_unlock(&mtx);
	if (!_b)
		return -ENXIO;

	/* First get the total size of the area to map. */
	b = &_b->buf_info;
	for (i = 0; i < b->num_blocks; i++) {
		if (b->blocks[i].fmt >= TILFMT_8BIT &&
					b->blocks[i].fmt <= TILFMT_32BIT) {
			/* get line width */
			bpp = (b->blocks[i].fmt == TILFMT_8BIT ? 1 :
				b->blocks[i].fmt == TILFMT_16BIT ? 2 : 4);
			p = PAGE_ALIGN(b->blocks[i].dim.area.width * bpp);

			for (j = 0; j < b->blocks[i].dim.area.height; j++) {
				size += p;
			}
		} else if (b->blocks[i].fmt == TILFMT_PAGE) {
			p = PAGE_ALIGN(b->blocks[i].dim.len);
			size += p;
		}
	}

	/* Do a dummy mapping of this area to get contiguous virtual mem. */
	va = mmap64_peer(ctp->info.pid, NULL, size, PROT_NOCACHE | PROT_NONE, MAP_ANON | MAP_LAZY, NOFD, 0);
	if (va == MAP_FAILED) {
		fprintf(stderr, "tiler_mmap: mmap failed.");
		return -EAGAIN;
	}

	i = 0;
	j = 0;
	k = 0;
	m = 0;
	p = 0;
	bpp = 1;
	b = &_b->buf_info;
	for (i = 0; i < b->num_blocks; i++) {
		if (b->blocks[i].fmt >= TILFMT_8BIT &&
					b->blocks[i].fmt <= TILFMT_32BIT) {
			/* get line width */
			bpp = (b->blocks[i].fmt == TILFMT_8BIT ? 1 :
				b->blocks[i].fmt == TILFMT_16BIT ? 2 : 4);
			p = PAGE_ALIGN(b->blocks[i].dim.area.width * bpp);

			for (j = 0; j < b->blocks[i].dim.area.height; j++) {
				/* map each page of the line */
				ret = mmap64_peer(ctp->info.pid, va + k, p,
						PROT_NOCACHE | PROT_READ | PROT_WRITE,
						MAP_FIXED | MAP_PHYS | MAP_SHARED, NOFD,
						(b->blocks[i].ssptr + m));
				if (ret != va + k) {
					/* TODO: should unmap any mapped regions upon failure? */
					return -EAGAIN;
				}
				k += p;
				if (b->blocks[i].fmt == TILFMT_8BIT)
					m += 64*TILER_WIDTH;
				else
					m += 2*64*TILER_WIDTH;
			}
			m = 0;
		} else if (b->blocks[i].fmt == TILFMT_PAGE) {
			p = PAGE_ALIGN(b->blocks[i].dim.len);
			/* protect shm_open/unlink, so that each open gets a
			 * unique memory object */
			mutex_lock(&mtx);
			int fd = shm_open("tiler1dmem",
						O_RDWR|O_CREAT|O_TRUNC, 0600);
			if (fd == -1) {
				mutex_unlock(&mtx);
				perror("tiler_mmap: shm_open");
				return -EAGAIN;
			}
			shm_unlink("tiler1dmem");
			mutex_unlock(&mtx);
			/* Want non-cacheable, bufferable memory (Device memory)
			 * This translates for ARMv7 to
			 *     TEX=0, C=0, B=1, S=ignored for device memory */
			int special = ARM_PTE_V6_SP_TEX(0) | ARM_PTE_B;
			rc = shm_ctl_special(fd, SHMCTL_PHYS,
						(unsigned)(b->blocks[i].ssptr),
						p, special);
			if (rc == -1) {
				perror("tiler_mmap: shm_ctl_special");
				close (fd);
				return -EAGAIN;
			}
			ret = mmap64_peer(ctp->info.pid, va + k, p,
					PROT_NOCACHE | PROT_READ | PROT_WRITE,
					MAP_FIXED | MAP_SHARED, fd, 0);
			close (fd);
			if (ret != va + k) {
				/* TODO: should unmap any mapped regions upon failure? */
				return -EAGAIN;
			}
			k += p;
		}
	}

	return (int)va;
}

static s32 refill_pat(struct tmm *tmm, struct tcm_area *area, u32 *ptr, enum tiler_fmt fmt)
{
	s32 res = 0;
	struct pat_area p_area = {0};
	struct tcm_area slice, area_s;
	u32 y_offset = 0;

#ifdef TILER_PLATFORM_OMAP5
	if (fmt == TILFMT_PAGE)
		y_offset = 128;
#endif

	tcm_for_each_slice(slice, *area, area_s) {
		p_area.x0 = slice.p0.x;
		p_area.y0 = slice.p0.y + y_offset;
		p_area.x1 = slice.p1.x;
		p_area.y1 = slice.p1.y + y_offset;

		memcpy(dmac_va, ptr, sizeof(*ptr) * tcm_sizeof(slice));
		/* save the info to use for context restore */
		(*tiler_save_pat)(&p_area, ptr);

		ptr += tcm_sizeof(slice);

		if (tmm_map(tmm, p_area, dmac_pa)) {
			res = -EFAULT;
			break;
		}
	}

	return res;
}

static s32 map_block(enum tiler_fmt fmt, u32 width, u32 height, u32 gid,
			struct process_info *pi, u32 *sys_addr, u32 usr_addr)
{
	u32 i = 0, j = 0, tmp = -1, len = 0, bytes = 0;
	s64 offset = 0;
	s32 res = -ENOMEM, status = 0;
	struct til_mem_info *mi = NULL;
	struct gid_info *gi = NULL;

	/* we only support mapping a user buffer in page mode */
	if (fmt != TILFMT_PAGE)
		return -EPERM;

	/* check if mapping is supported by tmm */
	if (!tmm_can_map(TMM(fmt)))
		return -EPERM;

	/* get group context */
	mutex_lock(&mtx);
	gi = _m_get_gi(pi, gid);
	mutex_unlock(&mtx);

	if (!gi)
		return -ENOMEM;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, 0, 0, gi);
	if (!mi) {
		mutex_lock(&mtx);
		_m_try_free_group(gi);
		mutex_unlock(&mtx);
		return -ENOMEM;
	}

	*sys_addr = mi->sys_addr;
	mi->usr = usr_addr;

	/* allocate pages */
	mi->num_pg = tcm_sizeof(mi->area);

	mi->pg_ptr = kmalloc(mi->num_pg * sizeof(*mi->pg_ptr), GFP_KERNEL);
	if (!mi->pg_ptr)
		goto done;
	memset(mi->pg_ptr, 0x0, sizeof(*mi->pg_ptr) * mi->num_pg);

	/*
	 * Important Note: usr_addr is mapped from user
	 * application process to current process - it must lie
	 * completely within the current virtual memory address
	 * space in order to be of use to us here.
	 */
	res = -EFAULT;

	/*
	 * It is observed that under some circumstances, the user
	 * buffer is spread across several vmas, so loop through
	 * and check if the entire user buffer is covered.
	 */
	tmp = mi->usr;
	bytes = mi->num_pg * PAGE_SIZE;
	do {
		status = mem_offset64_peer(pi->pid, tmp, bytes, &offset, &len);
		if (status == 0) {
			for (j = 0; j < len/PAGE_SIZE; j++) {
				mi->pg_ptr[i++] = offset;
				offset += PAGE_SIZE;
			}
			tmp += len;
			bytes -= len;
		}
		else {
			fprintf(stderr, "mem_offset64_peer() failed %d", errno);
			goto fault;
		}
	} while (bytes > 0);

	/* Ensure the data reaches to main memory before PAT refill */
	wmb();

	mutex_lock(&mtx);
	if (refill_pat(TMM(fmt), &mi->area, mi->pg_ptr, fmt)) {
		mutex_unlock(&mtx);
		goto fault;
	}
	mutex_unlock(&mtx);

	res = 0;
	goto done;
fault:
done:
	if (res) {
		mutex_lock(&mtx);
		_m_free(mi);
		mutex_unlock(&mtx);
	}
	return res;
}

s32 tiler_mapx(enum tiler_fmt fmt, u32 width, u32 height, u32 gid,
				void *pi, u32 *sys_addr, u32 usr_addr)
{
	return map_block(fmt, width, height, gid, (struct process_info *)pi,
							sys_addr, usr_addr);
}

s32 tiler_map(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr,
								u32 usr_addr)
{
	return tiler_mapx(fmt, width, height, 0, NULL, sys_addr,
								usr_addr);
}

static s32 free_block(u32 sys_addr, struct process_info *pi)
{
	struct gid_info *gi = NULL, *gi_tmp = NULL;
	struct area_info *ai = NULL, *ai_tmp = NULL;
	struct til_mem_info *mi = NULL, *mi_tmp = NULL;
	s32 res = -ENOENT;

	mutex_lock(&mtx);

	/* find block in process list and free it */
	list_for_each_entry_safe(gi, gi_tmp, &pi->groups, by_pid) {
		/* currently we know if block is 1D or 2D by the address */
		if (TILER_GET_ACC_MODE(sys_addr) == TILFMT_PAGE) {
			list_for_each_entry_safe(mi, mi_tmp, &gi->onedim, by_area) {
				if (mi->sys_addr == sys_addr) {
					_m_try_free(mi);
					res = 0;
					goto done;
				}
			}
		} else {
			list_for_each_entry_safe(ai, ai_tmp, &gi->areas, by_gid) {
				list_for_each_entry_safe(mi, mi_tmp, &ai->blocks, by_area) {
					if (mi->sys_addr == sys_addr) {
						_m_try_free(mi);
						res = 0;
						goto done;
					}
				}
			}
		}
	}

done:
	mutex_unlock(&mtx);

	/* for debugging, we can set the PAT entries to DMM_LISA_MAP__0 */
	return res;
}

s32 tiler_free(u32 sys_addr)
{
	struct til_mem_info *mi, *mi_tmp;
	s32 res = -ENOENT;

	mutex_lock(&mtx);

	/* find block in global list and free it */
	list_for_each_entry_safe(mi, mi_tmp, &blocks, global) {
		if (mi->sys_addr == sys_addr) {
			_m_try_free(mi);
			res = 0;
			break;
		}
	}
	mutex_unlock(&mtx);

	/* for debugging, we can set the PAT entries to DMM_LISA_MAP__0 */
	return res;
}

/* :TODO: Currently we do not track enough information from alloc to get back
   the actual width and height of the container, so we must make a guess.  We
   do not even have enough information to get the virtual stride of the buffer,
   which is the real reason for this ioctl */
static s32 find_block(u32 sys_addr, struct tiler_block_info *blk)
{
	struct til_mem_info *i, *tmp;
	struct tcm_pt pt;

	if (get_area(sys_addr, &pt))
		return -EFAULT;

	list_for_each_entry_safe(i, tmp, &blocks, global) {
		if (tcm_is_in(pt, i->area))
			goto found;
	}

	blk->fmt = TILFMT_INVALID;
	blk->dim.len = blk->stride = blk->ssptr = 0;
	return -EFAULT;

found:
	blk->ptr = NULL;
	blk->fmt = TILER_GET_ACC_MODE(sys_addr);
	blk->ssptr = __get_alias_addr(blk->fmt, i->area.p0.x, i->area.p0.y);

	if (blk->fmt == TILFMT_PAGE) {
		blk->dim.len = tcm_sizeof(i->area) * TILER_PAGE;
		blk->stride = 0;
	} else {
		blk->stride = blk->dim.area.width =
			tcm_awidth(i->area) * TILER_BLOCK_WIDTH;
		blk->dim.area.height = tcm_aheight(i->area)
							* TILER_BLOCK_HEIGHT;
		if (blk->fmt != TILFMT_8BIT) {
			blk->stride <<= 1;
			blk->dim.area.height >>= 1;
			if (blk->fmt == TILFMT_32BIT)
				blk->dim.area.width >>= 1;
		}
		blk->stride = PAGE_ALIGN(blk->stride);
	}
	return 0;
}

static s32 alloc_block(enum tiler_fmt fmt, u32 width, u32 height,
			u32 align, u32 offs, u32 gid, struct process_info *pi,
			u32 *sys_addr);

int
tiler_devctl(resmgr_context_t *ctp, io_devctl_t *msg, tiler_ocb_t *ocb)
{
	int status;
	int nbytes=0;
	s32 r = -1;
	u32 til_addr = 0x0;
	struct process_info *pi = ocb->pi;
	u64 gssp_ptr = 0;
	u32 size = 0;

	struct __buf_info *_b = NULL, *_b_tmp = NULL;
	struct tiler_buf_info *buf_info;
	struct tiler_block_info *block_info;
	uint32_t *offset = NULL;

	if ((status = iofunc_devctl_default(ctp, msg, &ocb->hdr)) != _RESMGR_DEFAULT)
		return(_RESMGR_ERRNO(status));
	status=0;

	switch (msg->i.dcmd)
	{
	case TILIOC_MMAP:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(uint32_t)) {
			msg->o.ret_val = (int32_t)MAP_FAILED;
			return (_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o)));
		}
		offset = (uint32_t *)_DEVCTL_DATA(msg->i);
		msg->o.ret_val = tiler_mmap(ctp, *offset, ocb);
		if (msg->o.ret_val < 0) {
			msg->o.ret_val = (int32_t)MAP_FAILED;
		}
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(uint32_t)));
		break;
	case TILIOC_GBUF:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_block_info)) {
			msg->o.ret_val = -EINVAL;
			return (_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o)));
		}
		// get a pointer to the structure
		block_info = (struct tiler_block_info *)_DEVCTL_DATA(msg->i);

		switch (block_info->fmt) {
		case TILFMT_PAGE:
			r = alloc_block(block_info->fmt, block_info->dim.len, 1,
						0, 0, 0, pi, &til_addr);
			if (r) {
				msg->o.ret_val = r;
				return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
			}
			break;
		case TILFMT_8BIT:
		case TILFMT_16BIT:
		case TILFMT_32BIT:
			r = alloc_block(block_info->fmt,
					block_info->dim.area.width,
					block_info->dim.area.height,
					0, 0, 0, pi, &til_addr);
			if (r) {
				msg->o.ret_val = r;
				return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
			}
			break;
		default:
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		}

		block_info->ssptr = til_addr;
		msg->o.ret_val = 0;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		break;
	case TILIOC_FBUF:
	case TILIOC_UMBUF:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_block_info)) {
			msg->o.ret_val = -EINVAL;
			return (_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o)));
		}
		block_info = (struct tiler_block_info *)_DEVCTL_DATA(msg->i);

		/* search current process first, then all processes */
		free_block(block_info->ssptr, pi) ?
			tiler_free(block_info->ssptr) : 0;

		/* free always succeeds */
		msg->o.ret_val = 0;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		break;

	case TILIOC_GSSP:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(uint32_t)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		offset = (uint32_t *)_DEVCTL_DATA(msg->i);
		status = mem_offset64_peer(ctp->info.pid, *offset, 1, (s64 *)&gssp_ptr, &size);
		if (status < 0) {
			msg->o.ret_val = 0;
		}
		else {
			msg->o.ret_val = gssp_ptr;
		}
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(uint32_t)));
		break;
	case TILIOC_MBUF:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_block_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		block_info = (struct tiler_block_info *)_DEVCTL_DATA(msg->i);

		if (!block_info->ptr) {
			msg->o.ret_val = -EFAULT;
			return (_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		}

		if (map_block(block_info->fmt, block_info->dim.len, 1, 0, pi,
				&block_info->ssptr, (u32)block_info->ptr)) {
			msg->o.ret_val = -ENOMEM;
			return (_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		}

		msg->o.ret_val = 0;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));

		break;
	case TILIOC_QBUF:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_buf_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		buf_info = (struct tiler_buf_info *)_DEVCTL_DATA(msg->i);

		mutex_lock(&mtx);
		list_for_each_entry_safe(_b, _b_tmp, &pi->bufs, by_pid) {
			if (buf_info->offset == _b->buf_info.offset) {
				memcpy(buf_info, &_b->buf_info, sizeof(_b->buf_info));
				msg->o.ret_val = 0;
				mutex_unlock(&mtx);
				return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
			}
		}
		mutex_unlock(&mtx);
		msg->o.ret_val = -EFAULT;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
		break;
	case TILIOC_RBUF:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_buf_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		_b = kmalloc(sizeof(*_b), GFP_KERNEL);
		if (!_b) {
			msg->o.ret_val = -ENOMEM;
			return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
		}
		memset(_b, 0x0, sizeof(*_b));

		buf_info = (struct tiler_buf_info *)_DEVCTL_DATA(msg->i);
		memcpy(&_b->buf_info, buf_info, sizeof(_b->buf_info));

		r = register_buf(_b, pi);
		if (r) {
			kfree(_b);
			msg->o.ret_val = -EACCES;
			return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
		}

		memcpy(buf_info, &_b->buf_info, sizeof(_b->buf_info));
		msg->o.ret_val = 0;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
		break;
	case TILIOC_URBUF:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_buf_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		buf_info = (struct tiler_buf_info *)_DEVCTL_DATA(msg->i);

		pthread_mutex_lock(&mtx);
		/* buffer registration is per process */
		list_for_each_entry_safe(_b, _b_tmp, &pi->bufs, by_pid) {
			if (buf_info->offset == _b->buf_info.offset) {
				_m_unregister_buf(_b);
				mutex_unlock(&mtx);
				msg->o.ret_val = 0;
				return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
			}
		}
		mutex_unlock(&mtx);
		msg->o.ret_val = -EFAULT;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_buf_info)));
		break;
	case TILIOC_QUERY_BLK:
		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_block_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		block_info = (struct tiler_block_info *)_DEVCTL_DATA(msg->i);

		if (find_block(block_info->ssptr, block_info)) {
			msg->o.ret_val = -EFAULT;
			return (_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		}

		msg->o.ret_val = 0;
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_block_info)));
		break;
	case TILIOC_USRMX:
	{
		struct tiler_mapx_info *info = (struct tiler_mapx_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_mapx_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_mapx(info->fmt, info->width, info->height, info->gid,
					pi, &info->sys_addr, info->usr_addr);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_mapx_info)));
		break;
	}
	case TILIOC_USRF:
	case TILIOC_USRFB:
	{
		u32 *free_addr = (u32 *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(u32)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_free(*free_addr);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(u32)));
		break;
	}
	case TILIOC_USRGX:
	{
		struct tiler_allocx_info *info = (struct tiler_allocx_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_allocx_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_allocx(info->fmt, info->width, info->height,
				 info->align, info->offs, info->gid, (void *)pi, &info->sys_addr);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_allocx_info)));
		break;
	}
	case TILIOC_USRRX:
	{
		struct tiler_reservex_info *info = (struct tiler_reservex_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_reservex_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_reservex(info->n, &info->b, info->pid);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_reservex_info)));
		break;
	}
	case TILIOC_USRGP:
	{
		struct tiler_allocp_info *info = (struct tiler_allocp_info *)_DEVCTL_DATA(msg->i);
		int count = 0;
		void **sysptr_info = (void **)(info+1);
		void **allocptr_info = (void **)(NULL);
		uint32_t total_bytes = 0;

		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_allocp_info)) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		count = info->count;
		total_bytes = sizeof(struct tiler_allocp_info) + 2 * sizeof(void*) * count;
		if (total_bytes <= count || ctp->info.msglen - sizeof(msg->i) < total_bytes) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		allocptr_info = (void **)((sizeof(void*) * count) + (char *)(sysptr_info));
		tiler_alloc_packed(&info->count, info->fmt, info->width, info->height,
				sysptr_info, allocptr_info, info->aligned, ocb->pi);
		SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o) + sizeof(struct tiler_allocp_info));
		SETIOV(&ctp->iov[1], sysptr_info, sizeof(void*) * count);
		SETIOV(&ctp->iov[2], allocptr_info, sizeof(void*) * count);
		return _RESMGR_NPARTS(3);
		break;
	}
	case TILIOC_USRGPNV12OPT:
	{
		struct tiler_allocpnv12_info *info = (struct tiler_allocpnv12_info *)_DEVCTL_DATA(msg->i);
		int array_size = 0;
		int width = 0;
		int count = 0;
		int i = 0, j = 0;
		int bufsperpage = 0;
		void **y_sysptr_info = (void **)(info+1);
		void **uv_sysptr_info = (void **)(NULL);
		void **y_allocptr_info = (void **)(NULL);
		void **uv_allocptr_info = (void **)(NULL);
		uint32_t total_bytes = 0;

		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_allocpnv12_info)) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		array_size = info->count;
		width = info->width;
		count = info->count;
		total_bytes = sizeof(struct tiler_allocpnv12_info) + 4 * sizeof(void*) * array_size;
		if (total_bytes <= array_size || ctp->info.msglen - sizeof(msg->i) < total_bytes) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		uv_sysptr_info = (void **)((sizeof(void*) * array_size) + (char *)(y_sysptr_info));
		y_allocptr_info = (void **)((sizeof(void*) * array_size) + (char *)(uv_sysptr_info));
		uv_allocptr_info = (void **)((sizeof(void*) * array_size) + (char *)(y_allocptr_info));
		if (info->width != 0) {
			bufsperpage = PAGE_SIZE / info->width;
			if (bufsperpage > 1 && info->count > 1) {
				/* we can do some creative packing */
				count = (info->count + bufsperpage - 1) / bufsperpage;
				width *= MIN(bufsperpage, info->count);
				info->count = count;
			}
		}
		tiler_alloc_packed_nv12(&info->count, width, info->height,
				y_sysptr_info, uv_sysptr_info,
				y_allocptr_info, uv_allocptr_info,
				info->aligned, ocb->pi);
		if (width > info->width) {
			void *ysys_info[info->count];
			void *uvsys_info[info->count];
			memcpy(ysys_info, y_sysptr_info, info->count * sizeof(void*));
			memcpy(uvsys_info, uv_sysptr_info, info->count * sizeof(void*));
			info->count = MIN(info->count * bufsperpage, array_size);
			for (i = 0; i < count; i++) {
				for (j = 0; j < bufsperpage && (i * bufsperpage + j) < info->count; j++) {
					y_sysptr_info[(i * bufsperpage) + j] = ysys_info[i] + (info->width * j);
					uv_sysptr_info[(i * bufsperpage) + j] = uvsys_info[i] + (info->width * j);
				}
			}
		}
		SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o) + sizeof(struct tiler_allocpnv12_info));
		SETIOV(&ctp->iov[1], y_sysptr_info, sizeof(void*) * array_size);
		SETIOV(&ctp->iov[2], uv_sysptr_info, sizeof(void*) * array_size);
		SETIOV(&ctp->iov[3], y_allocptr_info, sizeof(void*) * array_size);
		SETIOV(&ctp->iov[4], uv_allocptr_info, sizeof(void*) * array_size);
		return _RESMGR_NPARTS(5);
		break;
	}
	case TILIOC_USRGPNV12:
	{
		struct tiler_allocpnv12_info *info = (struct tiler_allocpnv12_info *)_DEVCTL_DATA(msg->i);
		int count = 0;
		void **y_sysptr_info = (void **)(info+1);
		void **uv_sysptr_info = (void **)(NULL);
		void **y_allocptr_info = (void **)(NULL);
		void **uv_allocptr_info = (void **)(NULL);
		uint32_t total_bytes = 0;

		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_allocpnv12_info)) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		count = info->count;
		total_bytes = sizeof(struct tiler_allocpnv12_info) + 4 * sizeof(void*) * count;
		if (total_bytes <= count || ctp->info.msglen - sizeof(msg->i) < total_bytes) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		uv_sysptr_info = (void **)((sizeof(void*) * count) + (char *)(y_sysptr_info));
		y_allocptr_info = (void **)((sizeof(void*) * count) + (char *)(uv_sysptr_info));
		uv_allocptr_info = (void **)((sizeof(void*) * count) + (char *)(y_allocptr_info));
		tiler_alloc_packed_nv12(&info->count, info->width, info->height,
				y_sysptr_info, uv_sysptr_info,
				y_allocptr_info, uv_allocptr_info,
				info->aligned, ocb->pi);
		SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o) + sizeof(struct tiler_allocpnv12_info));
		SETIOV(&ctp->iov[1], y_sysptr_info, sizeof(void*) * count);
		SETIOV(&ctp->iov[2], uv_sysptr_info, sizeof(void*) * count);
		SETIOV(&ctp->iov[3], y_allocptr_info, sizeof(void*) * count);
		SETIOV(&ctp->iov[4], uv_allocptr_info, sizeof(void*) * count);
		return _RESMGR_NPARTS(5);
		break;
	}
	case TILIOC_USRROA:
	{
		struct tiler_reorient_info *info = (struct tiler_reorient_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_reorient_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_reorient_addr(info->tsptr, info->orient);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_reorient_info)));
		break;
	}
	case TILIOC_USRGNA:
	{
		u32 *nat_addr = (u32 *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(u32)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_get_natural_addr((void *)*nat_addr);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o)));
		break;
	}
	case TILIOC_USRROTL:
	{
		struct tiler_reorient_tl_info *info = (struct tiler_reorient_tl_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_reorient_tl_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_reorient_topleft(info->tsptr, info->orient,
				info->width, info->height);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(struct tiler_reorient_tl_info)));
		break;
	}
	case TILIOC_USRSTRIDE:
	{
		u32 *stride_val = (u32 *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(u32)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_stride(*stride_val);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(u32)));
		break;
	}
	case TILIOC_USRRV:
	{
		struct tiler_rotate_view_info *info = (struct tiler_rotate_view_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(struct tiler_rotate_view_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		tiler_rotate_view(&info->orient, info->rotation);
		msg->o.ret_val = 0;
		return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + sizeof(struct tiler_rotate_view_info)));
		break;
	}
	case TILIOC_REGNOTIFY:
	{
		struct tiler_regnotify_info *info = (struct tiler_regnotify_info *)_DEVCTL_DATA(msg->i);
		char *name = (char *)(info + 1);
		struct list_head *elem, *tmp;
		int count = 0;
		struct reg_notif_usr_info *pvtInfo = NULL;

		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_regnotify_info)) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		if (info->name_len > TILER_REGNOTIFY_MAX_NAME_LEN) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_regnotify_info) + info->name_len) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}

		/* Check if this process has already registered the max number of callbacks */
		list_for_each_safe(elem, tmp, &reg_notif_usr) {
			pvtInfo = (struct reg_notif_usr_info *)elem;
			if (pvtInfo->pid == pi->pid) {
				count++;
			}
		}
		if (count >= TILER_REGNOTIFY_MAX_REGISTERED) {
			msg->o.ret_val = -EINVAL;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}

		pvtInfo = NULL;
		pvtInfo = (struct reg_notif_usr_info *)kmalloc(sizeof(*pvtInfo), GFP_KERNEL);
		if (pvtInfo) {
			pvtInfo->cmd = info->cmd;
			pvtInfo->name_len = info->name_len;
			pvtInfo->pid = pi->pid;
			pvtInfo->name = (char *)kmalloc(sizeof(char) * info->name_len + 1, GFP_KERNEL);
			if (pvtInfo->name) {
				memset (pvtInfo->name, 0, info->name_len + 1);
				strncpy (pvtInfo->name, name, info->name_len);
				list_add(pvtInfo, &reg_notif_usr);
				msg->o.ret_val = tiler_reg_notifier(&tiler_notify_usr, pvtInfo);
			}
			else {
				msg->o.ret_val = -ENOMEM;
			}
		}
		else {
			msg->o.ret_val = -ENOMEM;
		}
		if (msg->o.ret_val < 0) {
			if (pvtInfo) {
				if (pvtInfo->name) {
					list_del(pvtInfo);
					kfree (pvtInfo->name);
				}
				kfree (pvtInfo);
			}
		}
		SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o) + sizeof(struct tiler_regnotify_info));
		return _RESMGR_NPARTS(1);
		break;
	}
	case TILIOC_UNREGNOTIFY:
	{
		struct tiler_regnotify_info *info = (struct tiler_regnotify_info *)_DEVCTL_DATA(msg->i);
		char *name = (char *)(info + 1);
		struct list_head *elem, *tmp;
		struct reg_notif_usr_info *pvtInfo = NULL;

		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_regnotify_info)) {
			msg->o.ret_val = -ENOMEM;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}
		if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct tiler_regnotify_info) + info->name_len) {
			msg->o.ret_val = -ENOMEM;
			SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o));
			return _RESMGR_NPARTS(1);
		}

		list_for_each_safe(elem, tmp, &reg_notif_usr) {
			pvtInfo = (struct reg_notif_usr_info *)elem;
			if (pvtInfo->name_len == info->name_len &&
				pvtInfo->cmd == info->cmd &&
				!strncmp(pvtInfo->name, name, pvtInfo->name_len)) {
				break;
			}
		}
		if (elem != &reg_notif_usr) {
			msg->o.ret_val = tiler_unreg_notifier(&tiler_notify_usr, pvtInfo);
			list_del(elem);
			kfree(pvtInfo->name);
			kfree(pvtInfo);
		}
		else
			msg->o.ret_val = -EINVAL;
		SETIOV(&ctp->iov[0], &msg->o, sizeof(msg->o) + sizeof(struct tiler_regnotify_info));
		return _RESMGR_NPARTS(1);
		break;
	}
	case TILIOC_USRGB:
	{
		struct tiler_alloc_block_area_info *info =
			(struct tiler_alloc_block_area_info *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) <
				sizeof (struct tiler_alloc_block_area_info)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_alloc_block_area(info->fmt,
					info->width, info->height,
					info->gid, (void *)pi,
					&info->sys_addr,
					&info->num_pages);
		return(_RESMGR_PTR(ctp,  &msg->o,
				sizeof(msg->o) + sizeof(struct tiler_alloc_block_area_info)));
		break;
	}
	case TILIOC_USRMB:
	{
		struct tiler_map_block_info *info =
				(struct tiler_map_block_info *)_DEVCTL_DATA(msg->i);
		u32 *pa = (u32 *)(info+1);
		uint32_t total_bytes = 0;
		s32 ret = 0;

		if (ctp->info.msglen - sizeof(msg->i) <
				sizeof(struct tiler_map_block_info)) {
			return _RESMGR_ERRNO(EINVAL);
		}
		total_bytes = sizeof(struct tiler_map_block_info) +
						sizeof(u32) * info->num_pages;
		if (ctp->info.msglen - sizeof(msg->i) < total_bytes) {
			return _RESMGR_ERRNO(EINVAL);
		}
		ret = tiler_map_block(info->sys_addr, info->num_pages, pa);
		if (ret)
			return _RESMGR_ERRNO(-ret);
		else
			return _RESMGR_ERRNO(EOK);
		break;
	}
	case TILIOC_USRUMB:
	{
		u32 *unmap_addr = (u32 *)_DEVCTL_DATA(msg->i);
		if (ctp->info.msglen - sizeof(msg->i) < sizeof(u32)) {
			msg->o.ret_val = -EINVAL;
			return(_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
		}
		msg->o.ret_val = tiler_unmap_block(*unmap_addr);
		return(_RESMGR_PTR(ctp,  &msg->o, sizeof(msg->o) + sizeof(u32)));
		break;
	}
	default:
		return (ENOSYS);
	}
	_IO_SET_WRITE_NBYTES (ctp, nbytes);
	return (_RESMGR_NPARTS(0));
}

static s32 alloc_block(enum tiler_fmt fmt, u32 width, u32 height,
		   u32 align, u32 offs, u32 gid, struct process_info *pi,
		   u32 *sys_addr)
{
	struct til_mem_info *mi = NULL;
	struct gid_info *gi = NULL;

	/* only support up to page alignment */
	if (align > PAGE_SIZE || offs > align || !pi)
		return -EINVAL;

	/* get group context */
	mutex_lock(&mtx);
	gi = _m_get_gi(pi, gid);
	mutex_unlock(&mtx);

	if (!gi)
		return -ENOMEM;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, align, offs, gi);
	if (!mi) {
		mutex_lock(&mtx);
		_m_try_free_group(gi);
		mutex_unlock(&mtx);
		return -ENOMEM;
	}

	*sys_addr = mi->sys_addr;

	/* allocate and map if mapping is supported */
	if (tmm_can_map(TMM(fmt))) {
		mi->num_pg = tcm_sizeof(mi->area);

		mi->mem = tmm_get(TMM(fmt), mi->num_pg);
		if (!mi->mem)
			goto cleanup;

		/* Ensure the data reaches to main memory before PAT refill */
		wmb();

		/* program PAT */
		mutex_lock(&mtx);
		if (refill_pat(TMM(fmt), &mi->area, mi->mem, fmt)) {
			mutex_unlock(&mtx);
			goto cleanup;
		}
		mutex_unlock(&mtx);
	}
	return 0;

cleanup:
	mutex_lock(&mtx);
	_m_free(mi);
	mutex_unlock(&mtx);
	return -ENOMEM;

}

s32 tiler_allocx(enum tiler_fmt fmt, u32 width, u32 height,
		 u32 align, u32 offs, u32 gid, void *pi, u32 *sys_addr)
{
	return alloc_block(fmt, width, height, align, offs, gid,
				(struct process_info *)pi, sys_addr);
}

s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr)
{
	return tiler_allocx(fmt, width, height, 0, 0,
				0, NULL, sys_addr);
}

static s32 alloc_block_area(enum tiler_fmt fmt, u32 width, u32 height,
		   u32 align, u32 offs, u32 gid, struct process_info *pi,
		   u32 *sys_addr, u32 *num_pages)
{
	struct til_mem_info *mi = NULL;
	struct gid_info *gi = NULL;

	/* only support up to page alignment */
	if (align > PAGE_SIZE || offs > align || !pi)
		return -EINVAL;

	/* get group context */
	mutex_lock(&mtx);
	gi = _m_get_gi(pi, gid);
	mutex_unlock(&mtx);

	if (!gi)
		return -ENOMEM;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, align, offs, gi);
	if (!mi) {
		mutex_lock(&mtx);
		_m_try_free_group(gi);
		mutex_unlock(&mtx);
		return -ENOMEM;
	}

	*sys_addr = mi->sys_addr;
	*num_pages = tcm_sizeof(mi->area);

	return 0;
}

static void reserve_nv12_blocks(u32 n, u32 width, u32 height,
				  u32 align, u32 offs, u32 gid, pid_t pid)
{
}

static void reserve_blocks(u32 n, enum tiler_fmt fmt, u32 width, u32 height,
				u32 align, u32 offs, u32 gid, pid_t pid)
{
}

/* reserve area for n identical buffers */
s32 tiler_reservex(u32 n, struct tiler_buf_info *b, pid_t pid)
{
	u32 i;

	if (b->num_blocks > TILER_MAX_NUM_BLOCKS ||	b->num_blocks < 0)
		return -EINVAL;

	for (i = 0; i < b->num_blocks; i++) {
		/* check for NV12 reservations */
		if (i + 1 < b->num_blocks &&
			b->blocks[i].fmt == TILFMT_8BIT &&
			b->blocks[i + 1].fmt == TILFMT_16BIT &&
			b->blocks[i].dim.area.height ==
			b->blocks[i + 1].dim.area.height &&
			b->blocks[i].dim.area.width ==
			b->blocks[i + 1].dim.area.width) {
			reserve_nv12_blocks(n,
						b->blocks[i].dim.area.width,
						b->blocks[i].dim.area.height,
						0, /* align */
						0, /* offs */
						0, /* gid */
						pid);
			i++;
		} else if (b->blocks[i].fmt >= TILFMT_8BIT &&
			   b->blocks[i].fmt <= TILFMT_32BIT) {
			/* other 2D reservations */
			reserve_blocks(n,
					 b->blocks[i].fmt,
					 b->blocks[i].dim.area.width,
					 b->blocks[i].dim.area.height,
					 0, /* align */
					 0, /* offs */
					 0, /* gid */
					 pid);
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

s32 tiler_reserve(u32 n, struct tiler_buf_info *b)
{
	return tiler_reservex(n, b, getgid());
}

static int tiler_notify_usr(int event_type, void *data, void *arg)
{
	struct reg_notif_usr_info *info = (struct reg_notif_usr_info *)arg;
	struct tiler_regnotify_response response;
	int fd = -1;
	int ret = 0;
	fd = open(info->name, O_SYNC | O_RDWR);
	if (fd < 0)
		return -EINVAL;
	response.event = event_type;
	response.pid = (pid_t)data;
	ret = ioctl(fd, info->cmd, (&response));
	close(fd);
	return ret;
}

int tiler_reg_notifier(tiler_notifier_cb cb_ptr, void * arg)
{
	struct reg_notif_info *info = NULL;

	if (!cb_ptr || !arg)
		return -EINVAL;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->cb_ptr = cb_ptr;
	info->arg = arg;

	list_add(info, &reg_notif);
	return EOK;
}

int tiler_unreg_notifier(tiler_notifier_cb cb_ptr, void * arg)
{
	struct list_head *elem, *tmp;
	struct reg_notif_info *info;

	if (!cb_ptr || !arg)
		return -EINVAL;

	list_for_each_safe(elem, tmp, &reg_notif) {
		info = (struct reg_notif_info *)elem;
		if (info->cb_ptr == cb_ptr && info->arg == arg) {
			list_del(elem);
			kfree(info);
			return EOK;
		}
	}
	return -EINVAL;
}

void tiler_purge(void)
{
	int i;
	mutex_lock(&mtx);
	for (i = TILFMT_8BIT; i <= TILFMT_MAX; i++) {
		tmm_purge(TMM(i));
	}
	mutex_unlock(&mtx);
}

s32 tiler_alloc_block_area(enum tiler_fmt fmt, u32 width, u32 height,
				u32 gid, void *pi,
				u32 *sys_addr, u32 *num_pages)
{
	return alloc_block_area(fmt, width, height, 0, 0, gid,
				(struct process_info *)pi, sys_addr, num_pages);
}

s32 tiler_map_block(u32 sys_addr, u32 num_pages, u32 *pages)
{
	struct til_mem_info *mi, *mi_tmp;
	s32 res = -ENOMEM;

	mutex_lock(&mtx);

	/* find block in global list */
	list_for_each_entry_safe(mi, mi_tmp, &blocks, global) {
		if (mi->sys_addr == sys_addr) {
			break;
		}
	}
	mutex_unlock(&mtx);

	if (mi == NULL) {
		res = -EINVAL;
		goto done;
	}

	/* map if mapping is supported and not already mapped */
	if (tmm_can_map(TMM(TILER_GET_ACC_MODE(mi->sys_addr))) && mi->pg_ptr == NULL) {
		mi->num_pg = tcm_sizeof(mi->area);
		if (num_pages != mi->num_pg) {
			res = -EINVAL;
			goto done;
		}

		mi->pg_ptr = kmalloc(mi->num_pg * sizeof(*mi->pg_ptr), GFP_KERNEL);
		if (!mi->pg_ptr)
			goto cleanup;
		memset(mi->pg_ptr, 0x0, sizeof(*mi->pg_ptr) * mi->num_pg);
		memcpy(mi->pg_ptr, pages, sizeof(*mi->pg_ptr) * mi->num_pg);

		/* Ensure the data reaches to main memory before PAT refill */
		wmb();

		/* program PAT */
		mutex_lock(&mtx);
		if (refill_pat(TMM_SS(mi->sys_addr), &mi->area, mi->pg_ptr,
					TILER_GET_ACC_MODE(mi->sys_addr))) {
			mutex_unlock(&mtx);
			goto cleanup;
		}
		mutex_unlock(&mtx);
	}
	else {
	    res = -EPERM;
	    goto done;
	}
	res = 0;
	goto done;
cleanup:
	if (mi->pg_ptr) {
		kfree(mi->pg_ptr);
		mi->pg_ptr = NULL;
	}
done:
	return res;
}

s32 tiler_unmap_block(u32 sys_addr)
{
	struct til_mem_info *mi, *mi_tmp;
	struct area_info *ai = NULL;
	s32 res = -ENOMEM;

	mutex_lock(&mtx);

	/* find block in global list and free it */
	list_for_each_entry_safe(mi, mi_tmp, &blocks, global) {
		if (mi->sys_addr == sys_addr) {
			break;
		}
	}
	mutex_unlock(&mtx);

	if (mi == NULL) {
		res = -EINVAL;
		goto done;
	}

	if (mi->pg_ptr == NULL) {
		res = -EINVAL;
		goto done;
	}

	/* allocate and map if mapping is supported */
	if (tmm_can_map(TMM(TILER_GET_ACC_MODE(mi->sys_addr))) && mi->pg_ptr) {
		kfree(mi->pg_ptr);
		mi->pg_ptr = NULL;
		if (mi->area.is2d) {
			ai = mi->parent;
			mutex_lock(&mtx);
			clear_pat(TMM_SS(mi->sys_addr), &ai->area, TILER_GET_ACC_MODE(mi->sys_addr));
			mutex_unlock(&mtx);
		}
		else {
			mutex_lock(&mtx);
			clear_pat(TMM_SS(mi->sys_addr), &mi->area, TILFMT_PAGE);
			mutex_unlock(&mtx);
		}
	}
	else {
	    res = -EPERM;
	    goto done;
	}
	res = 0;
done:
	return res;
}

void tiler_exit(void)
{
	struct process_info *pi = NULL, *pi_ = NULL;
	int i, j;

	mutex_lock(&mtx);

	/* free all process data */
	list_for_each_entry_safe(pi, pi_, &procs, list)
		_m_free_process_info(pi);

	/* all lists should have cleared */
	//WARN_ON(!list_empty(&blocks));
	//WARN_ON(!list_empty(&procs));
	//WARN_ON(!list_empty(&orphan_onedim));
	//WARN_ON(!list_empty(&orphan_areas));

	mutex_unlock(&mtx);

	munmap(dmac_va, TILER_WIDTH * TILER_HEIGHT *
					sizeof(*dmac_va));

	dlclose (tiler_pat_lib);

	/* close containers only once */
	for (i = TILFMT_8BIT; i <= TILFMT_MAX; i++) {
		/* remove identical containers (tmm is unique per tcm) */
		for (j = i + 1; j <= TILFMT_MAX; j++)
			if (TCM(i) == TCM(j)) {
				TCM_SET(j, NULL);
				TMM_SET(j, NULL);
			}

		tcm_deinit(TCM(i));
		tmm_deinit(TMM(i));
	}

	mutex_destroy(&mtx);

	munmap(dummy_va, PAGE_SIZE);
}

tiler_ocb_t *
ocb_calloc (resmgr_context_t * ctp, IOFUNC_ATTR_T * device)
{
	tiler_ocb_t *ocb;
	struct process_info *pi;

	/* Allocate the OCB */
	ocb = (tiler_ocb_t *) calloc (1, sizeof (tiler_ocb_t));
	if (ocb == NULL){
		errno = ENOMEM;
		return (NULL);
	}

	pi = __get_pi(ctp->info.pid, false);

	if (!pi) {
		errno = ENOMEM;
		return (NULL);
	}

	ocb->pi = pi;

	return (ocb);
}

void
ocb_free (tiler_ocb_t * ocb)
{
	struct process_info *pi = (struct process_info *)ocb->pi;

	mutex_lock(&mtx);
	/* free resources if last device in this process */
	if (0 == --pi->refs)
		_m_free_process_info(pi);

	mutex_unlock(&mtx);

	free (ocb);
}

s32 tiler_init(u32 size)
{
	s32 r = -1;
	struct tcm_pt div_pt;
	struct tcm *sita[MAX_CONTAINERS] = {0};
	struct tmm *tmm_pat[MAX_CONTAINERS] = {0};
	struct tcm_area area = {0};
	s32 ret = 0;
	u32 len = 0;
	s64 offset = 0;

	/**
	  * Array of physical pages for PAT programming, which must be a 16-byte
	  * aligned physical address
	*/
	dmac_va = mmap64(NULL, TILER_WIDTH * TILER_HEIGHT *
			sizeof(*dmac_va), PROT_NOCACHE | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PHYS | MAP_PRIVATE, NOFD, 0);
	if (dmac_va == MAP_FAILED) {
		return -ENOMEM;
	}

	ret = mem_offset64(dmac_va, NOFD, TILER_WIDTH * TILER_HEIGHT *
			sizeof(*dmac_va), &offset, &len);
	if (ret) {
		fprintf(stderr, "Tiler_init: Error from mem_offset [%d]\n", errno);
		return -ENOMEM;
	}
	else {
		dmac_pa = (u32)offset;
	}

	tiler_pat_lib = dlopen("tiler_pat.so", RTLD_NOW | RTLD_GLOBAL);
	if (tiler_pat_lib == NULL) {
		fprintf(stderr, "Tiler_init: Error opening shared lib [%d]\n", errno);
			    return -ENOMEM;
	}
	tiler_pat_init = dlsym(tiler_pat_lib, "tiler_pat_init");
	if (tiler_pat_init == NULL) {
		fprintf(stderr, "Tiler_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}
	(*tiler_pat_init)();
	tiler_save_pat = dlsym(tiler_pat_lib, "tiler_save_pat");
	if (tiler_save_pat == NULL) {
		fprintf(stderr, "Tiler_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}

	/* Allocate tiler container manager (we share 1 on OMAP4) */
	div_pt.x = TILER_WIDTH;   /* hardcoded default */
	div_pt.y = (3 * TILER_HEIGHT) / 4;
	sita[0] = sita_init(TILER_WIDTH, TILER_HEIGHT, (void *)&div_pt);

	/* Allocate tiler memory manager (must have 1 unique TMM per TCM ) */
	tmm_pat[0] = tmm_pat_init(0, size);

	if (!sita[0] || !tmm_pat[0]) {
		r = -ENOMEM;
		goto error;
	}

#ifdef TILER_PLATFORM_OMAP5
	div_pt.y = TILER_HEIGHT;
	sita[1] = sita_init(TILER_WIDTH, TILER_HEIGHT, (void *)&div_pt);
	tmm_pat[1] = tmm_pat_init(0, size);

	if (!sita[1] || !tmm_pat[1]) {
		r = -ENOMEM;
		goto error;
	}
#endif

	TCM_SET(TILFMT_8BIT, sita[0]);
	TCM_SET(TILFMT_16BIT, sita[0]);
	TCM_SET(TILFMT_32BIT, sita[0]);
#ifdef TILER_PLATFORM_OMAP5
	TCM_SET(TILFMT_PAGE, sita[1]);
#else
	TCM_SET(TILFMT_PAGE, sita[0]);
#endif

	TMM_SET(TILFMT_8BIT, tmm_pat[0]);
	TMM_SET(TILFMT_16BIT, tmm_pat[0]);
	TMM_SET(TILFMT_32BIT, tmm_pat[0]);
#ifdef TILER_PLATFORM_OMAP5
	TMM_SET(TILFMT_PAGE, tmm_pat[1]);
#else
	TMM_SET(TILFMT_PAGE, tmm_pat[0]);
#endif

	mutex_init(&mtx);
	INIT_LIST_HEAD(&blocks);
	INIT_LIST_HEAD(&procs);
	INIT_LIST_HEAD(&orphan_areas);
	INIT_LIST_HEAD(&orphan_onedim);
	INIT_LIST_HEAD(&reg_notif_usr);
	INIT_LIST_HEAD(&reg_notif);
	id = 0xda7a000;

	/* Dummy page for filling unused entries in dmm (dmac_va):
	 */
	dummy_va = mmap64(NULL, PAGE_SIZE,
			PROT_NOCACHE | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PHYS | MAP_PRIVATE, NOFD, 0);
	if (dummy_va == MAP_FAILED) {
		r = -ENOMEM;
		goto error;
	}

	ret = mem_offset64(dummy_va, NOFD, PAGE_SIZE, &offset, &len);
	if (ret) {
		fprintf(stderr, "Tiler_init: Error from mem_offset [%d]\n", errno);
		r = -ENOMEM;
		goto error;
	} else {
		dummy_pa = (u32)offset;
	}

	/* clear the entire dmm space:
	 */
	area.tcm = sita[0];
	area.p0.x = 0;
	area.p0.y = 0;
	area.p1.x = TILER_WIDTH - 1;
	area.p1.y = TILER_HEIGHT - 1;
	clear_pat(tmm_pat[0], &area, TILFMT_8BIT);
#ifdef TILER_PLATFORM_OMAP5
	area.tcm = sita[1];
	clear_pat(tmm_pat[1], &area, TILFMT_PAGE);
#endif
	r = 0;

error:
	/* TODO: error handling for device registration */
	if (r) {
#ifdef TILER_PLATFORM_OMAP5
		if (sita[1])
			tcm_deinit(sita[1]);
		if (tmm_pat[1])
			tmm_deinit(tmm_pat[1]);
#endif
		if (sita[0])
			tcm_deinit(sita[0]);
		if (tmm_pat[0])
			tmm_deinit(tmm_pat[0]);
		munmap(dmac_va, TILER_WIDTH * TILER_HEIGHT *
				sizeof(*dmac_va));
	}

	return r;
}
