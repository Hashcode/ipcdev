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
 * tiler_rot.c
 *
 * TILER driver support functions for TI OMAP processors.
 */
#include "proto.h"
#include "tiler.h"
#include "tiler_def.h"

#define DMM_SHIFT_PER_X_8 0
#define DMM_SHIFT_PER_Y_8 0
#define DMM_SHIFT_PER_X_16 0
#define DMM_SHIFT_PER_Y_16 1
#define DMM_SHIFT_PER_X_32 1
#define DMM_SHIFT_PER_Y_32 1
#define DMM_SHIFT_PER_X_PAGE 6
#define DMM_SHIFT_PER_Y_PAGE 6

#define DMM_TILER_THE(NAME) (1 << DMM_TILER_##NAME##_BITS)
#define DMM_TILER_THE_(N, NAME) (1 << DMM_TILER_##NAME##_BITS_(N))

#define DMM_TILER_CONT_WIDTH_BITS  14
#define DMM_TILER_CONT_HEIGHT_BITS 13

#define DMM_SHIFT_PER_P_(N) (DMM_SHIFT_PER_X_##N + DMM_SHIFT_PER_Y_##N)

#define DMM_TILER_CONT_HEIGHT_BITS_(N) \
	(DMM_TILER_CONT_HEIGHT_BITS - DMM_SHIFT_PER_Y_##N)
#define DMM_TILER_CONT_WIDTH_BITS_(N) \
	(DMM_TILER_CONT_WIDTH_BITS - DMM_SHIFT_PER_X_##N)

#define DMM_TILER_MASK(bits) ((1 << (bits)) - 1)

#define DMM_TILER_GET_OFFSET_(N, var) \
	((((u32) var) & DMM_TILER_MASK(DMM_TILER_CONT_WIDTH_BITS + \
	DMM_TILER_CONT_HEIGHT_BITS)) >> DMM_SHIFT_PER_P_(N))

#define DMM_TILER_GET_0_X_(N, var) \
	(DMM_TILER_GET_OFFSET_(N, var) & \
	DMM_TILER_MASK(DMM_TILER_CONT_WIDTH_BITS_(N)))
#define DMM_TILER_GET_0_Y_(N, var) \
	(DMM_TILER_GET_OFFSET_(N, var) >> DMM_TILER_CONT_WIDTH_BITS_(N))
#define DMM_TILER_GET_90_X_(N, var) \
	(DMM_TILER_GET_OFFSET_(N, var) & \
	DMM_TILER_MASK(DMM_TILER_CONT_HEIGHT_BITS_(N)))
#define DMM_TILER_GET_90_Y_(N, var) \
	(DMM_TILER_GET_OFFSET_(N, var) >> DMM_TILER_CONT_HEIGHT_BITS_(N))

#define DMM_TILER_STRIDE_0_(N) \
	(DMM_TILER_THE(CONT_WIDTH) << DMM_SHIFT_PER_Y_##N)
#define DMM_TILER_STRIDE_90_(N) \
	(DMM_TILER_THE(CONT_HEIGHT) << DMM_SHIFT_PER_X_##N)

void tiler_get_natural_xy(u32 tsptr, u32 *x, u32 *y)
{
	u32 x_bits, y_bits, offset;
	enum tiler_fmt fmt;

	fmt = TILER_GET_ACC_MODE(tsptr);

	switch (fmt) {
	case TILFMT_8BIT:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(8);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(8);
		offset = DMM_TILER_GET_OFFSET_(8, tsptr);
		break;
	case TILFMT_16BIT:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(16);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(16);
		offset = DMM_TILER_GET_OFFSET_(16, tsptr);
		break;
	case TILFMT_32BIT:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(32);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(32);
		offset = DMM_TILER_GET_OFFSET_(32, tsptr);
		break;
	case TILFMT_PAGE:
	default:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(PAGE);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(PAGE);
		offset = DMM_TILER_GET_OFFSET_(PAGE, tsptr);
		break;
	}

	if (DMM_GET_ROTATED(tsptr)) {
		*x = offset >> y_bits;
		*y = offset & DMM_TILER_MASK(y_bits);
	} else {
		*x = offset & DMM_TILER_MASK(x_bits);
		*y = offset >> x_bits;
	}

	if (DMM_GET_X_INVERTED(tsptr))
		*x ^= DMM_TILER_MASK(x_bits);
	if (DMM_GET_Y_INVERTED(tsptr))
		*y ^= DMM_TILER_MASK(y_bits);
}

u32 tiler_get_address(struct tiler_view_orient orient,
			enum tiler_fmt fmt, u32 x, u32 y)
{
	u32 x_bits, y_bits, tmp, x_mask, y_mask, alignment;

	switch (fmt) {
	case TILFMT_8BIT:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(8);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(8);
		alignment = DMM_SHIFT_PER_P_(8);
		break;
	case TILFMT_16BIT:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(16);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(16);
		alignment = DMM_SHIFT_PER_P_(16);
		break;
	case TILFMT_32BIT:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(32);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(32);
		alignment = DMM_SHIFT_PER_P_(32);
		break;
	case TILFMT_PAGE:
	default:
		x_bits = DMM_TILER_CONT_WIDTH_BITS_(PAGE);
		y_bits = DMM_TILER_CONT_HEIGHT_BITS_(PAGE);
		alignment = DMM_SHIFT_PER_P_(PAGE);
		break;
	}

	x_mask = DMM_TILER_MASK(x_bits);
	y_mask = DMM_TILER_MASK(y_bits);
	if (x < 0 || x > x_mask || y < 0 || y > y_mask)
		return 0;

	if (orient.x_invert)
		x ^= x_mask;
	if (orient.y_invert)
		y ^= y_mask;

	if (orient.rotate_90)
		tmp = ((x << y_bits) + y);
	else
		tmp = ((y << x_bits) + x);

	return (u32)
		TIL_ADDR((tmp << alignment), (orient.rotate_90 ? 1 : 0),
			(orient.y_invert ? 1 : 0), (orient.x_invert ? 1 : 0),
			(fmt - 1));
}

u32 tiler_reorient_addr(u32 tsptr, struct tiler_view_orient orient)
{
	u32 x, y;

	tiler_get_natural_xy(tsptr, &x, &y);
	return tiler_get_address(orient, TILER_GET_ACC_MODE(tsptr), x, y);
}

u32 tiler_get_natural_addr(void *sys_ptr)
{
	return (u32)sys_ptr & DMM_ALIAS_VIEW_CLEAR;
}

u32 tiler_reorient_topleft(u32 tsptr, struct tiler_view_orient orient,
				u32 width, u32 height)
{
	enum tiler_fmt fmt;
	u32 x, y;

	fmt = TILER_GET_ACC_MODE(tsptr);

	tiler_get_natural_xy(tsptr, &x, &y);

	if (DMM_GET_X_INVERTED(tsptr))
		x -= width - 1;
	if (DMM_GET_Y_INVERTED(tsptr))
		y -= height - 1;

	if (orient.x_invert)
		x += width - 1;
	if (orient.y_invert)
		y += height - 1;

	return tiler_get_address(orient, fmt, x, y);
}

u32 tiler_stride(u32 tsptr)
{
	enum tiler_fmt fmt;

	fmt = TILER_GET_ACC_MODE(tsptr);

	switch (fmt) {
	case TILFMT_8BIT:
		return DMM_GET_ROTATED(tsptr) ?
			DMM_TILER_STRIDE_90_(8) : DMM_TILER_STRIDE_0_(8);
	case TILFMT_16BIT:
		return DMM_GET_ROTATED(tsptr) ?
			DMM_TILER_STRIDE_90_(16) : DMM_TILER_STRIDE_0_(16);
	case TILFMT_32BIT:
		return DMM_GET_ROTATED(tsptr) ?
			DMM_TILER_STRIDE_90_(32) : DMM_TILER_STRIDE_0_(32);
	default:
		return 0;
	}
}

void tiler_rotate_view(struct tiler_view_orient *orient, u32 rotation)
{
	rotation = (rotation / 90) & 3;

	if (rotation & 2) {
		orient->x_invert = !orient->x_invert;
		orient->y_invert = !orient->y_invert;
	}

	if (rotation & 1) {
		if (orient->rotate_90)
			orient->y_invert = !orient->y_invert;
		else
			orient->x_invert = !orient->x_invert;
		orient->rotate_90 = !orient->rotate_90;
	}
}
