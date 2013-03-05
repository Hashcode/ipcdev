/*
 *  @file   Dm8168DspPhyShmem.c
 *
 *  @brief      Linux shared memory physical interface file for DM8168DSP.
 *
 *              This module is responsible for handling boot-related hardware-
 *              specific operations.
 *              The implementation is specific to DM8168DSP.
 *
 *
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





#if defined(SYSLINK_BUILD_HLOS)
/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL and utils */
#include <ti/syslink/utils/Memory.h>
#endif /* #if defined(SYSLINK_BUILD_HLOS) */

#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Error.h>
#include <ti/syslink/utils/_Memory.h>
#endif /* #if defined(SYSLINK_BUILD_RTOS) */

#include <ti/syslink/utils/Trace.h>

/* Module headers */
#include <_ProcDefs.h>
#include <Processor.h>


/* Hardware Abstraction Layer headers */
#include <Dm8168DspHal.h>
#include <Dm8168DspHalBoot.h>
#include <Dm8168DspHalReset.h>
#include <Dm8168DspPhyShmem.h>
#include <Dm8168DspHalMmu.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */


/* =============================================================================
 * APIs called by DM8168DSPPROC module
 * =============================================================================
 */
/*!
 *  @brief      Function to initialize Shared Driver/device.
 *
 *  @param      halObj  Pointer to the HAL object
 *
 *  @sa         DM8168DSP_phyShmemExit
 *              Memory_map
 */
Int
DM8168DSP_phyShmemInit (Ptr halObj)
{
    Int                  status    = PROCESSOR_SUCCESS;
    DM8168DSP_HalObject * halObject = NULL;
    Memory_MapInfo       mapInfo;

    GT_1trace(curTrace, GT_ENTER,
        "--> DM8168DSP_phyShmemInit: halObj=0x%x", halObj);

    GT_assert (curTrace,(halObj != NULL));

    halObject = (DM8168DSP_HalObject *) halObj;

    mapInfo.src = DSP_BOOT_ADDR;
    mapInfo.size = DSP_BOOT_ADDR_SIZE;
    mapInfo.isCached = FALSE;

    status = Memory_map (&mapInfo);

    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->generalCtrlBase = 0;
    }
    else {
        halObject->generalCtrlBase = mapInfo.dst;
    }

    mapInfo.src      = DSP_BOOT_STAT;
    mapInfo.size     = DSP_BOOT_STAT_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->bootStatBase = 0;
    }
    else {
        halObject->bootStatBase = mapInfo.dst;
    }

    mapInfo.src      = L2_RAM_CLK_ENABLE;
    mapInfo.size     = L2_RAM_CLK_ENABLE_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->l2ClkBase = 0;
    }
    else {
        halObject->l2ClkBase = mapInfo.dst;
    }

    mapInfo.src      = PRCM_BASE_ADDR;
    mapInfo.size     = PRCM_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->prcmBase = 0;
    }
    else {
        halObject->prcmBase = mapInfo.dst;
    }
/* TO BE IMPLEMENTED
    mapInfo.src      = MMU_BASE;
    mapInfo.size     = MMU_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->mmuBase = 0;
    }
    else {
        halObject->mmuBase = mapInfo.dst;
    }
*/
    GT_1trace(curTrace, GT_LEAVE, "<-- DM8168DSP_phyShmemInit: 0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to finalize Shared Driver/device.
 *
 *  @param      halObj  Pointer to the HAL object
 *
 *  @sa         DM8168DSP_phyShmemInit
 *              Memory_Unmap
 */
Int
DM8168DSP_phyShmemExit (Ptr halObj)
{
    Int                  status    = PROCESSOR_SUCCESS;
    DM8168DSP_HalObject * halObject = NULL;
    Memory_UnmapInfo     unmapInfo;

    GT_1trace (curTrace, GT_ENTER, "DM8168DSP_phyShmemExit", halObj);

    GT_assert (curTrace, (halObj != NULL));

    halObject = (DM8168DSP_HalObject *) halObj;

    unmapInfo.addr = halObject->prcmBase;
    unmapInfo.size = PRCM_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168DSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->mmuBase = 0 ;
    }

    unmapInfo.addr = halObject->l2ClkBase;
    unmapInfo.size = L2_RAM_CLK_ENABLE_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168DSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->l2ClkBase = 0 ;
    }

    unmapInfo.addr = halObject->bootStatBase;
    unmapInfo.size = DSP_BOOT_STAT_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168DSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->bootStatBase = 0 ;
    }

    unmapInfo.addr = halObject->generalCtrlBase;
    unmapInfo.size = DSP_BOOT_ADDR_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168DSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->generalCtrlBase = 0 ;
    }


    GT_1trace (curTrace, GT_LEAVE, "DM8168DSP_phyShmemExit",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


#if defined (__cplusplus)

}
#endif
