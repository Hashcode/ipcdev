/*
 *  @file   VAYUIpuCore1HalReset.c
 *
 *  @brief      Reset control module.
 *
 *              This module is responsible for handling reset-related hardware-
 *              specific operations.
 *              The implementation is specific to VAYUIPUCORE1.
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



#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/Trace.h>
#include <Bitops.h>

/* Module level headers */
#include <_ProcDefs.h>
#include <Processor.h>

/* Hardware Abstraction Layer headers */
#include <VAYUIpuHal.h>
#include <VAYUIpuCore1HalReset.h>

#include <hw/inout.h>

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/* M4 PRCM Regs*/
#define RM_IPU_RSTCTRL_OFFSET        0x210
#define RM_IPU_RSTST_OFFSET          0x214

#define CM_IPU_CLKSTCTRL_OFFSET      0x200
#define CM_IPU_IPU_CLKCTRL_OFFSET    0x220


/* =============================================================================
 * APIs called by VAYUIPUCORE1PROC module
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
VAYUIPUCORE1_halResetCtrl (Ptr halObj, Processor_ResetCtrlCmd cmd)
{
    Int                 status    = PROCESSOR_SUCCESS;
    VAYUIPU_HalObject * halObject = NULL;
    UInt32              cmBase;
    UInt32              prmBase;
    UInt32              reg       = 0;

    GT_2trace (curTrace, GT_ENTER, "VAYUIPUCORE1_halResetCtrl", halObj, cmd);

    GT_assert (curTrace, (halObj != NULL));
    GT_assert (curTrace, (cmd < Processor_ResetCtrlCmd_EndValue));

    halObject = (VAYUIPU_HalObject *) halObj ;
    cmBase = halObject->cmBase;
    prmBase = halObject->prmBase;

    switch (cmd) {
        case Processor_ResetCtrlCmd_Reset:
        {
            /*Put ONLY Benelli M4_1 to Reset*/
            /* Put IPU core 1 into reset */
            SETBITREG32(prmBase + RM_IPU_RSTCTRL_OFFSET, 1);
            /* Read back the reset control register */
            reg = INREG32(prmBase + RM_IPU_RSTCTRL_OFFSET);
        }
        break ;

        case Processor_ResetCtrlCmd_MMU_Reset:
        {
            /* Do nothing, handled by M4_0 MMU_Reset */
        }
        break;

        case Processor_ResetCtrlCmd_MMU_Release:
        {
            /* Do nothing, handled by M4_0 MMU_Release */
        }
        break;

        case Processor_ResetCtrlCmd_Release:
        {
            /*Bring ONLY Benelli M4_1 out of Reset*/
            /* De-assert RST1, and clear the Reset status */
            Osal_printf("De-assert RST2\n");
            CLRBITREG32(prmBase + RM_IPU_RSTCTRL_OFFSET, 1);

#ifndef VAYU_VIRTIO // skip this check for now
            while (!(INREG32(prmBase + RM_IPU_RSTST_OFFSET) & 0x2));
            Osal_printf("RST2 released!");
            SETBITREG32(prmBase + RM_IPU_RSTST_OFFSET, 1);
#endif

            /* Setting to HW_AUTO Mode */
            reg = INREG32(cmBase + CM_IPU_CLKSTCTRL_OFFSET);
            reg &= ~0x3;
            reg |= 0x3;
            OUTREG32(cmBase + CM_IPU_CLKSTCTRL_OFFSET, reg);
        }
        break;

        case Processor_ResetCtrlCmd_PeripheralUp:
        {
            /* Nothing to be done to bringup the peripherals for this device. */
        }
        break;

        default:
        {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "VAYUIPUCORE1_halResetCtrl",
                                 status,
                                 "Unsupported reset ctrl cmd specified");
        }
        break ;
    }

    GT_1trace (curTrace, GT_LEAVE, "VAYUIPUCORE1_halResetCtrl",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


#if defined (__cplusplus)
}
#endif
