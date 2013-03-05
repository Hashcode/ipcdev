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
 * tiler_devctl_cmds.h
 *
 * TILER driver supported commands for TI OMAP processors.
 */

#ifndef TILER_DEVCTL_CMDS_H
#define TILER_DEVCTL_CMDS_H

/* Event types */
#define TILER_DEVICE_CLOSE	0

#define TILIOC_BASE 100

#define TILIOC_GBUF      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x00, struct tiler_block_info)
#define TILIOC_FBUF      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x01, struct tiler_block_info)
#define TILIOC_GSSP      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x02, uint32_t)
#define TILIOC_MBUF      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x03, struct tiler_block_info)
#define TILIOC_UMBUF     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x04, struct tiler_block_info)
#define TILIOC_QBUF      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x05, struct tiler_buf_info)
#define TILIOC_RBUF      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x06, struct tiler_buf_info)
#define TILIOC_URBUF     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x07, struct tiler_buf_info)
#define TILIOC_QUERY_BLK __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x08, struct tiler_block_info)
#define TILIOC_MMAP      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x09, uint32_t)

#define TILIOC_USRMX     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x0A, struct tiler_mapx_info)
#define TILIOC_USRF      __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x0B, uint32_t)
#define TILIOC_USRGX     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x0C, struct tiler_allocx_info)
#define TILIOC_USRRX     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x0D, struct tiler_reservex_info)
#define TILIOC_USRGP     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x0E, struct tiler_allocp_info)
#define TILIOC_USRGPNV12 __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x0F, struct tiler_allocpnv12_info)
#define TILIOC_USRROA    __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x10, struct tiler_reorient_info)
#define TILIOC_USRGNA    __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x11, uint32_t)
#define TILIOC_USRROTL   __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x12, struct tiler_reorient_tl_info)
#define TILIOC_USRSTRIDE __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x13, uint32_t)
#define TILIOC_USRRV     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x14, struct tiler_rotate_view_info)
#define TILIOC_USRGPNV12OPT __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x15, struct tiler_allocpnv12_info)
#define TILIOC_REGNOTIFY    __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x16, struct tiler_regnotify_info)
#define TILIOC_UNREGNOTIFY  __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x17, struct tiler_regnotify_info)
#define TILIOC_USRGB     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x19, struct tiler_alloc_block_area_info)
#define TILIOC_USRFB     __DIOTF(_DCMD_MISC, TILIOC_BASE + 0x1A, uint32_t)
#define TILIOC_USRMB     __DIOT(_DCMD_MISC, TILIOC_BASE + 0x1B, struct tiler_map_block_info)
#define TILIOC_USRUMB    __DIOT(_DCMD_MISC, TILIOC_BASE + 0x1C, uint32_t)

#endif
