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
 * _tcm_sita.h
 *
 * SImple Tiler Allocator (SiTA) private structures.
 *
 * Author: Ravi Ramachandra <r.ramachandra@ti.com>
 */

#ifndef _TCM_SITA_H_
#define _TCM_SITA_H_

#include "proto.h"
#include "tcm.h"

#define TL_CORNER       0
#define TR_CORNER       1
#define BL_CORNER       3
#define BR_CORNER       4

/*Provide inclusive length between co-ordinates */
#define INCL_LEN(high, low)		((high) - (low) + 1)
#define INCL_LEN_MOD(start, end)   ((start) > (end) ? (start) - (end) + 1 : \
		(end) - (start) + 1)

#define BOUNDARY(stat) ((stat)->top_boundary + (stat)->bottom_boundary + \
				(stat)->left_boundary + (stat)->right_boundary)
#define OCCUPIED(stat) ((stat)->top_occupied + (stat)->bottom_occupied + \
				(stat)->left_occupied + (stat)->right_occupied)

enum Criteria {
	CR_MAX_NEIGHS       = 0x01,
	CR_FIRST_FOUND      = 0x10,
	CR_BIAS_HORIZONTAL  = 0x20,
	CR_BIAS_VERTICAL    = 0x40,
	CR_DIAGONAL_BALANCE = 0x80
};

struct nearness_factor {
	s32 x;
	s32 y;
};

/*
 * Area info kept
 */
struct area_spec {
	struct tcm_area area;
	struct list_head list;
};

/*
 * Everything is a rectangle with four sides and on
 * each side you could have a boundary or another Tile.
 * The tile could be Occupied or Not. These info is stored
 */
struct neighbour_stats {
	u16 left_boundary;
	u16 left_occupied;
	u16 top_boundary;
	u16 top_occupied;
	u16 right_boundary;
	u16 right_occupied;
	u16 bottom_boundary;
	u16 bottom_occupied;
};

struct slot {
	u8 busy;		/* is slot occupied */
	struct tcm_area parent; /* parent area */
	u32 reserved;
};

struct sita_pvt {
	u16 width;
	u16 height;
	struct list_head res;	/* all allocations */
	//struct mutex mtx;
	pthread_mutex_t mtx;
	struct tcm_pt div_pt;	/* divider point splitting container */
	struct slot **map;	/* container slots */
};

#endif /* _TCM_SITA_H_ */
