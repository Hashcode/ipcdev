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
 * tilerusr_test.c
 *
 * Memory Allocator Interface tests.
 */

/* retrieve type definitions */
#define __DEBUG__
#undef __DEBUG_ENTRY__
#define __DEBUG_ASSERT__

#define __MAP_OK__
#undef __WRITE_IN_STRIDE__
#undef STAR_TRACE_MEM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "tiler_utils/utils.h"
#include "tiler_utils/list_utils.h"
#include "tiler_utils/debug_utils.h"
#include "tilerusr/tiler.h"
#include "memmgr/memmgr.h"
#include "memmgr/tilermem.h"
#include "memmgr/tilermem_utils.h"
#include "testlib.h"
#include <dlfcn.h>

#define FALSE 0
#define TESTERR_NOTIMPLEMENTED -65378
#define PAGE_ALIGN(a) (((a)+((PAGE_SIZE)-1))&(~((PAGE_SIZE)-1)))

#define MAX_ALLOCS 40

#define TESTS\
    T(alloc_1D_test(4096, 0))\
    T(alloc_2D_test(64, 64, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(64, 64, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(64, 64, PIXEL_FMT_32BIT))\
    T(alloc_NV12_test(64, 64))\
    T(map_1D_test(4096, 0))\
    T(alloc_1D_test(176 * 144 * 2, 0))\
    T(alloc_2D_test(176, 144, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(176, 144, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(176, 144, PIXEL_FMT_32BIT))\
    T(alloc_NV12_test(176, 144))\
    T(map_1D_test(176 * 144 * 2, 0))\
    T(alloc_1D_test(640 * 480 * 2, 0))\
    T(alloc_2D_test(640, 480, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(640, 480, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(640, 480, PIXEL_FMT_32BIT))\
    T(alloc_NV12_test(640, 480))\
    T(map_1D_test(640 * 480 * 2, 0))\
    T(alloc_1D_test(848 * 480 * 2, 0))\
    T(alloc_2D_test(848, 480, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(848, 480, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(848, 480, PIXEL_FMT_32BIT))\
    T(alloc_NV12_test(848, 480))\
    T(map_1D_test(848 * 480 * 2, 0))\
    T(alloc_1D_test(1280 * 720 * 2, 0))\
    T(alloc_2D_test(1280, 720, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(1280, 720, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(1280, 720, PIXEL_FMT_32BIT))\
    T(alloc_NV12_test(1280, 720))\
    T(map_1D_test(1280 * 720 * 2, 0))\
    T(alloc_1D_test(1920 * 1080 * 2, 0))\
    T(alloc_2D_test(1920, 1080, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(1920, 1080, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(1920, 1080, PIXEL_FMT_32BIT))\
    T(alloc_NV12_test(1920, 1080))\
    T(map_1D_test(1920 * 1080 * 2, 0))\
    T(map_1D_test(4096, 0))\
    T(map_1D_test(8192, 0))\
    T(map_1D_test(16384, 0))\
    T(map_1D_test(32768, 0))\
    T(map_1D_test(65536, 0))\
    T(neg_alloc_tests())\
    T(neg_free_tests())\
    T(neg_map_tests())\
    T(neg_unmap_tests())\
    T(neg_check_tests())\
    T(page_size_test())\
    T(maxalloc_2D_test(2500, 32, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(2500, 16, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(1250, 16, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(5000, 32, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(5000, 16, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(2500, 16, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(alloc_2D_test(8193, 16, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(8193, 16, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(4097, 16, PIXEL_FMT_32BIT))\
    T(alloc_2D_test(16384, 16, PIXEL_FMT_8BIT))\
    T(alloc_2D_test(16384, 16, PIXEL_FMT_16BIT))\
    T(alloc_2D_test(8192, 16, PIXEL_FMT_32BIT))\
    T(!alloc_2D_test(16385, 16, PIXEL_FMT_8BIT))\
    T(!alloc_2D_test(16385, 16, PIXEL_FMT_16BIT))\
    T(!alloc_2D_test(8193, 16, PIXEL_FMT_32BIT))\
    T(maxalloc_1D_test(4096, MAX_ALLOCS))\
    T(maxalloc_2D_test(64, 64, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(64, 64, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(64, 64, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_NV12_test(64, 64, MAX_ALLOCS))\
    T(maxmap_1D_test(4096, MAX_ALLOCS))\
    T(maxalloc_1D_test(176 * 144 * 2, MAX_ALLOCS))\
    T(maxalloc_2D_test(176, 144, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(176, 144, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(176, 144, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_NV12_test(176, 144, MAX_ALLOCS))\
    T(maxmap_1D_test(176 * 144 * 2, MAX_ALLOCS))\
    T(maxalloc_1D_test(640 * 480 * 2, MAX_ALLOCS))\
    T(maxalloc_2D_test(640, 480, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(640, 480, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(640, 480, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_NV12_test(640, 480, MAX_ALLOCS))\
    T(maxmap_1D_test(640 * 480 * 2, MAX_ALLOCS))\
    T(maxalloc_1D_test(848 * 480 * 2, MAX_ALLOCS))\
    T(maxalloc_2D_test(848, 480, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(848, 480, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(848, 480, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_NV12_test(848, 480, MAX_ALLOCS))\
    T(maxmap_1D_test(848 * 480 * 2, MAX_ALLOCS))\
    T(maxalloc_1D_test(1280 * 720 * 2, MAX_ALLOCS))\
    T(maxalloc_2D_test(1280, 720, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(1280, 720, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(1280, 720, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_NV12_test(1280, 720, MAX_ALLOCS))\
    T(maxmap_1D_test(1280 * 720 * 2, MAX_ALLOCS))\
    T(maxalloc_1D_test(1920 * 1080 * 2, MAX_ALLOCS))\
    T(maxalloc_2D_test(1920, 1080, PIXEL_FMT_8BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(1920, 1080, PIXEL_FMT_16BIT, MAX_ALLOCS))\
    T(maxalloc_2D_test(1920, 1080, PIXEL_FMT_32BIT, MAX_ALLOCS))\
    T(maxalloc_NV12_test(1920, 1080, 2))\
    T(maxalloc_NV12_test(1920, 1080, MAX_ALLOCS))\
    T(maxmap_1D_test(1920 * 1080 * 2, MAX_ALLOCS))\
    T(star_test(100, 10))\
    T(star_test(1000, 10))\
    T(restore_test(1920, 1080))\
    T(alloc_map_1D_test(4096, 0))\
    T(alloc_map_2D_test(64, 64, PIXEL_FMT_8BIT))\
    T(alloc_map_2D_test(64, 64, PIXEL_FMT_16BIT))\
    T(alloc_map_2D_test(64, 64, PIXEL_FMT_32BIT))\
    T(alloc_map_1D_test(176 * 144 * 2, 0))\
    T(alloc_map_2D_test(176, 144, PIXEL_FMT_8BIT))\
    T(alloc_map_2D_test(176, 144, PIXEL_FMT_16BIT))\
    T(alloc_map_2D_test(176, 144, PIXEL_FMT_32BIT))\
    T(alloc_map_1D_test(640 * 480 * 2, 0))\
    T(alloc_map_2D_test(640, 480, PIXEL_FMT_8BIT))\
    T(alloc_map_2D_test(640, 480, PIXEL_FMT_16BIT))\
    T(alloc_map_2D_test(640, 480, PIXEL_FMT_32BIT))\
    T(alloc_map_1D_test(848 * 480 * 2, 0))\
    T(alloc_map_2D_test(848, 480, PIXEL_FMT_8BIT))\
    T(alloc_map_2D_test(848, 480, PIXEL_FMT_16BIT))\
    T(alloc_map_2D_test(848, 480, PIXEL_FMT_32BIT))\
    T(alloc_map_1D_test(1280 * 720 * 2, 0))\
    T(alloc_map_2D_test(1280, 720, PIXEL_FMT_8BIT))\
    T(alloc_map_2D_test(1280, 720, PIXEL_FMT_16BIT))\
    T(alloc_map_2D_test(1280, 720, PIXEL_FMT_32BIT))\
    T(alloc_map_1D_test(1920 * 1080 * 2, 0))\
    T(alloc_map_2D_test(1920, 1080, PIXEL_FMT_8BIT))\
    T(alloc_map_2D_test(1920, 1080, PIXEL_FMT_16BIT))\
    T(alloc_map_2D_test(1920, 1080, PIXEL_FMT_32BIT))\

/* this is defined in memmgr.c, but not exported as it is for internal
   use only */
extern int __test__MemMgr();

/**
 * Returns the default page stride for this block
 *
 * @author a0194118 (9/4/2009)
 *
 * @param width  Width of 2D container
 *
 * @return Stride
 */
static bytes_t def_stride(pixels_t width)
{
    return (PAGE_SIZE - 1 + (bytes_t)width) & ~(PAGE_SIZE - 1);
}

/**
 * Returns the bytes per pixel for the pixel format.
 *
 * @author a0194118 (9/4/2009)
 *
 * @param pixelFormat   Pixelformat
 *
 * @return Bytes per pixel
 */
static bytes_t def_bpp(enum tiler_fmt pixelFormat)
{
    return (pixelFormat == TILFMT_32BIT ? 4 :
            pixelFormat == TILFMT_16BIT ? 2 : 1);
}

/**
 * This method fills up a range of memory using a start address
 * and start value.  The method of filling ensures that
 * accidentally overlapping regions have minimal chances of
 * matching, even if the same starting value is used.  This is
 * because the difference between successive values varies as
 * such.  This series only repeats after 704189 values, so the
 * probability of a match for a range of at least 2 values is
 * less than 2*10^-11.
 *
 * V(i + 1) - V(i) = { 1, 2, 3, ..., 65535, 2, 4, 6, 8 ...,
 * 65534, 3, 6, 9, 12, ..., 4, 8, 12, 16, ... }
 *
 * @author a0194118 (9/6/2009)
 *
 * @param start   start value
 * @param block   pointer to block info strucure
 */
void fill_mem(uint16_t start, struct tiler_block_info *block)
{
    IN;
    uint16_t *ptr = (uint16_t *)block->ptr, delta = 1, step = 1;
    bytes_t height, width, stride, i;
    if (block->fmt == TILFMT_PAGE)
    {
        height = 1;
        stride = width = block->dim.len;
    }
    else
    {
        height = block->dim.area.height;
        width = block->dim.area.width;
        stride = block->stride;
    }
    width *= def_bpp(block->fmt);
    bytes_t size = height * stride;

    P(DEBUG_TRACE_INFO, "(%p,0x%x*0x%x,s=0x%x)=0x%x", block->ptr, width, height, stride, start);

    CHK_I(width,<=,stride);
    uint32_t *ptr32 = (uint32_t *)ptr;
    while (height--)
    {
        if (block->fmt == TILFMT_32BIT)
        {
            for (i = 0; i < width; i += sizeof(uint32_t))
            {
                uint32_t val = (start & 0xFFFF) | (((uint32_t)(start + delta) & 0xFFFF) << 16);
                *ptr32++ = val;
                start += delta;
                delta += step;
                /* increase step if overflown */
                if (delta < step) delta = ++step;
                start += delta;
                delta += step;
                /* increase step if overflown */
                if (delta < step) delta = ++step;
            }
#ifdef __WRITE_IN_STRIDE__
            while (i < stride && (height || ((PAGE_SIZE - 1) & (uint32_t)ptr32)))
            {
                *ptr32++ = 0;
                i += sizeof(uint32_t);
            }
#else
            ptr32 += (stride - i) / sizeof(uint32_t);
#endif
        }
        else
        {
            for (i = 0; i < width; i += sizeof(uint16_t))
            {
                *ptr++ = start;
                start += delta;
                delta += step;
                /* increase step if overflown */
                if (delta < step) delta = ++step;
            }
#ifdef __WRITE_IN_STRIDE__
            while (i < stride && (height || ((PAGE_SIZE - 1) & (uint32_t)ptr)))
            {
                *ptr++ = 0;
                i += sizeof(uint16_t);
            }
#else
            ptr += (stride - i) / sizeof(uint16_t);
#endif

        }
    }
    CHK_P((block->fmt == TILFMT_32BIT ? (void *)ptr32 : (void *)ptr),==,
          (block->ptr + size));
    OUT;
}

/**
 * This verifies if a range of memory at a given address was
 * filled up using the start value.
 *
 * @author a0194118 (9/6/2009)
 *
 * @param start   start value
 * @param block   pointer to block info strucure
 *
 * @return 0 on success, non-0 error value on failure
 */
int check_mem(uint16_t start, struct tiler_block_info *block)
{
    IN;
    uint16_t *ptr = (uint16_t *)block->ptr, delta = 1, step = 1;
    bytes_t height, width, stride, r, i;
    if (block->fmt == TILFMT_PAGE)
    {
        height = 1;
        stride = width = block->dim.len;
    }
    else
    {
        height = block->dim.area.height;
        width = block->dim.area.width;
        stride = block->stride;
    }
    width *= def_bpp(block->fmt);

    CHK_I(width,<=,stride);
    CHK_P(ptr,!=,NULL);
    if (ptr == NULL) {
        DP(DEBUG_TRACE_WARN, "assert: ptr in NULL");
        return R_I(MEMMGR_ERR_GENERIC);
    }
    uint32_t *ptr32 = (uint32_t *)ptr;
    for (r = 0; r < height; r++)
    {
        if (block->fmt == TILFMT_32BIT)
        {
            for (i = 0; i < width; i += sizeof(uint32_t))
            {
                uint32_t val = (start & 0xFFFF) | (((uint32_t)(start + delta) & 0xFFFF) << 16);
                if (*ptr32++ != val) {
                    DP(DEBUG_TRACE_WARN, "assert: val[%u,%u] (=0x%x) != 0x%x", r, i, *--ptr32, val);
                    return R_I(MEMMGR_ERR_GENERIC);
                }
                start += delta;
                delta += step;
                /* increase step if overflown */
                if (delta < step) delta = ++step;
                start += delta;
                delta += step;
                /* increase step if overflown */
                if (delta < step) delta = ++step;
            }
#ifdef __WRITE_IN_STRIDE__
            while (i < stride && ((r < height - 1) || ((PAGE_SIZE - 1) & (uint32_t)ptr32)))
            {
                if (*ptr32++) {
                    DP("assert: val[%u,%u] (=0x%x) != 0", r, i, *--ptr32);
                    return R_I(MEMMGR_ERR_GENERIC);
                }
                i += sizeof(uint32_t);
            }
#else
            ptr32 += (stride - i) / sizeof(uint32_t);
#endif
        }
        else
        {
            for (i = 0; i < width; i += sizeof(uint16_t))
            {
                if (*ptr++ != start) {
                    DP(DEBUG_TRACE_WARN, "assert: val[%u,%u] (=0x%x) != 0x%x", r, i, *--ptr, start);
                    return R_I(MEMMGR_ERR_GENERIC);
                }
                start += delta;
                delta += step;
                /* increase step if overflown */
                if (delta < step) delta = ++step;
            }
#ifdef __WRITE_IN_STRIDE__
            while (i < stride && ((r < height - 1) || ((PAGE_SIZE - 1) & (uint32_t)ptr)))
            {
                if (*ptr++) {
                    DP(DEBUG_TRACE_WARN, "assert: val[%u,%u] (=0x%x) != 0", r, i, *--ptr);
                    return R_I(MEMMGR_ERR_GENERIC);
                }
                i += sizeof(uint16_t);
            }
#else
            ptr += (stride - i) / sizeof(uint16_t);
#endif
        }
    }
    return R_I(MEMMGR_ERR_NONE);
}

int tilerusr_test_mmap(struct tiler_buf_info *b)
{
	s32 i = 0, j = 0, k = 0, m = 0, p = 0, bpp = 1, size = 0;
	void *va = NULL;
	void *ret = NULL;

	/* First get the total size of the area to map. */
	for (i = 0; i < b->num_blocks; i++) {
		if (b->blocks[i].fmt >= TILFMT_8BIT &&
					b->blocks[i].fmt <= TILFMT_32BIT) {
			/* get line width */
			p = PAGE_ALIGN((b->blocks[i].dim.area.width * def_bpp(b->blocks[i].fmt)) + (b->blocks[i].ssptr % PAGE_SIZE));
			size += (p * b->blocks[i].dim.area.height);
		} else if (b->blocks[i].fmt == TILFMT_PAGE) {
			p = PAGE_ALIGN(b->blocks[i].dim.len + (b->blocks[i].ssptr % PAGE_SIZE));
			size += p;
		}
	}

	/* Do a dummy mapping of this area to get contiguous virtual mem. */
    va = mmap64(NULL, size, PROT_NOCACHE | PROT_NONE, MAP_ANON | MAP_LAZY, NOFD, 0);
    if (va == MAP_FAILED) {
    	fprintf(stderr, "tilerusr_test_mmap: mmap failed.");
		return NULL;
    }

	i = 0;
	j = 0;
	k = 0;
	m = 0;
	p = 0;
	bpp = 1;
	for (i = 0; i < b->num_blocks; i++) {
		b->blocks[i].ptr = va + k + (b->blocks[i].ssptr % PAGE_SIZE);
		if (b->blocks[i].fmt >= TILFMT_8BIT &&
					b->blocks[i].fmt <= TILFMT_32BIT) {
			/* get line width */
			bpp = (b->blocks[i].fmt == TILFMT_8BIT ? 1 :
			       b->blocks[i].fmt == TILFMT_16BIT ? 2 : 4);
			p = PAGE_ALIGN((b->blocks[i].dim.area.width * bpp) + (b->blocks[i].ssptr % PAGE_SIZE));

			for (j = 0; j < b->blocks[i].dim.area.height; j++) {
				/* map each page of the line */
				ret = mmap64(va + k, p,
						PROT_NOCACHE | PROT_READ | PROT_WRITE,
						MAP_FIXED | MAP_PHYS | MAP_SHARED, NOFD,
						(b->blocks[i].ssptr - (b->blocks[i].ssptr % PAGE_SIZE) + m));
				if (ret != va + k) {
					/* TODO: should unmap any mapped regions upon failure? */
					return NULL;
				}
				k += p;
				m += tiler_stride(tiler_get_natural_addr((void*)b->blocks[i].ssptr));
			}
			m = 0;
		} else if (b->blocks[i].fmt == TILFMT_PAGE) {
			//vma->vm_pgoff = (b->blocks[i].ssptr) >> PAGE_SHIFT;
			p = PAGE_ALIGN(b->blocks[i].dim.len + (b->blocks[i].dim.len % PAGE_SIZE));
			ret = mmap64(va + k, p,
					PROT_NOCACHE | PROT_READ | PROT_WRITE,
					MAP_FIXED | MAP_PHYS | MAP_SHARED, NOFD,
					(b->blocks[i].ssptr - (b->blocks[i].ssptr % PAGE_SIZE)));
			if (ret != va + k) {
				/* TODO: should unmap any mapped regions upon failure? */
				return NULL;
			}
			k += p;
		}
	}

	return (int)b->blocks[0].ptr;
}

/**
 * This method allocates a 1D tiled buffer of the given length
 * and stride using MemMgr_Alloc.  If successful, it checks
 * that the block information was updated with the pointer to
 * the block.  Additionally, it verifies the correct return
 * values for MemMgr_IsMapped, MemMgr_Is1DBlock,
 * MemMgr_Is2DBlock, MemMgr_GetStride, TilerMem_GetStride.  It
 * also verifies TilerMem_VirtToPhys using an internally stored
 * value of the ssptr. If any of these verifications fail, the
 * buffer is freed.  Otherwise, it is filled using the given
 * start value.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 * @param stride   Buffer stride
 * @param val      Fill start value
 *
 * @return pointer to the allocated buffer, or NULL on failure
 */
void *alloc_1D(bytes_t length, bytes_t stride, uint16_t val, uint32_t *addr)
{
	uint32_t sysaddr = 0;
	int32_t ret = 0;
	struct tiler_buf_info b;
	struct tiler_block_info *block = &b.blocks[0];
    memset(&b, 0, sizeof(b));

    *addr = 0;

    ret = tiler_alloc(TILFMT_PAGE, length, 1, &sysaddr);
    if (NOT_I(ret,==,0) ||
        NOT_I(sysaddr,!=,NULL)) {
    	return NULL;
    }
    /* Populate block info */
    b.num_blocks = 1;
    block->fmt = TILFMT_PAGE;
    block->dim.len = length;
    block->stride = stride;
    block->ssptr = sysaddr;
    void *bufPtr = (void *)tilerusr_test_mmap(&b);
    block->ptr = bufPtr;

    CHK_P(bufPtr,!=,NULL);
    if (bufPtr) {
        if (NOT_I(MemMgr_IsMapped(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is1DBlock(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is2DBlock(bufPtr),==,0) ||
            /*NOT_I(MemMgr_GetStride(bufPtr),==,block->stride) ||*/
            NOT_P(TilerMem_VirtToPhys(bufPtr),==,block->ssptr) ||
            NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(bufPtr)),==,PAGE_SIZE) ||
            NOT_L((PAGE_SIZE - 1) & (long)bufPtr,==,(PAGE_SIZE - 1) & block->ssptr))
        {
            munmap(bufPtr, PAGE_ALIGN(block->dim.len));
            tiler_free(sysaddr);
            return NULL;
        }
        fill_mem(val, block);
    }
    *addr = sysaddr;
    return bufPtr;
}

/**
 * This method allocates a 1D tiled buffer of the given length
 * and stride using tiler_alloc_block_area.  If successful,
 * it checks that the sysaddr was updated with the pointer to
 * the block.  Additionally, it checks for an error in the
 * return value of the call.  If any of these verifications fail,
 * NULL is returned.
 *
 * @author a0866189 (4/30/2012)
 *
 * @param length    Buffer length
 * @param adr       Pointer to the allocated area
 * @param num_pages Pointer to the number of pages needed to
 *                  program the PAT.
 *
 * @return pointer to the allocated buffer, or NULL on failure
 */
void *alloc_area_1D(bytes_t length, uint32_t *addr, uint32_t *num_pgs)
{
	uint32_t sysaddr = 0;
	int32_t ret = 0;
	void * bufPtr = NULL;

    *addr = 0;
    *num_pgs = 0;

    ret = tiler_alloc_block_area(TILFMT_PAGE, length, 1, &sysaddr, num_pgs);
    if (NOT_I(ret,==,0) ||
        NOT_I(sysaddr,!=,NULL)) {
        return NULL;
    }

    *addr = sysaddr;
    return bufPtr;
}

/**
 * This method frees a 1D tiled buffer.  The given length,
 * stride and start values are used to verify that the buffer is
 * still correctly filled.  In the event of any errors, the
 * error value is returned.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 * @param stride   Buffer stride
 * @param val      Fill start value
 * @param bufPtr   Pointer to the allocated buffer
 *
 * @return 0 on success, non-0 error value on failure
 */
int free_1D(bytes_t length, bytes_t stride, uint16_t val, void *bufPtr, uint32_t sysAddr)
{
    struct tiler_block_info block;
    memset(&block, 0, sizeof(block));

    block.fmt = TILFMT_PAGE;
    block.dim.len = length;
    block.stride = stride;
    block.ptr = bufPtr;

    int ret = A_I(check_mem(val, &block),==,0);
    ERR_ADD(ret, munmap(bufPtr,PAGE_ALIGN(block.dim.len)));
    ERR_ADD(ret, tiler_free(sysAddr));
    return ret;
}

/**
 * This method allocates a 2D tiled buffer of the given width,
 * height, stride and pixel format using
 * MemMgr_Alloc. If successful, it checks that the block
 * information was updated with the pointer to the block.
 * Additionally, it verifies the correct return values for
 * MemMgr_IsMapped, MemMgr_Is1DBlock, MemMgr_Is2DBlock,
 * MemMgr_GetStride, TilerMem_GetStride.  It also verifies
 * TilerMem_VirtToPhys using an internally stored value of the
 * ssptr. If any of these verifications fail, the buffer is
 * freed.  Otherwise, it is filled using the given start value.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param fmt      Pixel format
 * @param stride   Buffer stride
 * @param val      Fill start value
 *
 * @return pointer to the allocated buffer, or NULL on failure
 */
void *alloc_2D(pixels_t width, pixels_t height, enum tiler_fmt fmt, bytes_t stride,
               uint16_t val, uint32_t *addr)
{
	uint32_t sysaddr = 0;
	int32_t ret = 0;
	struct tiler_buf_info b;
	struct tiler_block_info *block = &b.blocks[0];
    memset(block, 0, sizeof(block));

    *addr = 0;

    ret = tiler_alloc(fmt, width, height, &sysaddr);
    if (NOT_I(ret,==,0) ||
        NOT_I(sysaddr,!=,NULL)) {
    	return NULL;
    }
    /* Populate block info */
    b.num_blocks = 1;
    block->fmt = fmt;
    block->dim.area.width = width;
    block->dim.area.height = height;
    block->stride = def_stride(width * def_bpp(fmt));
    block->ssptr = sysaddr;
    void *bufPtr = (void *)tilerusr_test_mmap(&b);
    block->ptr = bufPtr;

    CHK_P(bufPtr,!=,NULL);
    if (bufPtr) {
        bytes_t cstride = (fmt == TILFMT_8BIT  ? TILER_STRIDE_8BIT :
                           fmt == TILFMT_16BIT ? TILER_STRIDE_16BIT :
                           TILER_STRIDE_32BIT);

        if (NOT_I(MemMgr_IsMapped(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is1DBlock(bufPtr),==,0) ||
            NOT_I(MemMgr_Is2DBlock(bufPtr),!=,0) ||
            NOT_I(block->stride,!=,0) ||
            /*NOT_I(MemMgr_GetStride(bufPtr),==,block.stride) ||*/
            NOT_P(TilerMem_VirtToPhys(bufPtr),==,block->ssptr) ||
            NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(bufPtr)),==,cstride) ||
            NOT_L((PAGE_SIZE - 1) & (long)bufPtr,==,(PAGE_SIZE - 1) & block->ssptr))
        {
            munmap(bufPtr, PAGE_ALIGN(width * def_bpp(fmt)) * height);
            tiler_free(sysaddr);
            return NULL;
        }
        fill_mem(val, block);
    }
    *addr = sysaddr;
    return bufPtr;
}

/**
 * This method allocates a 2D tiled buffer of the given width,
 * height and pixel format using tiler_alloc_block_area.
 * If successful, it checks that the sysaddr was updated with
 * the pointer to the block. Additionally, it checks for an error
 * in the return value of the call. If any of these verifications
 * fail, NULL is returned.
 *
 * @author a0866189 (4/30/2012)
 *
 * @param width     Buffer width
 * @param height    Buffer height
 * @param fmt       Pixel format
 * @param addr      Pointer to the allocated area
 * @param num_pages Pointer to the number of pages needed to
 *                  program the PAT.
 *
 * @return pointer to the allocated buffer, or NULL on failure
 */
void *alloc_area_2D(pixels_t width, pixels_t height, enum tiler_fmt fmt,
			uint32_t *addr, uint32_t *num_pgs)
{
	uint32_t sysaddr = 0;
	int32_t ret = 0;
	void *bufPtr = NULL;

    *addr = 0;

    ret = tiler_alloc_block_area(fmt, width, height, &sysaddr, num_pgs);

    if (NOT_I(ret,==,0) ||
        NOT_I(sysaddr,!=,NULL)) {
        return NULL;
    }

    *addr = sysaddr;
    return bufPtr;
}

/**
 * This method frees a 2D tiled buffer.  The given width,
 * height, pixel format, stride and start values are used to
 * verify that the buffer is still correctly filled.  In the
 * event of any errors, the error value is returned.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param fmt      Pixel format
 * @param stride   Buffer stride
 * @param val      Fill start value
 * @param bufPtr   Pointer to the allocated buffer
 *
 * @return 0 on success, non-0 error value on failure
 */
int free_2D(pixels_t width, pixels_t height, enum tiler_fmt fmt, bytes_t stride,
            uint16_t val, void *bufPtr, uint32_t sysAddr)
{
    struct tiler_block_info block;
    memset(&block, 0, sizeof(block));

    block.fmt = fmt;
    block.dim.area.width  = width;
    block.dim.area.height = height;
    block.stride = def_stride(width * def_bpp(fmt));
    block.ptr = bufPtr;

    int ret = A_I(check_mem(val, &block),==,0);
    ERR_ADD(ret, munmap(bufPtr,PAGE_ALIGN(width * def_bpp(fmt)) * height));
    ERR_ADD(ret, tiler_free(sysAddr));
    return ret;
}

/**
 * This method allocates an NV12 tiled buffer of the given width
 * and height using MemMgr_Alloc. If successful, it checks that
 * the block informations were updated with the pointers to the
 * individual blocks. Additionally, it verifies the correct
 * return values for MemMgr_IsMapped, MemMgr_Is1DBlock,
 * MemMgr_Is2DBlock, MemMgr_GetStride, TilerMem_GetStride for
 * both blocks. It also verifies TilerMem_VirtToPhys using an
 * internally stored values of the ssptr. If any of these
 * verifications fail, the buffer is freed.  Otherwise, it is
 * filled using the given start value.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param val      Fill start value
 *
 * @return pointer to the allocated buffer, or NULL on failure
 */
void alloc_NV12(void **bufPtr, pixels_t width, pixels_t height, uint16_t val, uint32_t *addr, uint32_t *uv_addr, uint32_t n)
{
	int32_t count = n;
	int32_t i = 0;
	uint32_t y_ptr[n];
	uint32_t uv_ptr[n];
	uint32_t y_ssptr[n];
	uint32_t uv_ssptr[n];
	uint32_t aligned = 1;
	struct tiler_buf_info b;
	struct tiler_block_info *blocks = &b.blocks[0];
    memset (blocks, 0, sizeof(blocks) * 2);
    memset (y_ptr, 0, sizeof(uint32_t) * n);
    memset (uv_ptr, 0, sizeof(uint32_t) * n);
    memset (y_ssptr, 0, sizeof(uint32_t) * n);
    memset (uv_ssptr, 0, sizeof(uint32_t) * n);
    memset (bufPtr, 0, sizeof(void*) * n);

    tiler_alloc_packed_nv12_opt(&count, width, height,
    		(void **)&y_ssptr, (void **)&uv_ssptr,
    		(void **)&y_ptr, (void **)&uv_ptr, aligned);
    if (NOT_I(y_ssptr,!=,NULL) ||
        NOT_I(uv_ssptr,!=,NULL) ||
        NOT_I(y_ptr,!=,NULL)) {
    	return;
    }

    for (i = 0; i < count; i++) {
		/* Populate block info */
		b.num_blocks = 2;
		blocks[0].fmt = TILFMT_8BIT;
		blocks[0].dim.area.width  = width;
		blocks[0].dim.area.height = height;
		blocks[0].stride = def_stride(width);
		blocks[0].ssptr = (u32)y_ssptr[i];
		blocks[1].fmt = TILFMT_16BIT;
		blocks[1].dim.area.width  = width >> 1;
		blocks[1].dim.area.height = height >> 1;
		blocks[1].stride = def_stride(width);
		blocks[1].ssptr = (u32)uv_ssptr[i];
		bufPtr[i] = (void *)tilerusr_test_mmap(&b);

		addr[i] = (uint32_t)y_ptr[i];
		uv_addr[i] = (uint32_t)uv_ptr[i];

		CHK_P(blocks[0].ptr,!=,NULL);
		if (bufPtr[i]) {
			void *buf2 = bufPtr[i] + blocks[0].stride * height;
			if (NOT_P(blocks[1].ptr,==,buf2) ||
				NOT_I(MemMgr_IsMapped(bufPtr[i]),!=,0) ||
				NOT_I(MemMgr_IsMapped(buf2),!=,0) ||
				NOT_I(MemMgr_Is1DBlock(bufPtr[i]),==,0) ||
				NOT_I(MemMgr_Is1DBlock(buf2),==,0) ||
				NOT_I(MemMgr_Is2DBlock(bufPtr[i]),!=,0) ||
				NOT_I(MemMgr_Is2DBlock(buf2),!=,0) ||
				NOT_I(blocks[0].stride,!=,0) ||
				NOT_I(blocks[1].stride,!=,0) ||
				/*NOT_I(MemMgr_GetStride(bufPtr[i]),==,blocks[0].stride) ||
				NOT_I(MemMgr_GetStride(buf2),==,blocks[1].stride) ||*/
				NOT_P(TilerMem_VirtToPhys(bufPtr[i]),==,y_ssptr[i]) ||
				NOT_P(TilerMem_VirtToPhys(buf2),==,uv_ssptr[i]) ||
				NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(bufPtr[i])),==,TILER_STRIDE_8BIT) ||
				NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(buf2)),==,TILER_STRIDE_16BIT) ||
				NOT_L((PAGE_SIZE - 1) & (long)blocks[0].ptr,==,(PAGE_SIZE - 1) & y_ssptr[i]) ||
				NOT_L((PAGE_SIZE - 1) & (long)blocks[1].ptr,==,(PAGE_SIZE - 1) & uv_ssptr[i]))
			{
				munmap(bufPtr[i], PAGE_ALIGN(width) * height * 3 / 2);
				tiler_free((uint32_t)y_ptr);
				return;
			}

			fill_mem(val, blocks);
			fill_mem(val, blocks + 1);
		} else {
			CHK_P(blocks[1].ptr,==,NULL);
		}
    }

    return;
}

/**
 * This method frees an NV12 tiled buffer.  The given width,
 * height and start values are used to verify that the buffer is
 * still correctly filled.  In the event of any errors, the
 * error value is returned.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param val      Fill start value
 * @param bufPtr   Pointer to the allocated buffer
 *
 * @return 0 on success, non-0 error value on failure
 */
int free_NV12(pixels_t width, pixels_t height, uint16_t val, void *bufPtr, uint32_t ssptr)
{
	struct tiler_block_info blocks[2];
    memset(blocks, 0, sizeof(blocks));

    blocks[0].fmt = TILFMT_8BIT;
    blocks[0].dim.area.width  = width;
    blocks[0].dim.area.height = height;
    blocks[0].stride = def_stride(width);
    blocks[0].ptr = bufPtr;
    blocks[1].fmt = TILFMT_16BIT;
    blocks[1].dim.area.width  = width >> 1;
    blocks[1].dim.area.height = height >> 1;
    blocks[1].stride = def_stride(width);
    blocks[1].ptr = bufPtr + blocks[0].stride * height;

    int ret = A_I(check_mem(val, blocks),==,0);
    ERR_ADD(ret, check_mem(val, blocks + 1));
    ERR_ADD(ret, munmap((void*)((uint32_t)bufPtr - ((uint32_t)bufPtr % getpagesize())),PAGE_ALIGN(width) * height * 3 / 2));
    //if (ssptr)
    //    ERR_ADD(ret, tiler_free(ssptr));
    return ret;
}

/**
 * This method maps a preallocated 1D buffer of the given length
 * and stride into tiler space using MemMgr_Map.  The mapped
 * address must differ from the supplied address is successful.
 * Moreover, it checks that the block information was
 * updated with the pointer to the block. Additionally, it
 * verifies the correct return values for MemMgr_IsMapped,
 * MemMgr_Is1DBlock, MemMgr_Is2DBlock, MemMgr_GetStride,
 * TilerMem_GetStride.  It also verifies TilerMem_VirtToPhys
 * using an internally stored value of the ssptr. If any of
 * these verifications fail, the buffer is unmapped. Otherwise,
 * the original buffer is filled using the given start value.
 *
 * :TODO: how do we verify the mapping?
 *
 * @author a0194118 (9/7/2009)
 *
 * @param dataPtr  Pointer to the allocated buffer
 * @param length   Buffer length
 * @param stride   Buffer stride
 * @param val      Fill start value
 *
 * @return pointer to the mapped buffer, or NULL on failure
 */
void *map_1D(void *dataPtr, bytes_t length, bytes_t stride, uint16_t val, uint32_t *addr)
{
	uint32_t sysaddr = 0;
	int32_t ret = 0;
	struct tiler_buf_info b;
	struct tiler_block_info *block = &b.blocks[0];
    memset(&b, 0, sizeof(b));

    *addr = 0;

    ret = tiler_map(PIXEL_FMT_PAGE, length, 1, &sysaddr, (uint32_t)dataPtr);
    if (NOT_I(ret,==,0) ||
        NOT_I(sysaddr,!=,NULL)) {
        return NULL;
    }
    /* Populate block info */
    b.num_blocks = 1;
    block->fmt = TILFMT_PAGE;
    block->dim.len = length;
    block->stride = stride;
    block->ssptr = sysaddr;
    void *bufPtr = (void *)tilerusr_test_mmap(&b);

    CHK_P(bufPtr,!=,NULL);
    if (bufPtr) {
        if (NOT_P(bufPtr,!=,dataPtr) ||
            NOT_I(MemMgr_IsMapped(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is1DBlock(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is2DBlock(bufPtr),==,0) ||
            /*NOT_I(MemMgr_GetStride(bufPtr),==,block.stride) ||*/
            NOT_P(TilerMem_VirtToPhys(bufPtr),==,block->ssptr) ||
            NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(bufPtr)),==,PAGE_SIZE) ||
            NOT_L((PAGE_SIZE - 1) & (long)bufPtr,==,0) ||
            NOT_L((PAGE_SIZE - 1) & block->ssptr,==,0))
        {
            munmap(bufPtr, PAGE_ALIGN(block->dim.len));
            tiler_free(sysaddr);
            return NULL;
        }
        block->ptr = dataPtr;
        fill_mem(val, block);
    }
    *addr = sysaddr;
    return bufPtr;
}

/**
 * This method maps given array of physical addresses into a
 * preallocated tiler space 1D buffer of the given length
 * and stride using tiler_map_block.  The returned value
 * of tiler_map_block must be zero if successful.  The tiler
 * space buffer is then mapped into the current process space.
 * Additionally, it verifies the correct return values for
 * MemMgr_IsMapped, MemMgr_Is1DBlock, MemMgr_Is2DBlock,
 * TilerMem_GetStride.  It also verifies TilerMem_VirtToPhys
 * using an internally stored value of the ssptr. If any of
 * these verifications fail, the buffer is unmapped. Otherwise,
 * the mapped buffer is filled using the given start value.
 *
 * @author a0866189 (4/30/2012)
 *
 * @param addr     Pointer to the allocated tiler space buffer
 * @param num_pages Number of physical pages being passed in the pgs array
 * @param pgs       Array of physical pages to be used for mapping
 * @param length    Buffer length
 * @param stride    Buffer stride
 * @param val       Fill start value
 *
 * @return pointer to the mapped buffer, or NULL on failure
 */
void *map_block_1D(uint32_t addr, uint32_t num_pgs, uint32_t *pgs,
			bytes_t length, bytes_t stride,
			uint16_t val, bool fill)
{
	int32_t ret = 0;
	struct tiler_buf_info b;
	struct tiler_block_info *block = &b.blocks[0];
    memset(&b, 0, sizeof(b));

    ret = tiler_map_block(addr, num_pgs, pgs);
    if (NOT_I(ret,==,0)) {
        return NULL;
    }
    /* Populate block info */
    b.num_blocks = 1;
    block->fmt = TILFMT_PAGE;
    block->dim.len = length;
    block->stride = stride;
    block->ssptr = addr;
    void *bufPtr = (void *)tilerusr_test_mmap(&b);

    CHK_P(bufPtr,!=,NULL);
    if (bufPtr) {
        if (/*NOT_P(bufPtr,!=,dataPtr) ||*/
            NOT_I(MemMgr_IsMapped(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is1DBlock(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is2DBlock(bufPtr),==,0) ||
            /*NOT_I(MemMgr_GetStride(bufPtr),==,block.stride) ||*/
            NOT_P(TilerMem_VirtToPhys(bufPtr),==,block->ssptr) ||
            NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(bufPtr)),==,PAGE_SIZE) ||
            NOT_L((PAGE_SIZE - 1) & (long)bufPtr,==,0) ||
            NOT_L((PAGE_SIZE - 1) & block->ssptr,==,0))
        {
            munmap(bufPtr, PAGE_ALIGN(block->dim.len));
            tiler_unmap_block(addr);
            return NULL;
        }
        if (fill)
            fill_mem(val, block);
    }

    return bufPtr;
}

/**
 * This method frees a 1D tiled buffer.  The given length,
 * stride and start values are used to verify that the buffer is
 * still correctly filled.  In the event of any errors, the
 * error value is returned.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 * @param stride   Buffer stride
 * @param val      Fill start value
 * @param bufPtr   Pointer to the allocated buffer
 *
 * @return 0 on success, non-0 error value on failure
 */
int unmap_block_1D(bytes_t length, bytes_t stride, uint16_t val, void *bufPtr, uint32_t sysAddr)
{
    struct tiler_block_info block;
    memset(&block, 0, sizeof(block));

    block.fmt = TILFMT_PAGE;
    block.dim.len = length;
    block.stride = stride;
    block.ptr = bufPtr;

    int ret = A_I(check_mem(val, &block),==,0);
    ERR_ADD(ret, munmap(bufPtr,PAGE_ALIGN(block.dim.len)));
    ERR_ADD(ret, tiler_unmap_block(sysAddr));
    return ret;
}

/**
 * This method maps given array of physical addresses into a
 * preallocated tiler space 2D buffer of the given width and
 * height using tiler_map_block.  The returned value
 * of tiler_map_block must be zero if successful.  The tiler
 * space buffer is then mapped into the current process space.
 * Additionally, it verifies the correct return values for
 * MemMgr_IsMapped, MemMgr_Is1DBlock, MemMgr_Is2DBlock,
 * TilerMem_GetStride.  It also verifies TilerMem_VirtToPhys
 * using an internally stored value of the ssptr. If any of
 * these verifications fail, the buffer is unmapped. Otherwise,
 * the mapped buffer is filled using the given start value.
 *
 * @author a0866189 (4/30/2012)
 *
 * @param width     Buffer width
 * @param height    Buffer height
 * @param fmt       Pixel format
 * @param addr      Pointer to the allocated tiler space buffer
 * @param num_pages Number of physical pages being passed in the pgs array
 * @param pgs       Array of physical pages to be used for mapping
 * @param val       Fill start value
 *
 * @return pointer to the mapped buffer, or NULL on failure
 */
void *map_block_2D(pixels_t width, pixels_t height, enum tiler_fmt fmt,
			uint32_t addr, uint32_t num_pgs,
			uint32_t *pgs, uint16_t val, bool fill)
{
	int32_t ret = 0;
	struct tiler_buf_info b;
	struct tiler_block_info *block = &b.blocks[0];
    memset(&b, 0, sizeof(b));

    ret = tiler_map_block(addr, num_pgs, pgs);
    if (NOT_I(ret,==,0)) {
        return NULL;
    }

    /* Populate block info */
    b.num_blocks = 1;
    block->fmt = fmt;
    block->dim.area.width = width;
    block->dim.area.height = height;
    block->stride = def_stride(width * def_bpp(fmt));
    block->ssptr = addr;
    void *bufPtr = (void *)tilerusr_test_mmap(&b);
    block->ptr = bufPtr;

    CHK_P(bufPtr,!=,NULL);
    if (bufPtr) {
        bytes_t cstride = (fmt == TILFMT_8BIT  ? TILER_STRIDE_8BIT :
                           fmt == TILFMT_16BIT ? TILER_STRIDE_16BIT :
                           TILER_STRIDE_32BIT);

        if (NOT_I(MemMgr_IsMapped(bufPtr),!=,0) ||
            NOT_I(MemMgr_Is1DBlock(bufPtr),==,0) ||
            NOT_I(MemMgr_Is2DBlock(bufPtr),!=,0) ||
            NOT_I(block->stride,!=,0) ||
            /*NOT_I(MemMgr_GetStride(bufPtr),==,block.stride) ||*/
            NOT_P(TilerMem_VirtToPhys(bufPtr),==,block->ssptr) ||
            NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(bufPtr)),==,cstride) ||
            NOT_L((PAGE_SIZE - 1) & (long)bufPtr,==,(PAGE_SIZE - 1) & block->ssptr))
        {
            munmap(bufPtr, PAGE_ALIGN(width * def_bpp(fmt)) * height);
            tiler_unmap_block(addr);
            return NULL;
        }
        if (fill)
            fill_mem(val, block);
    }

    return bufPtr;
}

/**
 * This method frees a 2D tiled buffer.  The given width,
 * height, pixel format, stride and start values are used to
 * verify that the buffer is still correctly filled.  In the
 * event of any errors, the error value is returned.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param fmt      Pixel format
 * @param stride   Buffer stride
 * @param val      Fill start value
 * @param bufPtr   Pointer to the allocated buffer
 *
 * @return 0 on success, non-0 error value on failure
 */
int unmap_block_2D(pixels_t width, pixels_t height, enum tiler_fmt fmt, bytes_t stride,
            uint16_t val, void *bufPtr, uint32_t sysAddr)
{
    struct tiler_block_info block;
    memset(&block, 0, sizeof(block));

    block.fmt = fmt;
    block.dim.area.width  = width;
    block.dim.area.height = height;
    block.stride = def_stride(width * def_bpp(fmt));
    block.ptr = bufPtr;

    int ret = A_I(check_mem(val, &block),==,0);
    ERR_ADD(ret, munmap(bufPtr,PAGE_ALIGN(width * def_bpp(fmt)) * height));
    ERR_ADD(ret, tiler_unmap_block(sysAddr));
    return ret;
}

/**
 * This method unmaps a 1D tiled buffer.  The given data
 * pointer, length, stride and start values are used to verify
 * that the buffer is still correctly filled.  In the event of
 * any errors, the error value is returned.
 *
 * :TODO: how do we verify the mapping?
 *
 * @author a0194118 (9/7/2009)
 *
 * @param dataPtr  Pointer to the preallocated buffer
 * @param length   Buffer length
 * @param stride   Buffer stride
 * @param val      Fill start value
 * @param bufPtr   Pointer to the mapped buffer
 *
 * @return 0 on success, non-0 error value on failure
 */
int unmap_1D(void *dataPtr, bytes_t length, bytes_t stride, uint16_t val,
		void *bufPtr, uint32_t sysAddr)
{
    struct tiler_block_info block;
    memset(&block, 0, sizeof(block));

    block.fmt = TILFMT_PAGE;
    block.dim.len = length;
    block.stride = stride;
    block.ptr = dataPtr;
    int ret = A_I(check_mem(val, &block),==,0);
    ERR_ADD(ret, munmap(bufPtr,PAGE_ALIGN(block.dim.len)));
    ERR_ADD(ret, tiler_free(sysAddr));
    return ret;
}

/**
 * Tests the MemMgr_PageSize method.
 *
 * @author a0194118 (9/15/2009)
 *
 * @return 0 on success, non-0 error value on failure.
 */
int page_size_test()
{
    return NOT_I(MemMgr_PageSize(),==,PAGE_SIZE);
}

/**
 * This method tests the allocation and freeing of a 1D tiled
 * buffer.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 * @param stride   Buffer stride
 *
 * @return 0 on success, non-0 error value on failure
 */
int alloc_1D_test(bytes_t length, bytes_t stride)
{
    printf("Allocate & Free %ub 1D buffer\n", length);

    uint32_t ssptr;
    uint16_t val = (uint16_t) rand();
    void *ptr = alloc_1D(length, stride, val, &ssptr);
    if (!ptr) return 1;
    int res = free_1D(length, stride, val, ptr, ssptr);
    return res;
}

int allocate_pages(int num, uint32_t *pages, uint32_t *va)
{
	int32_t i = 0, j = 0;
	uint32_t bytes = 0;
    uint32_t page_size = 4096;
	uint32_t len = 0;
	int64_t offset = 0;
	void *chunk_va = NULL;
	uint32_t chunk_pa = 0;
    int ret = 0;

    bytes = num * page_size;
	chunk_va = mmap64(NULL, bytes, PROT_NOCACHE, MAP_ANON | MAP_PRIVATE, NOFD, 0);
	if (chunk_va == MAP_FAILED) {
		return -1;
	}
	*va = (uint32_t)chunk_va;

	do {
		ret = mem_offset64(chunk_va, NOFD, bytes, &offset, &len);
		if (ret) {
			fprintf(stderr, "allocate_pages: Error from mem_offset [%d]\n", errno);
			munmap((void *)*va, num * page_size);
			*va = NULL;
			return -1;
		}
		else {
			chunk_pa = (u32)offset;
		}
		bytes -= len;

		for (i = 0; i < (len/page_size); i++) {
			pages[j++] = chunk_pa;
			chunk_pa += page_size;
			chunk_va += page_size;
		}
	} while(bytes > 0);

	return 0;
}

/**
 * This method tests the allocation and freeing of a 1D tiled
 * buffer.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 * @param stride   Buffer stride
 *
 * @return 0 on success, non-0 error value on failure
 */
int alloc_map_1D_test(bytes_t length, bytes_t stride)
{
    printf("Allocate & Map & Free %ub 1D buffer\n", length);

    uint32_t i = 0;
    uint32_t ssptr, num_pgs;
    uint32_t *pages[2];
    uint32_t page_size = 4096;
	uint32_t chunk_va[2];
    uint16_t val[2];
    val[0] = (uint16_t) rand();
    val[1] = (uint16_t) rand();
    /* Allocate an area to swap buffers in and out */
    void *ptr = alloc_area_1D(length, &ssptr, &num_pgs);
    if (!ssptr) return 1;
    /* allocate pages to use to map the area */
    for (i = 0; i < 2; i++) {
		pages[i] = malloc(sizeof(uint32_t) * num_pgs);
		if (allocate_pages(num_pgs, pages[i], &chunk_va[i])) {
			break;
		}
    }
    if (i != 2) {
        for (i = 0; i < 2; i++) {
            if (pages[i]) {
                if (chunk_va[i])
                    munmap((void *)chunk_va[i], num_pgs * page_size);
                FREE(pages[i]);
            }
        }
        free_1D(length, stride, val[0], ptr, ssptr);
        return 1;
    }

    /* map and fill the first set of pages */
	ptr = map_block_1D(ssptr, num_pgs, pages[0], length, stride, val[0], true);
    if (!ptr) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_1D(length, stride, val[0], ptr, ssptr);
        return 1;
    }

    /* unmap the first set of pages */
    int res = tiler_unmap_block(ssptr);
    if (res) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_1D(length, stride, val[0], ptr, ssptr);
        return 1;
    }

    /* map and fill the second set of pages */
	ptr = map_block_1D(ssptr, num_pgs, pages[1], length, stride, val[1], true);
    if (!ptr) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_1D(length, stride, val[0], ptr, ssptr);
        return 1;
    }

    /* unmap the second set of pages and check the contents */
    res = unmap_block_1D(length, stride, val[1], ptr, ssptr);
    if (res) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_1D(length, stride, val[0], ptr, ssptr);
        return 1;
    }

    /* re-map the first set of pages */
	ptr = map_block_1D(ssptr, num_pgs, pages[0], length, stride, val[0], false);
    if (!ptr) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_1D(length, stride, val[0], ptr, ssptr);
        return 1;
    }

	/* unmap/free the first set of pages and check the contents */
    res = free_1D(length, stride, val[0], ptr, ssptr);
    munmap((void *)chunk_va[0], num_pgs * page_size);
    munmap((void *)chunk_va[1], num_pgs * page_size);
    FREE(pages[0]);
    FREE(pages[1]);
    return res;
}

/**
 * This method tests the allocation and freeing of a 2D tiled
 * buffer.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param fmt      Pixel format
 *
 * @return 0 on success, non-0 error value on failure
 */
int alloc_map_2D_test(pixels_t width, pixels_t height, enum tiler_fmt fmt)
{
    printf("Allocate & Map & Free %ux%ux%ub 2D buffer\n", width, height, def_bpp(fmt));

    uint32_t i = 0;
    uint32_t sysaddr = 0;
    uint32_t num_pgs;
    uint32_t *pages[2];
    uint32_t page_size = 4096;
	uint32_t chunk_va[2];
    uint16_t val[2];
    val[0] = (uint16_t) rand();
    val[1] = (uint16_t) rand();

    P(DEBUG_TRACE_INFO, "alloc_area_2D...");
    void *ptr = alloc_area_2D(width, height, fmt, &sysaddr, &num_pgs);
    if (!sysaddr) return 1;

    /* allocate pages */
    P(DEBUG_TRACE_INFO, "malloc pages...");
    /* allocate pages to use to map the area */
    for (i = 0; i < 2; i++) {
		pages[i] = malloc(sizeof(uint32_t) * num_pgs);
		if (allocate_pages(num_pgs, pages[i], &chunk_va[i])) {
			break;
		}
    }
    if (i != 2) {
        for (i = 0; i < 2; i++) {
            if (pages[i]) {
                if (chunk_va[i])
                    munmap((void *)chunk_va[i], num_pgs * page_size);
                FREE(pages[i]);
            }
        }
        free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
        return 1;
    }

	P(DEBUG_TRACE_INFO, "map_block_2D...");
	ptr = map_block_2D(width, height, fmt, sysaddr, num_pgs, pages[0], val[0], true);
    if (!ptr) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
        return 1;
    }

    /* unmap the first set of pages */
    int res = tiler_unmap_block(sysaddr);
    if (res) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
        return 1;
    }

    /* map and fill the second set of pages */
	ptr = map_block_2D(width, height, fmt, sysaddr, num_pgs, pages[1], val[1], true);
    if (!ptr) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
        return 1;
    }

    /* unmap the second set of pages and check the contents */
    res = unmap_block_2D(width, height, fmt, 0, val[1], ptr, sysaddr);
    if (res) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
        return 1;
    }

    /* re-map the first set of pages */
	ptr = map_block_2D(width, height, fmt, sysaddr, num_pgs, pages[0], val[0], false);
    if (!ptr) {
        munmap((void *)chunk_va[0], num_pgs * page_size);
        munmap((void *)chunk_va[1], num_pgs * page_size);
        FREE(pages[0]);
        FREE(pages[1]);
        free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
        return 1;
    }

    P(DEBUG_TRACE_INFO, "free_2D...");
    res = free_2D(width, height, fmt, 0, val[0], ptr, sysaddr);
    munmap((void *)chunk_va[0], num_pgs * page_size);
    munmap((void *)chunk_va[1], num_pgs * page_size);
    FREE(pages[0]);
    FREE(pages[1]);
    return res;
}

/**
 * This method tests the allocation and freeing of a 2D tiled
 * buffer.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param fmt      Pixel format
 *
 * @return 0 on success, non-0 error value on failure
 */
int alloc_2D_test(pixels_t width, pixels_t height, enum tiler_fmt fmt)
{
    printf("Allocate & Free %ux%ux%ub 2D buffer\n", width, height, def_bpp(fmt));

    uint32_t sysaddr = 0;
    uint16_t val = (uint16_t) rand();
    void *ptr = alloc_2D(width, height, fmt, 0, val, &sysaddr);
    if (!ptr) return 1;
    int res = free_2D(width, height, fmt, 0, val, ptr, sysaddr);
    return res;
}

/**
 * This method tests the allocation and freeing of an NV12 tiled
 * buffer.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 *
 * @return 0 on success, non-0 error value on failure
 */
int alloc_NV12_test(pixels_t width, pixels_t height)
{
    printf("Allocate & Free %ux%u NV12 buffer\n", width, height);

    uint32_t ssptr[1];
    uint32_t uv_ssptr[1];
    ssptr[0] = 0;
    uv_ssptr[0] = 0;
    uint16_t val = (uint16_t) rand();
    void *array[1];
    alloc_NV12(array, width, height, val, ssptr, uv_ssptr, 1);
    void *ptr = array[0];
    if (!ptr) return 1;
    int res = free_NV12(width, height, val, ptr, ssptr[0]);
    if (ssptr[0])
        ERR_ADD(res, tiler_free(ssptr[0]));
    if (uv_ssptr[0])
        ERR_ADD(res, tiler_free(uv_ssptr[0]));
    return res;
}

/**
 * This method tests the mapping and unmapping of a 1D buffer.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 * @param stride   Buffer stride
 *
 * @return 0 on success, non-0 error value on failure
 */
int map_1D_test(bytes_t length, bytes_t stride)
{
    length = (length + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1);
    printf("Mapping and UnMapping 0x%xb 1D buffer\n", length);

#ifdef __MAP_OK__
    /* allocate aligned buffer */
    void *buffer = malloc(length + PAGE_SIZE - 1);
    void *dataPtr = (void *)(((uint32_t)buffer + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1));
    uint32_t sysaddr = 0;
    uint16_t val = (uint16_t) rand();
    void *ptr = map_1D(dataPtr, length, stride, val, &sysaddr);
    if (!ptr) return 1;
    int res = unmap_1D(dataPtr, length, stride, val, ptr, sysaddr);
    FREE(buffer);
#else
    int res = TESTERR_NOTIMPLEMENTED;
#endif
    return res;
}

/**
 * This method tests the allocation and freeing of a number of
 * 1D tiled buffers (up to MAX_ALLOCS)
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 *
 * @return 0 on success, non-0 error value on failure
 */
int maxalloc_1D_test(bytes_t length, int max_allocs)
{
    printf("Allocate & Free max # of %ub 1D buffers\n", length);

    struct data {
        uint16_t val;
        void    *bufPtr;
        uint32_t ssptr;
    } *mem;

    /* allocate as many buffers as we can */
    mem = NEWN(struct data, max_allocs);
    void *ptr = (void *)mem;
    int ix, res = 0;
    for (ix = 0; ptr && ix < max_allocs; ix++)
    {
        uint16_t val = (uint16_t) rand();
        ptr = alloc_1D(length, 0, val, &mem[ix].ssptr);
        if (ptr)
        {
            mem[ix].val = val;
            mem[ix].bufPtr = ptr;
        }
        else
        {
            break;
        }
    }

    P(DEBUG_TRACE_INFO, ":: Allocated %d buffers", ix);

    while (ix--)
    {
        ERR_ADD(res, free_1D(length, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr));
    }
    FREE(mem);
    return res;
}

/**
 * This method tests the allocation and freeing of a number of
 * 2D tiled buffers (up to MAX_ALLOCS)
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 * @param fmt      Pixel format
 *
 * @return 0 on success, non-0 error value on failure
 */
int maxalloc_2D_test(pixels_t width, pixels_t height, pixel_fmt_t fmt, int max_allocs)
{
    printf("Allocate & Free max # of %ux%ux%ub 2D buffers\n", width, height, def_bpp(fmt));

    struct data {
        uint16_t  val;
        void     *bufPtr;
        uint32_t  sysAddr;
    } *mem;

    /* allocate as many buffers as we can */
    mem = NEWN(struct data, max_allocs);
    void *ptr = (void *)mem;
    int ix, res = 0;
    for (ix = 0; ptr && ix < max_allocs; ix++)
    {
        uint16_t val = (uint16_t) rand();
        ptr = alloc_2D(width, height, fmt, 0, val, &mem[ix].sysAddr);
        if (ptr)
        {
            mem[ix].val = val;
            mem[ix].bufPtr = ptr;
        }
        else
        {
        	break;
        }
    }

    P(DEBUG_TRACE_INFO, ":: Allocated %d buffers", ix);

    while (ix--)
    {
        ERR_ADD(res, free_2D(width, height, fmt, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].sysAddr));
    }
    FREE(mem);
    return res;
}

/**
 * This method tests the allocation and freeing of a number of
 * NV12 tiled buffers (up to MAX_ALLOCS)
 *
 * @author a0194118 (9/7/2009)
 *
 * @param width    Buffer width
 * @param height   Buffer height
 *
 * @return 0 on success, non-0 error value on failure
 */
int maxalloc_NV12_test(pixels_t width, pixels_t height, int max_allocs)
{
    printf("Allocate & Free max # of %ux%u NV12 buffers\n", width, height);

    struct data {
        uint16_t     val;
        void        *bufPtr;
        uint32_t     ssptr;
        uint32_t     uv_ssptr;
    } *mem;

    /* allocate as many buffers as we can */
    mem = NEWN(struct data, max_allocs);
    uint32_t ssptr[max_allocs];
    uint32_t uv_ssptr[max_allocs];
    void *ptr[max_allocs];
    int ix, res = 0;
    uint16_t val = (uint16_t) rand();
    alloc_NV12(ptr, width, height, val, ssptr, uv_ssptr, max_allocs);
    if (ptr[0])
    {
       	for (ix = 0; ptr[ix] && ix < max_allocs; ix++) {
            mem[ix].ssptr = ssptr[ix];
            mem[ix].uv_ssptr = uv_ssptr[ix];
            mem[ix].val = val;
            mem[ix].bufPtr = ptr[ix];
      	}
    }

    P(DEBUG_TRACE_INFO, ":: Allocated %d buffers", ix);

    while (ix--)
    {
        ERR_ADD(res, free_NV12(width, height, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr));
    }
    ix = max_allocs;
    while (ix--)
    {
    	if (mem[ix].ssptr)
            ERR_ADD(res, tiler_free(mem[ix].ssptr));
    	if (mem[ix].uv_ssptr)
            ERR_ADD(res, tiler_free(mem[ix].uv_ssptr));
    }
    FREE(mem);
    return res;
}

/**
 * This method tests the mapping and unnapping of a number of
 * 1D buffers (up to MAX_ALLOCS)
 *
 * @author a0194118 (9/7/2009)
 *
 * @param length   Buffer length
 *
 * @return 0 on success, non-0 error value on failure
 */
int maxmap_1D_test(bytes_t length, int max_maps)
{
    length = (length + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1);
    printf("Map & UnMap max # of %xb 1D buffers\n", length);

#ifdef __MAP_OK__
    struct data {
        uint16_t val;
        void    *bufPtr, *buffer, *dataPtr;
        uint32_t ssptr;
    } *mem;

    /* map as many buffers as we can */
    mem = NEWN(struct data, max_maps);
    void *ptr = (void *)mem;
    int ix, res = 0;
    for (ix = 0; ptr && ix < max_maps; ix++)
    {
        /* allocate aligned buffer */
        void *ptr = malloc(length + PAGE_SIZE - 1);
        if (ptr)
        {
            void *buffer = ptr;
            void *dataPtr = (void *)(((uint32_t)buffer + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1));
            uint16_t val = (uint16_t) rand();
            ptr = map_1D(dataPtr, length, 0, val, &mem[ix].ssptr);
            if (ptr)
            {
                mem[ix].val = val;
                mem[ix].bufPtr = ptr;
                mem[ix].buffer = buffer;
                mem[ix].dataPtr = dataPtr;
            }
            else
            {
                FREE(buffer);
                break;
            }
        }
    }

    P(DEBUG_TRACE_INFO, ":: Mapped %d buffers", ix);

    while (ix--)
    {
        ERR_ADD(res, unmap_1D(mem[ix].dataPtr, length, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr));
        FREE(mem[ix].buffer);
    }
    FREE(mem);
#else
    int res = TESTERR_NOTIMPLEMENTED;
#endif
    return res;
}

/**
 * This stress tests allocates/maps/frees/unmaps buffers at
 * least num_ops times.  The test maintains a set of slots that
 * are initially NULL.  For each operation, a slot is randomly
 * selected.  If the slot is not used, it is filled randomly
 * with a 1D, 2D, NV12 or mapped buffer.  If it is used, the
 * slot is cleared by freeing/unmapping the buffer already
 * there.  The buffers are filled on alloc/map and this is
 * checked on free/unmap to verify that there was no memory
 * corruption.  Failed allocation and maps are ignored as we may
 * run out of memory.  The return value is the first error code
 * encountered, or 0 on success.
 *
 * This test sets the seed so that it produces reproducible
 * results.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param num_ops    Number of operations to perform
 * @param num_slots  Number of slots to maintain
 *
 * @return 0 on success, non-0 error value on failure
 */
int star_test(uint32_t num_ops, uint16_t num_slots)
{
    printf("Random set of %d Allocs/Maps and Frees/UnMaps for %d slots\n", num_ops, num_slots);
    srand(0x4B72316A);
    void *tmpBufPtr[1];
    uint32_t tmpSSPtr[1];
    uint32_t tmpSSPtrUV[1];
    struct data {
        int      op;
        uint16_t val;
        pixels_t width, height;
        bytes_t  length;
        void    *bufPtr;
        void    *buffer;
        void    *dataPtr;
        uint32_t ssptr;
        uint32_t uv_ssptr;
    } *mem;

    /* allocate memory state */
    mem = NEWN(struct data, num_slots);
    if (!mem) return NOT_P(mem,!=,NULL);

    /* perform alloc/free/unmaps */
    int res = 0, ix;
    while (!res && num_ops--)
    {
        ix = rand() % num_slots;
        /* see if we need to free/unmap data */
        if (mem[ix].bufPtr)
        {
            /* check memory fill */
            switch (mem[ix].op)
            {
            case 0: res = unmap_1D(mem[ix].dataPtr, mem[ix].length, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr);
                FREE(mem[ix].buffer);
                break;
            case 1: res = free_1D(mem[ix].length, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr); break;
            case 2: res = free_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_8BIT, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr); break;
            case 3: res = free_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_16BIT, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr); break;
            case 4: res = free_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_32BIT, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr); break;
            case 5: res = free_NV12(mem[ix].width, mem[ix].height, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr);
                if (mem[ix].ssptr)
                    ERR_ADD(res, tiler_free(mem[ix].ssptr));
                if (mem[ix].uv_ssptr)
                    ERR_ADD(res, tiler_free(mem[ix].uv_ssptr));
                break;
            }
            P(DEBUG_TRACE_INFO, "%s[%p]", mem[ix].op ? "free" : "unmap", mem[ix].bufPtr);
            ZERO(mem[ix]);
        }
        /* we need to allocate/map data */
        else
        {
            int op = rand();
            /* set width */
            pixels_t width = 0, height = 0;
            switch ("AAAABBBBCCCDDEEF"[op & 15]) {
            case 'F': width = 1920; height = 1080; break;
            case 'E': width = 1280; height = 720; break;
            case 'D': width = 640; height = 480; break;
            case 'C': width = 848; height = 480; break;
            case 'B': width = 176; height = 144; break;
            case 'A': width = height = 64; break;
            }
            mem[ix].length = (bytes_t)width * height;
            mem[ix].width = width;
            mem[ix].height = height;
            mem[ix].val = ((uint16_t)rand());

            /* perform operation */
            mem[ix].op = "AAABBBBCCCCDDDDE"[(op >> 4) & 15] - 'A';
            switch (mem[ix].op)
            {
            case 0: /* map 1D buffer */
#ifdef __MAP_OK__
                /* allocate aligned buffer */
                mem[ix].length = (mem[ix].length + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1);
                mem[ix].buffer = malloc(mem[ix].length + PAGE_SIZE - 1);
                if (mem[ix].buffer)
                {
                    mem[ix].dataPtr = (void *)(((uint32_t)mem[ix].buffer + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1));
                    mem[ix].bufPtr = map_1D(mem[ix].dataPtr, mem[ix].length, 0, mem[ix].val, &mem[ix].ssptr);
                    if (!mem[ix].bufPtr) FREE(mem[ix].buffer);
                }
                P(DEBUG_TRACE_INFO, "map[l=0x%x] = %p", mem[ix].length, mem[ix].bufPtr);
                break;
#else
                mem[ix].op = 1;
#endif
            case 1:
                mem[ix].bufPtr = alloc_1D(mem[ix].length, 0, mem[ix].val, &mem[ix].ssptr);
                P(DEBUG_TRACE_INFO, "alloc[l=0x%x] = %p", mem[ix].length, mem[ix].bufPtr);
                break;
            case 2:
                mem[ix].bufPtr = alloc_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_8BIT, 0, mem[ix].val, &mem[ix].ssptr);
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*8] = %p", mem[ix].width, mem[ix].height, mem[ix].bufPtr);
                break;
            case 3:
                mem[ix].bufPtr = alloc_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_16BIT, 0, mem[ix].val, &mem[ix].ssptr);
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*16] = %p", mem[ix].width, mem[ix].height, mem[ix].bufPtr);
                break;
            case 4:
                mem[ix].bufPtr = alloc_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_32BIT, 0, mem[ix].val, &mem[ix].ssptr);
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*32] = %p", mem[ix].width, mem[ix].height, mem[ix].bufPtr);
                break;
            case 5:
                alloc_NV12(tmpBufPtr, mem[ix].width, mem[ix].height, mem[ix].val, tmpSSPtr, tmpSSPtrUV, 1);
                mem[ix].ssptr = tmpSSPtr[0];
                mem[ix].uv_ssptr = tmpSSPtrUV[0];
                mem[ix].bufPtr = tmpBufPtr[0];
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*NV12] = %p", mem[ix].width, mem[ix].height, mem[ix].bufPtr);
                break;
            }

            /* check all previous buffers */
#ifdef STAR_TRACE_MEM
            for (ix = 0; ix < num_slots; ix++)
            {
                MemAllocBlock blk;
                if (mem[ix].bufPtr)
                {
                    if(0) P(DEBUG_TRACE_INFO, "ptr=%p, op=%d, w=%d, h=%d, l=%x, val=%x",
                      mem[ix].bufPtr, mem[ix].op, mem[ix].width, mem[ix].height,
                      mem[ix].length, mem[ix].val);
                    switch (mem[ix].op)
                    {
                    case 0: case 1:
                        blk.pixelFormat = PIXEL_FMT_PAGE;
                        blk.dim.len = mem[ix].length;
                        break;
                    case 5:
                        blk.pixelFormat = PIXEL_FMT_16BIT;
                        blk.dim.area.width = mem[ix].width >> 1;
                        blk.dim.area.height = mem[ix].height >> 1;
                        blk.stride = def_stride(mem[ix].width);  /* same for Y and UV */
                        blk.ptr = mem[ix].bufPtr + mem[ix].height * blk.stride;
                        check_mem(mem[ix].val, &blk);
                    case 2:
                        blk.pixelFormat = PIXEL_FMT_8BIT;
                        blk.dim.area.width = mem[ix].width;
                        blk.dim.area.height = mem[ix].height;
                        blk.stride = def_stride(mem[ix].width);
                        break;
                    case 3:
                        blk.pixelFormat = PIXEL_FMT_16BIT;
                        blk.dim.area.width = mem[ix].width;
                        blk.dim.area.height = mem[ix].height;
                        blk.stride = def_stride(mem[ix].width * 2);
                        break;
                    case 4:
                        blk.pixelFormat = PIXEL_FMT_32BIT;
                        blk.dim.area.width = mem[ix].width;
                        blk.dim.area.height = mem[ix].height;
                        blk.stride = def_stride(mem[ix].width * 4);
                        break;
                    }
                    blk.ptr = mem[ix].bufPtr;
                    check_mem(mem[ix].val, &blk);
                }
            }
#endif
        }
    }

    /* unmap and free everything */
    for (ix = 0; ix < num_slots; ix++)
    {
        if (mem[ix].bufPtr)
        {
            /* check memory fill */
            switch (mem[ix].op)
            {
            case 0: ERR_ADD(res, unmap_1D(mem[ix].dataPtr, mem[ix].length, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr));
                FREE(mem[ix].buffer);
                break;
            case 1: ERR_ADD(res, free_1D(mem[ix].length, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr)); break;
            case 2: ERR_ADD(res, free_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_8BIT, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr)); break;
            case 3: ERR_ADD(res, free_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_16BIT, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr)); break;
            case 4: ERR_ADD(res, free_2D(mem[ix].width, mem[ix].height, PIXEL_FMT_32BIT, 0, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr)); break;
            case 5: ERR_ADD(res, free_NV12(mem[ix].width, mem[ix].height, mem[ix].val, mem[ix].bufPtr, mem[ix].ssptr));
                if (mem[ix].ssptr)
                    ERR_ADD(res, tiler_free(mem[ix].ssptr));
                if (mem[ix].uv_ssptr)
                    ERR_ADD(res, tiler_free(mem[ix].uv_ssptr));
                break;
            }
        }
    }
    FREE(mem);

    return res;
}

/**
 * This method tests the restore functionality of Tiler.
 * Allocated 1D and 2D buffers are tested.
 *
 * @author a0866189 (5/14/2012)
 *
 * @param width   Buffer width
 * @param height  Buffer height
 *
 * @return 0 on success, non-0 error value on failure
 */
int restore_test(pixels_t width, pixels_t height)
{
    void *tiler_pat_lib = NULL;
    int (*tiler_pat_init)(void);
    int (*tiler_restore_pat)(void);

    printf("Allocate, Restore & Free 1D and 2D buffers\n");

    struct data {
        uint16_t val;
        void    *bufPtr;
        uint32_t ssptr;
    } *mem1d, *mem2d, *mem1dmap;

	tiler_pat_lib = dlopen("tiler_pat.so", RTLD_NOW | RTLD_GLOBAL);
	if (tiler_pat_lib == NULL) {
		fprintf(stderr, "Tiler_init: Error opening shared lib [%d]\n", errno);
		return 1;
	}
	tiler_pat_init = dlsym(tiler_pat_lib, "tiler_pat_init");
	if (tiler_pat_init == NULL) {
		fprintf(stderr, "Tiler_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}
	(*tiler_pat_init)();
	tiler_restore_pat = dlsym(tiler_pat_lib, "tiler_restore_pat");
	if (tiler_restore_pat == NULL) {
		fprintf(stderr, "Tiler_init: Error getting shared lib sym [%d]\n", errno);
		goto error;
	}

    /* allocate 1 buffer */
    mem1d = NEWN(struct data, 1);
    void *ptr = (void *)mem1d;
    int res = 0;
    int length = width * height * 2;

    uint16_t val = (uint16_t) rand();
    ptr = alloc_1D(length, 0, val, &mem1d[0].ssptr);
    if (ptr)
    {
        mem1d[0].val = val;
        mem1d[0].bufPtr = ptr;
    }
	else {
		ERR_ADD(res, MEMMGR_ERR_GENERIC);
	}

    P(DEBUG_TRACE_INFO, ":: Allocated 1 buffer");

    /* allocate 1 buffer */
    mem2d = NEWN(struct data, 1);
    ptr = (void *)mem2d;
	val = (uint16_t) rand();
	ptr = alloc_2D(width, height, PIXEL_FMT_16BIT, 0, val, &mem2d[0].ssptr);
	CHK_P(ptr,!=,NULL);
	if (ptr)
	{
		mem2d[0].val = val;
		mem2d[0].bufPtr = ptr;
	}
	else {
		ERR_ADD(res, MEMMGR_ERR_GENERIC);
	}

    P(DEBUG_TRACE_INFO, ":: Allocated 1 buffer");

    /* map 1 buffer */
    mem1dmap = NEWN(struct data, 1);
    ptr = (void *)mem1dmap;
	val = (uint16_t) rand();
    /* allocate aligned buffer */
    void *buffer = malloc(length + PAGE_SIZE - 1);
    void *dataPtr = (void *)(((uint32_t)buffer + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1));
	ptr = map_1D(dataPtr, length, 0, val, &mem1dmap[0].ssptr);
	CHK_P(ptr,!=,NULL);
	if (ptr)
	{
		mem1dmap[0].val = val;
		mem1dmap[0].bufPtr = ptr;
	}
	else {
		ERR_ADD(res, MEMMGR_ERR_GENERIC);
	}

    P(DEBUG_TRACE_INFO, ":: Mapped 1 buffer");

    ERR_ADD(res, (*tiler_restore_pat)());

    ERR_ADD(res, unmap_1D(dataPtr, length, 0, mem1dmap[0].val, mem1dmap[0].bufPtr, mem1dmap[0].ssptr));
    FREE(buffer);

    ERR_ADD(res, free_1D(length, 0, mem1d[0].val, mem1d[0].bufPtr, mem1d[0].ssptr));

    ERR_ADD(res, free_2D(width, height, PIXEL_FMT_16BIT, 0, mem2d[0].val, mem2d[0].bufPtr, mem2d[0].ssptr));

    FREE(mem1dmap);
    FREE(mem2d);
    FREE(mem1d);
error:
    dlclose(tiler_pat_lib);
    return res;
}

#if 0
#include <memmgr/tilermgr.h>
/**
 * This stress tests allocates/maps/frees/unmaps buffers at
 * least num_ops times.  The test maintains a set of slots that
 * are initially NULL.  For each operation, a slot is randomly
 * selected.  If the slot is not used, it is filled randomly
 * with a 1D, 2D, NV12 or mapped buffer.  If it is used, the
 * slot is cleared by freeing/unmapping the buffer already
 * there.  The buffers are filled on alloc/map and this is
 * checked on free/unmap to verify that there was no memory
 * corruption.  Failed allocation and maps are ignored as we may
 * run out of memory.  The return value is the first error code
 * encountered, or 0 on success.
 *
 * This test sets the seed so that it produces reproducible
 * results.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param num_ops    Number of operations to perform
 * @param num_slots  Number of slots to maintain
 *
 * @return 0 on success, non-0 error value on failure
 */
int star_tiler_test(uint32_t num_ops, uint16_t num_slots)
{
    printf("Random set of %d tiler Allocs/Maps and Frees/UnMaps for %d slots\n", num_ops, num_slots);
    srand(0x4B72316A);
    struct data {
        int      op;
        SSPtr    ssptr;
        void    *buffer;
        void    *dataPtr;
    } *mem;

    /* allocate memory state */
    mem = NEWN(struct data, num_slots);
    if (!mem) return NOT_P(mem,!=,NULL);

    /* perform alloc/free/unmaps */
    int ix, res = TilerMgr_Open();
    while (!res && num_ops--)
    {
        ix = rand() % num_slots;
        /* see if we need to free/unmap data */
        if (mem[ix].ssptr)
        {
            switch (mem[ix].op)
            {
            case 0: /*
            res = unmap_1D(mem[ix].dataPtr, mem[ix].length, 0, mem[ix].val, mem[ix].bufPtr); */
                FREE(mem[ix].buffer);
                break;
            case 1:
                P(DEBUG_TRACE_INFO, "free [0x%x]", mem[ix].ssptr);
                res = TilerMgr_PageModeFree(mem[ix].ssptr); break;
            case 2: case 3: case 4:
                P(DEBUG_TRACE_INFO, "free [0x%x]", mem[ix].ssptr);
                res = TilerMgr_Free(mem[ix].ssptr); break;
            }
            ZERO(mem[ix]);
        }
        /* we need to allocate/map data */
        else
        {
            int op = rand();
            /* set width */
            pixels_t width = 0, height = 0;
            switch ("AAAABBBBCCCDDEEF"[op & 15]) {
            case 'F': width = 1920; height = 1080; break;
            case 'E': width = 1280; height = 720; break;
            case 'D': width = 640; height = 480; break;
            case 'C': width = 848; height = 480; break;
            case 'B': width = 176; height = 144; break;
            case 'A': width = height = 64; break;
            }
            bytes_t length = (bytes_t)width * height;

            /* perform operation */
            mem[ix].op = "AAABBBBCCCCDDDDE"[(op >> 4) & 15] - 'A';
            switch (mem[ix].op)
            {
            case 0: /* map 1D buffer */
                mem[ix].op = 1;
            case 1:
                mem[ix].ssptr = TilerMgr_PageModeAlloc(length);
                P(DEBUG_TRACE_INFO, "alloc[l=0x%x] = 0x%x", length, mem[ix].ssptr);
                break;
            case 2:
                mem[ix].ssptr = TilerMgr_Alloc(PIXEL_FMT_8BIT, width, height);
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*8] = 0x%x", width, height, mem[ix].ssptr);
                break;
            case 3:
                mem[ix].ssptr = TilerMgr_Alloc(PIXEL_FMT_16BIT, width, height);
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*16] = 0x%x", width, height, mem[ix].ssptr);
                break;
            case 4:
                mem[ix].ssptr = TilerMgr_Alloc(PIXEL_FMT_32BIT, width, height);
                P(DEBUG_TRACE_INFO, "alloc[%d*%d*32] = 0x%x", width, height, mem[ix].ssptr);
                break;
            }
        }
    }

    /* unmap and free everything */
    for (ix = 0; ix < num_slots; ix++)
    {
        if (mem[ix].ssptr)
        {
            /* check memory fill */
            switch (mem[ix].op)
            {
            case 0: /* ERR_ADD(res, unmap_1D(mem[ix].dataPtr, mem[ix].length, 0, mem[ix].val, mem[ix].bufPtr)); */
                FREE(mem[ix].buffer);
                break;
            case 1: ERR_ADD(res, TilerMgr_PageModeFree(mem[ix].ssptr)); break;
            default: ERR_ADD(res, TilerMgr_Free(mem[ix].ssptr)); break;
            }
        }
    }
    ERR_ADD(res, TilerMgr_Close());
    FREE(mem);

    return res;
}
#endif

#define NEGA(exp) E_ { int __ret__ = A_I(exp,==,NULL); if (ssptr) tiler_free(ssptr); __ret__ == 0; } _E

/**
 * Performs negative tests for MemMgr_Alloc.
 *
 * @author a0194118 (9/7/2009)
 *
 * @return 0 on success, non-0 error value on failure
 */
int neg_alloc_tests()
{
    uint32_t ssptr = 0;

    printf("Negative Alloc tests\n");

    MemAllocBlock block[2], *blk;
    memset(&block, 0, sizeof(block));

    int ret = 0, num_blocks;

    for (num_blocks = 1; num_blocks < 3; num_blocks++)
    {
        blk = block + num_blocks - 1;

        P(DEBUG_TRACE_INFO, "/* bad pixel format */");
        ret |= NEGA(tiler_alloc(PIXEL_FMT_MIN - 1, PAGE_SIZE, 1, &ssptr));
        ret |= NEGA(tiler_alloc(PIXEL_FMT_MAX + 1, PAGE_SIZE, 1, &ssptr));

#if 0 // tilerusr functions don't take stride...
        P(DEBUG_TRACE_INFO, "/* bad 1D stride */");
        blk->pixelFormat = PIXEL_FMT_PAGE;
        blk->stride = PAGE_SIZE - 1;
        ret |= NEGA(MemMgr_Alloc(block, num_blocks));
#endif

        P(DEBUG_TRACE_INFO, "/* 0 1D length */");
        ret |= NEGA(tiler_alloc(PIXEL_FMT_PAGE, 0, 1, &ssptr));

#if 0 // tilerusr functions don't take stride...
        P(DEBUG_TRACE_INFO, "/* bad 2D stride */");
        blk->pixelFormat = PIXEL_FMT_8BIT;
        blk->dim.area.width = PAGE_SIZE - 1;
        blk->stride = PAGE_SIZE - 1;
        blk->dim.area.height = 16;
        ret |= NEGA(MemMgr_Alloc(block, num_blocks));
#endif

        P(DEBUG_TRACE_INFO, "/* bad 2D width */");
        blk->stride = blk->dim.area.width = 0;
        ret |= NEGA(tiler_alloc(PIXEL_FMT_8BIT, 0, 16, &ssptr));

        P(DEBUG_TRACE_INFO, "/* bad 2D height */");
        ret |= NEGA(tiler_alloc(PIXEL_FMT_8BIT, 16, 0, &ssptr));

        /* good 2D block */
        blk->dim.area.height = 16;
    }

    return ret;
}

/**
 * Performs negative tests for MemMgr_Free.
 *
 * @author a0194118 (9/7/2009)
 *
 * @return 0 on success, non-0 error value on failure
 */
int neg_free_tests()
{
    printf("Negative Free tests\n");

    uint32_t sysaddr = 0;
    void *ptr = alloc_2D(2500, 10, PIXEL_FMT_16BIT, 2 * PAGE_SIZE, 0, &sysaddr);
    int ret = 0;

    tiler_free(sysaddr);

    P(DEBUG_TRACE_INFO, "/* free something twice */");
    ret |= NOT_I(tiler_free(sysaddr),!=,0);

    P(DEBUG_TRACE_INFO, "/* free NULL */");
    ret |= NOT_I(tiler_free(NULL),!=,0);

    P(DEBUG_TRACE_INFO, "/* free arbitrary value */");
    ret |= NOT_I(tiler_free((uint32_t)0x12345678),!=,0);

    P(DEBUG_TRACE_INFO, "/* free mapped buffer */");
    void *buffer = malloc(PAGE_SIZE * 2);
    void *dataPtr = (void *)(((uint32_t)buffer + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1));
    ptr = map_1D(dataPtr, PAGE_SIZE, 0, 0, &sysaddr);
    ret |= NOT_I(tiler_free((uint32_t)ptr),!=,0);

    tiler_free(sysaddr);

    return ret;
}

#define NEGM(exp) E_ { int __ret__ = A_I(exp,==,NULL); if (sysaddr) tiler_free(sysaddr); __ret__ == 0; } _E

/**
 * Performs negative tests for MemMgr_Map.
 *
 * @author a0194118 (9/7/2009)
 *
 * @return 0 on success, non-0 error value on failure
 */
int neg_map_tests()
{
    uint32_t sysaddr = 0;
    uint32_t usraddr = 0;
    printf("Negative Map tests\n");

    MemAllocBlock block[2], *blk;
    memset(&block, 0, sizeof(block));

    int ret = 0, num_blocks;

    for (num_blocks = 1; num_blocks < 3; num_blocks++)
    {
        blk = block + num_blocks - 1;

        P(DEBUG_TRACE_INFO, "/* bad pixel format */");
        ret |= NEGM(tiler_map(PIXEL_FMT_MIN - 1, PAGE_SIZE, 1, &sysaddr, usraddr));
        ret |= NEGM(tiler_map(PIXEL_FMT_MAX + 1, PAGE_SIZE, 1, &sysaddr, usraddr));

#if 0 // tilerusr functions don't take stride...
        P(DEBUG_TRACE_INFO, "/* bad 1D stride */");
        blk->pixelFormat = PIXEL_FMT_PAGE;
        blk->stride = PAGE_SIZE - 1;
        ret |= NEGM(MemMgr_Map(block, num_blocks));
#endif

        P(DEBUG_TRACE_INFO, "/* 0 1D length */");
        ret |= NEGM(tiler_map(PIXEL_FMT_PAGE, 0, 1, &sysaddr, usraddr));

#if 0 // tilerusr functions don't take stride...
        P(DEBUG_TRACE_INFO, "/* bad 2D stride */");
        blk->pixelFormat = PIXEL_FMT_8BIT;
        blk->dim.area.width = PAGE_SIZE - 1;
        blk->stride = PAGE_SIZE - 1;
        blk->dim.area.height = 16;
        ret |= NEGM(MemMgr_Map(block, num_blocks));
#endif

        P(DEBUG_TRACE_INFO, "/* bad 2D width */");
        ret |= NEGM(tiler_map(PIXEL_FMT_8BIT, 0, 16, &sysaddr, usraddr));

        P(DEBUG_TRACE_INFO, "/* bad 2D height */");
        ret |= NEGM(tiler_map(PIXEL_FMT_8BIT, 16, 0, &sysaddr, usraddr));
    }

    P(DEBUG_TRACE_INFO, "/* 1 2D buffer */");
    ret |= NEGM(tiler_map(PIXEL_FMT_8BIT, 16, 16, &sysaddr, usraddr));

    P(DEBUG_TRACE_INFO, "/* 1 1D buffer with no address */");
    ret |= NEGM(tiler_map(PIXEL_FMT_PAGE, 2 * PAGE_SIZE, 1, &sysaddr, usraddr));

    return ret;
}

/**
 * Performs negative tests for MemMgr_UnMap.
 *
 * @author a0194118 (9/7/2009)
 *
 * @return 0 on success, non-0 error value on failure
 */
int neg_unmap_tests()
{
    printf("Negative Unmap tests\n");

    uint32_t ssptr;
    void *ptr = alloc_1D(PAGE_SIZE, 0, 0, &ssptr);
    int ret = 0;

    P(DEBUG_TRACE_INFO, "/* unmap alloced buffer */");
    ret |= NOT_I(tiler_free((uint32_t)ptr),!=,0);

    tiler_free(ssptr);

    void *buffer = malloc(PAGE_SIZE * 2);
    void *dataPtr = (void *)(((uint32_t)buffer + PAGE_SIZE - 1) &~ (PAGE_SIZE - 1));
    ptr = map_1D(dataPtr, PAGE_SIZE, 0, 0, &ssptr);
    tiler_free(ssptr);

    P(DEBUG_TRACE_INFO, "/* unmap something twice */");
    ret |= NOT_I(tiler_free(ssptr),!=,0);

    P(DEBUG_TRACE_INFO, "/* unmap NULL */");
    ret |= NOT_I(tiler_free(NULL),!=,0);

    P(DEBUG_TRACE_INFO, "/* unmap arbitrary value */");
    ret |= NOT_I(tiler_free((uint32_t)0x12345678),!=,0);

    return ret;
}

/**
 * Performs negative tests for MemMgr_Is.. functions.
 *
 * @author a0194118 (9/7/2009)
 *
 * @return 0 on success, non-0 error value on failure
 */
int neg_check_tests()
{
    printf("Negative Is... tests\n");
    void *ptr = malloc(32);

    int ret = 0;

    ret |= NOT_I(MemMgr_Is1DBlock(NULL),==,FALSE);
    ret |= NOT_I(MemMgr_Is1DBlock((void *)0x12345678),==,FALSE);
    ret |= NOT_I(MemMgr_Is1DBlock(ptr),==,FALSE);
    ret |= NOT_I(MemMgr_Is2DBlock(NULL),==,FALSE);
    ret |= NOT_I(MemMgr_Is2DBlock((void *)0x12345678),==,FALSE);
    ret |= NOT_I(MemMgr_Is2DBlock(ptr),==,FALSE);
    ret |= NOT_I(MemMgr_IsMapped(NULL),==,FALSE);
    ret |= NOT_I(MemMgr_IsMapped((void *)0x12345678),==,FALSE);
    ret |= NOT_I(MemMgr_IsMapped(ptr),==,FALSE);

    ret |= NOT_I(MemMgr_GetStride(NULL),==,0);
    ret |= NOT_I(MemMgr_GetStride((void *)0x12345678),==,0);
    ret |= NOT_I(MemMgr_GetStride(ptr),==,PAGE_SIZE);

    ret |= NOT_P(TilerMem_VirtToPhys(NULL),==,0);
    ret |= NOT_P(TilerMem_VirtToPhys((void *)0x12345678),==,0);
    ret |= NOT_P(TilerMem_VirtToPhys(ptr),!=,0);

    ret |= NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(NULL)),==,0);
    ret |= NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys((void *)0x12345678)),==,0);
    ret |= NOT_I(TilerMem_GetStride(TilerMem_VirtToPhys(ptr)),==,0);

    FREE(ptr);

    return ret;
}

DEFINE_TESTS(TESTS)

/**
 * We run the same identity check before and after running the
 * tests.
 *
 * @author a0194118 (9/12/2009)
 */
void memmgr_identity_test(void *ptr)
{
    /* also execute internal unit tests - this also verifies that we did not
       keep any references */
    __test__MemMgr();
}

/**
 * Main test function. Checks arguments for test case ranges,
 * runs tests and prints usage or test list if required.
 *
 * @author a0194118 (9/7/2009)
 *
 * @param argc   Number of arguments
 * @param argv   Arguments
 *
 * @return -1 on usage or test list, otherwise # of failed
 *         tests.
 */
int main(int argc, char **argv)
{
	tiler_open();
    return TestLib_Run(argc, argv,
                       memmgr_identity_test, memmgr_identity_test, NULL);
}

