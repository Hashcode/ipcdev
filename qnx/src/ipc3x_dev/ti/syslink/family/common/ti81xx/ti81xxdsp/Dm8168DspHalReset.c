/*
 *  @file   Dm8168DspHalReset.c
 *
 *  @brief      Reset control module.
 *
 *              This module is responsible for handling reset-related hardware-
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



#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#endif /* #if defined(SYSLINK_BUILD_RTOS) */

#if defined(SYSLINK_BUILD_HLOS)
#include <ti/syslink/Std.h>
#endif /* #if defined(SYSLINK_BUILD_HLOS) */

#include <ti/syslink/utils/Trace.h>


#include <Bitops.h>
/* Module level headers */
#include <_ProcDefs.h>
#include <Processor.h>

/* Hardware Abstraction Layer headers */
#include <Dm8168DspHal.h>
#include <Dm8168DspHalReset.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  PRM address for GEM
 */
#define CM_GEM_CLKSTCTRL      0x400
/*!
 *  @brief  Clock mgmt for GEM
 */
#define CM_ACTIVE_GEM_CLKCTRL 0x420
/*!
 *  @brief  Reset control for GEM
 */
#define RM_ACTIVE_RSTCTRL     0xA10
/*!
 *  @brief  Reset status for GEM
 */
#define RM_ACTIVE_RSTST       0xA14

/* =============================================================================
 * APIs called by DM8168DSPPROC module
 * =============================================================================
 */
/*!
 *  @brief      Function to control reset operations
 *
 *  @param      halObj  Pointer to the HAL object
 *  @param      cmd     Reset control command
 *  @param      arg     Arguments specific to the reset control command
 *
 *  @sa
 */
Int
Dm8168DspHal_reset(Ptr halObj, Dm8168DspHal_ResetCmd cmd)
{
    Int                     status = PROCESSOR_SUCCESS;
    DM8168DSP_HalObject *   halObject;
    UInt32                  prcmBase;
    UInt32                  addr;
    UInt32                  val;

    GT_2trace(curTrace, GT_ENTER, "--> Dm8168DspHal_reset", halObj, cmd);

    GT_assert(curTrace, (halObj != NULL));
    GT_assert(curTrace, (cmd < Dm8168DspHal_Reset_EndValue));

    halObject = (DM8168DSP_HalObject *)halObj;
    prcmBase = halObject->prcmBase;

    switch (cmd) {
        case Dm8168DspHal_Reset_Attach:
        {
            /* assert GEM global and cpu resets */
            addr = prcmBase + RM_ACTIVE_RSTCTRL;
            REG(addr) = 0x3;

            /* clear status bit, write-1 to clear bit */
            addr = prcmBase + RM_ACTIVE_RSTST;
            REG(addr) = 0x2;

            /* release GEM global reset */
            addr = prcmBase + RM_ACTIVE_RSTCTRL;
            val = REG(addr);
            val &= ~(0x2);
            REG(addr) = val;

            /* wait for reset done flag */
            addr = prcmBase + RM_ACTIVE_RSTST;
            do {
                val = REG(addr);
                val &= 0x2;
            } while(val != 0x2);
        }
        break;

        case Dm8168DspHal_Reset_Start:
        {
            /* clear status bit, write-1 to clear bit */
            addr = prcmBase + RM_ACTIVE_RSTST;
            REG(addr) = 1;

            /* release cpu reset (and global to ensure proper boot) */
            addr = prcmBase + RM_ACTIVE_RSTCTRL;
            REG(addr) = 0;

            /* wait for reset done flag */
            addr = prcmBase + RM_ACTIVE_RSTST;
            do {
                val = REG(addr);
                val &= 0x1;
            } while(val != 0x1);
        }
        break;

        case Dm8168DspHal_Reset_Stop:
        {
            /* assert only cpu reset */
            addr = prcmBase + RM_ACTIVE_RSTCTRL;
            REG(addr) = 0x1;
        }
        break;

        case Dm8168DspHal_Reset_Detach:
        {
            /* assert both GEM global and cpu resets */
            addr = prcmBase + RM_ACTIVE_RSTCTRL;
            REG(addr) = 0x3;
        }
        break;

        default:
        {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason(curTrace, GT_4CLASS,
                "DM8168DSP_halResetCtrl", status,
                "Unsupported reset ctrl cmd specified");
        }
        break;
    }

    GT_1trace(curTrace, GT_LEAVE, "<-- Dm8168DspHal_reset", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


#if defined (__cplusplus)
}
#endif
