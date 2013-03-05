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
 * tiler_def.h
 *
 * TILER driver support functions for TI OMAP processors.
 */

#ifndef TILER_DEF_H
#define TILER_DEF_H

#define ROUND_UP_2P(a, b) (((a) + (b) - 1) & ~((b) - 1))
#define DIVIDE_UP(a, b) (((a) + (b) - 1) / (b))
#define ROUND_UP(a, b) (DIVIDE_UP(a, b) * (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define TILER_ACC_MODE_SHIFT  (27)
#define TILER_ACC_MODE_MASK   (3)
#define TILER_GET_ACC_MODE(x) ((enum tiler_fmt) (1 + \
(((u32)x & (TILER_ACC_MODE_MASK<<TILER_ACC_MODE_SHIFT))>>TILER_ACC_MODE_SHIFT)))

#define TILER_ALIAS_BASE    (0x60000000)
#define TILER_ACC_MODE_SHIFT  (27)
#define DMM_ACC_MODE_SHIFT  (27)

#define TIL_ALIAS_ADDR(x, access_mode)\
((void *)(TILER_ALIAS_BASE | (u32)x | (access_mode << TILER_ACC_MODE_SHIFT)))

#define TIL_ADDR(x, r, yi, xi, a)\
((void *)((u32)x | (r << DMM_ROTATION_SHIFT) |\
(yi << DMM_Y_INVERT_SHIFT) | (xi << DMM_X_INVERT_SHIFT) |\
(a << DMM_ACC_MODE_SHIFT)))

#define TILER_ALIAS_VIEW_CLEAR    (~0xE0000000)

#define DMM_X_INVERT_SHIFT        (29)
#define DMM_GET_X_INVERTED(x) ((((u32)x & (1<<DMM_X_INVERT_SHIFT)) > 0) ? 1 : 0)
#define DMM_Y_INVERT_SHIFT        (30)
#define DMM_GET_Y_INVERTED(x) ((((u32)x & (1<<DMM_Y_INVERT_SHIFT)) > 0) ? 1 : 0)

#define DMM_ROTATION_SHIFT        (31)
#define DMM_GET_ROTATED(x)\
((((u32)x & ((u32)1<<DMM_ROTATION_SHIFT)) > 0) ? 1 : 0)

#define DMM_ALIAS_VIEW_CLEAR    (~0xE0000000)

#define DMM_TILE_DIMM_X_MODE_8    (32)
#define DMM_TILE_DIMM_Y_MODE_8    (32)

#define DMM_TILE_DIMM_X_MODE_16    (32)
#define DMM_TILE_DIMM_Y_MODE_16    (16)

#define DMM_TILE_DIMM_X_MODE_32    (16)
#define DMM_TILE_DIMM_Y_MODE_32    (16)

#define DMM_PAGE_DIMM_X_MODE_8    (DMM_TILE_DIMM_X_MODE_8*2)
#define DMM_PAGE_DIMM_Y_MODE_8    (DMM_TILE_DIMM_Y_MODE_8*2)

#define DMM_PAGE_DIMM_X_MODE_16    (DMM_TILE_DIMM_X_MODE_16*2)
#define DMM_PAGE_DIMM_Y_MODE_16    (DMM_TILE_DIMM_Y_MODE_16*2)

#define DMM_PAGE_DIMM_X_MODE_32    (DMM_TILE_DIMM_X_MODE_32*2)
#define DMM_PAGE_DIMM_Y_MODE_32    (DMM_TILE_DIMM_Y_MODE_32*2)

#define DMM_HOR_X_ADDRSHIFT_8            (0)
#define DMM_HOR_X_ADDRMASK_8            (0x3FFF)
#define DMM_HOR_X_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_HOR_X_ADDRSHIFT_8) & DMM_HOR_X_ADDRMASK_8)
#define DMM_HOR_X_PAGE_COOR_GET_8(x)\
				(DMM_HOR_X_COOR_GET_8(x)/DMM_PAGE_DIMM_X_MODE_8)

#define DMM_HOR_Y_ADDRSHIFT_8            (14)
#define DMM_HOR_Y_ADDRMASK_8            (0x1FFF)
#define DMM_HOR_Y_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_HOR_Y_ADDRSHIFT_8) & DMM_HOR_Y_ADDRMASK_8)
#define DMM_HOR_Y_PAGE_COOR_GET_8(x)\
				(DMM_HOR_Y_COOR_GET_8(x)/DMM_PAGE_DIMM_Y_MODE_8)

#define DMM_HOR_X_ADDRSHIFT_16            (1)
#define DMM_HOR_X_ADDRMASK_16            (0x7FFE)
#define DMM_HOR_X_COOR_GET_16(x)        (((unsigned long)x >> \
				DMM_HOR_X_ADDRSHIFT_16) & DMM_HOR_X_ADDRMASK_16)
#define DMM_HOR_X_PAGE_COOR_GET_16(x)    (DMM_HOR_X_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_X_MODE_16)

#define DMM_HOR_Y_ADDRSHIFT_16            (15)
#define DMM_HOR_Y_ADDRMASK_16            (0xFFF)
#define DMM_HOR_Y_COOR_GET_16(x)        (((unsigned long)x >>                  \
				DMM_HOR_Y_ADDRSHIFT_16) & DMM_HOR_Y_ADDRMASK_16)
#define DMM_HOR_Y_PAGE_COOR_GET_16(x)    (DMM_HOR_Y_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_Y_MODE_16)

#define DMM_HOR_X_ADDRSHIFT_32            (2)
#define DMM_HOR_X_ADDRMASK_32            (0x7FFC)
#define DMM_HOR_X_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_HOR_X_ADDRSHIFT_32) & DMM_HOR_X_ADDRMASK_32)
#define DMM_HOR_X_PAGE_COOR_GET_32(x)    (DMM_HOR_X_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_X_MODE_32)

#define DMM_HOR_Y_ADDRSHIFT_32            (15)
#define DMM_HOR_Y_ADDRMASK_32            (0xFFF)
#define DMM_HOR_Y_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_HOR_Y_ADDRSHIFT_32) & DMM_HOR_Y_ADDRMASK_32)
#define DMM_HOR_Y_PAGE_COOR_GET_32(x)    (DMM_HOR_Y_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_Y_MODE_32)

#define DMM_VER_X_ADDRSHIFT_8            (14)
#define DMM_VER_X_ADDRMASK_8            (0x1FFF)
#define DMM_VER_X_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_VER_X_ADDRSHIFT_8) & DMM_VER_X_ADDRMASK_8)
#define DMM_VER_X_PAGE_COOR_GET_8(x)\
				(DMM_VER_X_COOR_GET_8(x)/DMM_PAGE_DIMM_X_MODE_8)

#define DMM_VER_Y_ADDRSHIFT_8            (0)
#define DMM_VER_Y_ADDRMASK_8            (0x3FFF)
#define DMM_VER_Y_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_VER_Y_ADDRSHIFT_8) & DMM_VER_Y_ADDRMASK_8)
#define DMM_VER_Y_PAGE_COOR_GET_8(x)\
				(DMM_VER_Y_COOR_GET_8(x)/DMM_PAGE_DIMM_Y_MODE_8)

#define DMM_VER_X_ADDRSHIFT_16            (14)
#define DMM_VER_X_ADDRMASK_16            (0x1FFF)
#define DMM_VER_X_COOR_GET_16(x)        (((unsigned long)x >>                  \
				DMM_VER_X_ADDRSHIFT_16) & DMM_VER_X_ADDRMASK_16)
#define DMM_VER_X_PAGE_COOR_GET_16(x)    (DMM_VER_X_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_X_MODE_16)

#define DMM_VER_Y_ADDRSHIFT_16            (0)
#define DMM_VER_Y_ADDRMASK_16            (0x3FFF)
#define DMM_VER_Y_COOR_GET_16(x)        (((unsigned long)x >>                  \
				DMM_VER_Y_ADDRSHIFT_16) & DMM_VER_Y_ADDRMASK_16)
#define DMM_VER_Y_PAGE_COOR_GET_16(x)    (DMM_VER_Y_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_Y_MODE_16)

#define DMM_VER_X_ADDRSHIFT_32            (15)
#define DMM_VER_X_ADDRMASK_32            (0xFFF)
#define DMM_VER_X_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_VER_X_ADDRSHIFT_32) & DMM_VER_X_ADDRMASK_32)
#define DMM_VER_X_PAGE_COOR_GET_32(x)    (DMM_VER_X_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_X_MODE_32)

#define DMM_VER_Y_ADDRSHIFT_32            (0)
#define DMM_VER_Y_ADDRMASK_32            (0x7FFF)
#define DMM_VER_Y_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_VER_Y_ADDRSHIFT_32) & DMM_VER_Y_ADDRMASK_32)
#define DMM_VER_Y_PAGE_COOR_GET_32(x)    (DMM_VER_Y_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_Y_MODE_32)

#endif
