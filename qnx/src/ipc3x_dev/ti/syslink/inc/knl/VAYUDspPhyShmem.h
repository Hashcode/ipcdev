/**
 *  @file   VAYUDspPhyShmem.h
 *
 *  @brief      Physical Interface Abstraction Layer for VAYUDSP.
 *
 *              This file contains the definitions for shared memory physical
 *              link being used with VAYUDSP.
 *              The implementation is specific to VAYUDSP.
 *
 *
 */
/*
 *  ============================================================================
 *
 *  Copyright (c) 2013, Texas Instruments Incorporated
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


#ifndef VAYUDSPPHYSHMEM_H_0xbbed
#define VAYUDSPPHYSHMEM_H_0xbbed


/* Module level headers */
#include <_ProcDefs.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 *  See _ProcDefs.h
 * =============================================================================
 */
/*!
 *  @brief  Base addresses of dsp boot stat reg to indicate the boot complete.
 */
#define DSP_BOOT_STAT           0x48140044

#define DSP_BOOT_STAT_SIZE      0x4
/*!
 *  @brief  Base addresses of L2_RAM for Ducati.
 */
#define DSP_BOOT_ADDR           0x4a00255c

/*!
 *  @brief  Base addresses of L2_RAM for Ducati.
 */
#define DSP_BOOT_ADDR_SIZE      0x4

/*!
 *  @brief  Base addresses of L2_RAM for clock enable.
 */
#define L2_RAM_CLK_ENABLE       0x48140650

#define L2_RAM_CLK_ENABLE_SIZE  0x4
/*!
 *  @brief  Base addresses of PRCM module.
 */
#define PRM_BASE_ADDR           0x4Ae06000
/*!
 *  @brief  Base addresses of different peripherals.
 */

#define PRM_SIZE                0x00002000

/*!
 *  @brief  Base addresses of PRCM module.
 */
#define CM_BASE_ADDR            0x4A005000
/*!
 *  @brief  Base addresses of different peripherals.
 */

#define CM_SIZE                 0x00002000

#define MMU_BASE                0x40D01000
/*!
 *  @brief  size to be ioremapped.
 */
#define MMU_SIZE                0x1000


/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* Initializes Shared Driver/device. */
Int VAYUDSP_phyShmemInit (Ptr halObj);

/* Finalizes Shared Driver/device. */
Int VAYUDSP_phyShmemExit (Ptr halObj);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* VAYUDSPPHYSHMEM_H_0xbbed */
