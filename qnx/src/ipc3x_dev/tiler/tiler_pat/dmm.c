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
 * dmm.c
 *
 * DMM driver support functions for TI OMAP processors.
 */
#include "proto.h"
#include "dmm.h"

#define BITS_32(in_NbBits) ((((u32)1 << in_NbBits) - 1) | ((u32)1 << in_NbBits))
#define BITFIELD_32(in_UpBit, in_LowBit)\
	(BITS_32(in_UpBit) & ~((BITS_32(in_LowBit)) >> 1))
#define BF BITFIELD_32


/* IMPORTANT NOTE: This function needs to be IRQ safe.  Adding traces
 * or other non-interrupt safe calls to it will result in a crash in
 * CPU dll. */
s32 dmm_pat_refill(struct dmm *dmm, struct pat *pd, enum pat_mode mode)
{
	uintptr_t r = NULL;
	u32 v = -1, w = -1;

	/* Only manual refill supported */
	if (mode != MANUAL)
		return -EFAULT;

	/*
	 * Check that the DMM_PAT_STATUS register
	 * has not reported an error.
	*/
	r = (uintptr_t)((u32)dmm->base | DMM_PAT_STATUS__0);
	v = __raw_readl(r);
	if ((v & 0xFC00) != 0) {
		return -EFAULT;
	}

	/* Set "next" register to NULL */
	r = (uintptr_t)((u32)dmm->base | DMM_PAT_DESCR__0);
	v = __raw_readl(r);
	w = (v & (~(BF(31, 4)))) | ((((u32)NULL) << 4) & BF(31, 4));
	__raw_writel(w, r);

	/* Set area to be refilled */
	r = (uintptr_t)((u32)dmm->base | DMM_PAT_AREA__0);
	v = __raw_readl(r);
#ifdef TILER_PLATFORM_OMAP5
	w = (v & (~(BF(31, 24)))) | ((((s8)pd->area.y1) << 24) & BF(31, 24));
#else
	w = (v & (~(BF(30, 24)))) | ((((s8)pd->area.y1) << 24) & BF(30, 24));
#endif
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(23, 16)))) | ((((s8)pd->area.x1) << 16) & BF(23, 16));
	__raw_writel(w, r);

	v = __raw_readl(r);
#ifdef TILER_PLATFORM_OMAP5
	w = (v & (~(BF(15, 8)))) | ((((s8)pd->area.y0) << 8) & BF(15, 8));
#else
	w = (v & (~(BF(14, 8)))) | ((((s8)pd->area.y0) << 8) & BF(14, 8));
#endif
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(7, 0)))) | ((((s8)pd->area.x0) << 0) & BF(7, 0));
	__raw_writel(w, r);
	wmb();

	/* First, clear the DMM_PAT_IRQSTATUS register */
	r = (uintptr_t)((u32)dmm->base | (u32)DMM_PAT_IRQSTATUS);
	__raw_writel(0xFFFFFFFF, r);
	wmb();

	r = (uintptr_t)((u32)dmm->base | (u32)DMM_PAT_IRQSTATUS_RAW);
	v = 0xFFFFFFFF;

	while (v != 0x0) {
		v = __raw_readl(r);
	}

	/* Fill data register */
	r = (uintptr_t)((u32)dmm->base | DMM_PAT_DATA__0);
	v = __raw_readl(r);

	/* Apply 4 bit left shft to counter the 4 bit right shift */
	w = (v & (~(BF(31, 4)))) | ((((u32)(pd->data >> 4)) << 4) & BF(31, 4));
	__raw_writel(w, r);
	wmb();

	/* Read back PAT_DATA__0 to see if write was successful */
	v = 0x0;
	while (v != pd->data) {
		v = __raw_readl(r);
	}

	r = (uintptr_t)((u32)dmm->base | (u32)DMM_PAT_CTRL__0);
	v = __raw_readl(r);

	w = (v & (~(BF(31, 28)))) | ((((u32)pd->ctrl.ini) << 28) & BF(31, 28));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(16, 16)))) | ((((u32)pd->ctrl.sync) << 16) & BF(16, 16));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(9, 8)))) | ((((u32)pd->ctrl.lut_id) << 8) & BF(9, 8));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(6, 4)))) | ((((u32)pd->ctrl.dir) << 4) & BF(6, 4));
	__raw_writel(w, r);

	v = __raw_readl(r);
	w = (v & (~(BF(0, 0)))) | ((((u32)pd->ctrl.start) << 0) & BF(0, 0));
	__raw_writel(w, r);
	wmb();

	/*
	 * Now, check if PAT_IRQSTATUS_RAW has been
	 * set after the PAT has been refilled
	 */
	r = (uintptr_t)((u32)dmm->base | (u32)DMM_PAT_IRQSTATUS_RAW);
	v = 0x0;
	while ((v & 0x3) != 0x3) {
		v = __raw_readl(r);
	}

	/* Again, clear the DMM_PAT_IRQSTATUS register */
	r = (uintptr_t)((u32)dmm->base | (u32)DMM_PAT_IRQSTATUS);
	__raw_writel(0xFFFFFFFF, r);
	wmb();

	r = (uintptr_t)((u32)dmm->base | (u32)DMM_PAT_IRQSTATUS_RAW);
	v = 0xFFFFFFFF;

	while (v != 0x0) {
		v = __raw_readl(r);
	}

	/* Again, set "next" register to NULL to clear any PAT STATUS errors */
	r = (uintptr_t)((u32)dmm->base | DMM_PAT_DESCR__0);
	v = __raw_readl(r);
	w = (v & (~(BF(31, 4)))) | ((((u32)NULL) << 4) & BF(31, 4));
	__raw_writel(w, r);

	/*
	 * Now, check that the DMM_PAT_STATUS register
	 * has not reported an error before exiting.
	*/
	r = (uintptr_t)((u32)dmm->base | DMM_PAT_STATUS__0);
	v = __raw_readl(r);
	if ((v & 0xFC00) != 0) {
		return -EFAULT;
	}

	return 0;
}

struct dmm *dmm_pat_init(u32 id)
{
	u32 base = 0;
	struct dmm *dmm = NULL;
	switch (id) {
	case 0:
		/* only support id 0 for now */
		base = DMM_BASE;
		break;
	default:
		return NULL;
	}

	dmm = kmalloc(sizeof(*dmm), GFP_KERNEL);
	if (!dmm)
		return NULL;

	dmm->base = mmap_device_io(DMM_SIZE, base);
	if (!dmm->base) {
		kfree(dmm);
		return NULL;
	}

	__raw_writel(0x88888888, dmm->base + DMM_PAT_VIEW__0);
	__raw_writel(0x88888888, dmm->base + DMM_PAT_VIEW__1);
	__raw_writel(0x80808080, dmm->base + DMM_PAT_VIEW_MAP__0);
	__raw_writel(0x80000000, dmm->base + DMM_PAT_VIEW_MAP_BASE);
	__raw_writel(0x88888888, dmm->base + DMM_TILER_OR__0);
	__raw_writel(0x88888888, dmm->base + DMM_TILER_OR__1);

	/* Set PEG to zero for ISS */
	__raw_writel(0x8, dmm->base + DMM_PEG_PRIO + 0x4 * 2);

	return dmm;
}

/**
 * Clean up the physical address translator.
 * @param dmm    Device data
 * @return an error status.
 */
void dmm_pat_release(struct dmm *dmm)
{
	if (dmm) {
		munmap_device_io(dmm->base, DMM_SIZE);
		kfree(dmm);
	}
}

s32 dmm_init(void)
{
	return 0;
}

void dmm_exit(void)
{
	return;
}
