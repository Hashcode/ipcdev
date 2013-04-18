/*
 *  @file   VAYUDspPhyShmem.c
 *
 *  @brief      Shared memory physical interface file for VAYUDSP.
 *
 *              This module is responsible for handling boot-related hardware-
 *              specific operations.
 *              The implementation is specific to VAYUDSP.
 *
 *
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



/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL and utils */
#include <ti/syslink/utils/Memory.h>

#include <ti/syslink/utils/Trace.h>

/* Module headers */
#include <_ProcDefs.h>
#include <Processor.h>


/* Hardware Abstraction Layer headers */
#include <VAYUDspHal.h>
#include <VAYUDspHalBoot.h>
#include <VAYUDspHalReset.h>
#include <VAYUDspPhyShmem.h>
#include <VAYUDspHalMmu.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */


/* =============================================================================
 * APIs called by VAYUDSPPROC module
 * =============================================================================
 */
/*!
 *  @brief      Function to initialize Shared Driver/device.
 *
 *  @param      halObj  Pointer to the HAL object
 *
 *  @sa         VAYUDSP_phyShmemExit
 *              Memory_map
 */
Int
VAYUDSP_phyShmemInit (Ptr halObj)
{
    Int                  status    = PROCESSOR_SUCCESS;
    VAYUDSP_HalObject *  halObject = NULL;
    Memory_MapInfo       mapInfo;

    GT_1trace (curTrace, GT_ENTER, "VAYUDSP_phyShmemInit", halObj);

    GT_assert (curTrace,(halObj != NULL));

    halObject = (VAYUDSP_HalObject *) halObj;

    mapInfo.src = DSP_BOOT_ADDR;
    mapInfo.size = DSP_BOOT_ADDR_SIZE;
    mapInfo.isCached = FALSE;

    status = Memory_map (&mapInfo);

    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "VAYUDSP_phyShmemInit",
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
                             "VAYUDSP_phyShmemInit",
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
                             "VAYUDSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->l2ClkBase = 0;
    }
    else {
        halObject->l2ClkBase = mapInfo.dst;
    }

    mapInfo.src      = CM_BASE_ADDR;
    mapInfo.size     = CM_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "VAYUDSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->cmBase = 0;
    }
    else {
        halObject->cmBase = mapInfo.dst;
    }

    mapInfo.src      = PRM_BASE_ADDR;
    mapInfo.size     = PRM_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "VAYUDSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->prmBase = 0;
    }
    else {
        halObject->prmBase = mapInfo.dst;
    }

    mapInfo.src      = MMU_BASE;
    mapInfo.size     = MMU_SIZE;
    mapInfo.isCached = FALSE;
    status = Memory_map (&mapInfo);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "VAYUDSP_phyShmemInit",
                             status,
                             "Failure in Memory_map for MMU base registers");
        halObject->mmuBase = 0;
    }
    else {
        halObject->mmuBase = mapInfo.dst;
    }

    GT_1trace(curTrace, GT_LEAVE, "<-- VAYUDSP_phyShmemInit: 0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to finalize Shared Driver/device.
 *
 *  @param      halObj  Pointer to the HAL object
 *
 *  @sa         VAYUDSP_phyShmemInit
 *              Memory_Unmap
 */
Int
VAYUDSP_phyShmemExit (Ptr halObj)
{
    Int                  status    = PROCESSOR_SUCCESS;
    VAYUDSP_HalObject *  halObject = NULL;
    Memory_UnmapInfo     unmapInfo;

    GT_1trace (curTrace, GT_ENTER, "VAYUDSP_phyShmemExit", halObj);

    GT_assert (curTrace, (halObj != NULL));

    halObject = (VAYUDSP_HalObject *) halObj;

    unmapInfo.addr = halObject->mmuBase;
    unmapInfo.size = MMU_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "VAYUDSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->mmuBase = 0 ;
    }

    unmapInfo.addr = halObject->cmBase;
    unmapInfo.size = CM_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "VAYUDSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for CM base registers");
        }
        halObject->cmBase = 0 ;
    }

    unmapInfo.addr = halObject->prmBase;
    unmapInfo.size = PRM_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "VAYUDSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->prmBase = 0 ;
    }

    unmapInfo.addr = halObject->l2ClkBase;
    unmapInfo.size = L2_RAM_CLK_ENABLE_SIZE;
    unmapInfo.isCached = FALSE;
    if (unmapInfo.addr != 0) {
        status = Memory_unmap (&unmapInfo);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "VAYUDSP_phyShmemExit",
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
                              "VAYUDSP_phyShmemExit",
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
                              "VAYUDSP_phyShmemExit",
                              status,
                              "Failure in Memory_Unmap for MMU base registers");
        }
        halObject->generalCtrlBase = 0 ;
    }


    GT_1trace (curTrace, GT_LEAVE, "VAYUDSP_phyShmemExit",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


#if defined (__cplusplus)

}
#endif
