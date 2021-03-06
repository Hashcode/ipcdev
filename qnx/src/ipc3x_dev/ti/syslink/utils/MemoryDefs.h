/**
 *  @file   MemoryDefs.h
 *
 *  @brief      Definitions for Memory module.
 *
 *              This provides macros and type definitions for the Memory module.
 *
 *
 */
/*
 *  ============================================================================
 *
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  Contact information for paper mail:
 *  Texas Instruments
 *  Post Office Box 655303
 *  Dallas, Texas 75265
 *  Contact information:
 *  http://www-k.ext.ti.com/sc/technical-support/product-information-centers.htm?
 *  DCMP=TIHomeTracking&HQS=Other+OT+home_d_contact
 *  ============================================================================
 *
 */



#ifndef MEMORYDEFS_H_0x73E4
#define MEMORYDEFS_H_0x73E4

#include <ti/syslink/utils/_MemoryDefs.h>

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  Structure defining memory related statistics
 */
typedef struct Memory_Stats_tag {
    UInt32 totalSize;
    /*!< Total memory size */
    UInt32 totalFreeSize;
    /*!< Total free memory size */
    UInt32 largestFreeSize;
    /*!< Largest free memory size */
} Memory_Stats;

/*!
 *  @brief   Enumerates the types of Caching for memory regions
 */
typedef enum {
    MemoryOS_CacheFlags_Default           = 0x00000000,
    /*!< Default flags - Cached */
    MemoryOS_CacheFlags_Cached            = 0x00010000,
    /*!< Cached memory */
    MemoryOS_CacheFlags_Uncached          = 0x00020000,
    /*!< Uncached memory */
    MemoryOS_CacheFlags_EndValue          = 0x00030000
    /*!< End delimiter indicating start of invalid values for this enum */
} MemoryOS_CacheFlags;

/*!
 *  @brief   Enumerates the types of memory allocation
 */
typedef enum {
    MemoryOS_MemTypeFlags_Default         = 0x00000000,
    /*!< Default flags - virtually contiguous */
    MemoryOS_MemTypeFlags_Physical        = 0x00000001,
    /*!< Physically contiguous */
    MemoryOS_MemTypeFlags_Dma             = 0x00000002,
    /*!< Physically contiguous */
    MemoryOS_MemTypeFlags_EndValue        = 0x00000003
    /*!< End delimiter indicating start of invalid values for this enum */
} MemoryOS_MemTypeFlags;


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ifndef MEMORYDEFS_H_0x73E4 */
