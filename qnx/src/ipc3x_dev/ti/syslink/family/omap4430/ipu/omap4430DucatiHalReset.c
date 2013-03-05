/*
 *  @file   omap4430DucatiHalReset.c
 *
 *  @brief      Reset control module.
 *
 *              This module is responsible for handling reset-related hardware-
 *              specific operations.
 *              The implementation is specific to OMAP4430DUCATI.
 *
 *
 *  @ver        02.00.00.44_pre-alpha3
 *
 *  ============================================================================
 *
 *  Copyright (c) 2008-2009, Texas Instruments Incorporated
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

/*QNX specific header include */
#include <errno.h>

/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/Trace.h>
#include <Bitops.h>

/* Module level headers */
#include <_ProcDefs.h>
#include <Processor.h>
#include <OsalDrv.h>

/* Hardware Abstraction Layer headers */
#include <OMAP4430DucatiHal.h>
#include <OMAP4430DucatiHalReset.h>
#include "OMAP4430DucatiEnabler.h"
#include <hw/inout.h>
#include <_ipu_pm.h>
#include <ipu_pm.h>

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
#define GPT_IRQSTATUS_OFFSET 0x28

/* =============================================================================
 * APIs called by OMAP4430DUCATIPROC module
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
OMAP4430DUCATI_halResetCtrl (Ptr halObj, Processor_ResetCtrlCmd cmd,UInt32 entryPt)
{
    Int                  status    = PROCESSOR_SUCCESS;
    OMAP4430DUCATI_HalObject * halObject = NULL;
    UINT32 pa;
    int counter = 10;
    UINT32 M3RstCtrl;
    UINT32 M3ClkCtrl;
    UINT32 M3RstSt;
    UINT32 M3ClkStCtrl;
    Int ret;

    GT_3trace (curTrace, GT_ENTER, "OMAP4430DUCATI_halResetCtrl", halObj, cmd, entryPt);

    GT_assert (curTrace, (halObj != NULL));
    GT_assert (curTrace, (cmd < Processor_ResetCtrlCmd_EndValue));

    ULONG reg = 0;
    ULONG resets = 0;
    pa = RM_MPU_M3_RSTCTRL;
    M3RstCtrl = (UINT32)OsalDrv_ioMap(pa, sizeof(ULONG));

    pa = CM_MPU_M3_MPU_M3_CLKCTRL;
    M3ClkCtrl = (UINT32)OsalDrv_ioMap(pa, sizeof(ULONG));

    pa = RM_MPU_M3_RSTST;
    M3RstSt = (UINT32)OsalDrv_ioMap(pa, sizeof(ULONG));
    pa = CM_MPU_M3_CLKSTCTRL;
    M3ClkStCtrl = (UINT32)OsalDrv_ioMap(pa, sizeof(ULONG));

    halObject = (OMAP4430DUCATI_HalObject *) halObj ;

    switch (cmd) {
        case Processor_ResetCtrlCmd_Reset:
        {
            switch (halObject->procId) {
                case PROCTYPE_SYSM3:
                    /* Put SYSM3 into reset */
                    SETBITREG32(M3RstCtrl, RM_MPU_M3_RST1_BIT);
                    /* Read back the reset control register */
                    reg = INREG32(M3RstCtrl);
                    /* Disable the GPT3 clock, which is used by CORE0 */
                    ret = ipu_pm_gpt_stop(GPTIMER_3);
                    if (ret != EOK) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace, GT_4CLASS,
                                             "OMAP4430DUCATI_halResetCtrl",
                                             status,
                                             "Failed to stop gpt 3");
                    }
                    ret = ipu_pm_gpt_disable(GPTIMER_3);
                    if (ret != EOK) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace, GT_4CLASS,
                                             "OMAP4430DUCATI_halResetCtrl",
                                             status,
                                             "Failed to disable gpt 3");
                    }
                    break;
                case PROCTYPE_APPM3:
                    /* Put APPM3 into reset */
                    SETBITREG32(M3RstCtrl, RM_MPU_M3_RST2_BIT);
#ifndef SYSLINK_SYSBIOS_SMP
                    /* Disable the GPT4 clock, which is used by CORE1 */
                    ret = ipu_pm_gpt_stop(GPTIMER_4);
                    if (ret != EOK) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace, GT_4CLASS,
                                             "OMAP4430DUCATI_halResetCtrl",
                                             status,
                                             "Failed to stop gpt 4");
                    }
                    ret = ipu_pm_gpt_disable(GPTIMER_4);
                    if (ret != EOK) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace, GT_4CLASS,
                                             "OMAP4430DUCATI_halResetCtrl",
                                             status,
                                             "Failed to disable gpt 4");
                    }
#endif
                    break;
                default:
                    break;
            }
        }
        break;

        case Processor_ResetCtrlCmd_MMU_Reset:
        {
            switch (halObject->procId) {
                case PROCTYPE_SYSM3:
                    /* Put M3  MMU into reset */
                    SETBITREG32(M3RstCtrl, RM_MPU_M3_RST3_BIT);
                    /* Disable the M3 clock */
                    OUTREG32(M3ClkCtrl, 0x01);
                    break;
                case PROCTYPE_APPM3:
                    break;
                default:
                    break;
            }
        }
        break;

        case Processor_ResetCtrlCmd_MMU_Release:
        {
            reg = INREG32(M3RstSt);
            if (reg != 0x0) {
                Osal_printf("OMAP4430DUCATI_halResetCtrl: clearing reset status!");
                OUTREG32(M3RstSt,reg);
                do {
                    if ((reg = INREG32(M3RstSt)) == 0x0)
                        break;
                } while (--counter);

                if (reg == 0x0) {
                    Osal_printf("OMAP4430DUCATI_halResetCtrl: reset state reset!");
                }
                else {
                    status = PROCESSOR_E_FAIL;
                    GT_setFailureReason (curTrace, GT_4CLASS,
                                         "OMAP4430DUCATI_halResetCtrl", status,
                                         "Failed to clear reset status");
                }
            }
            if (status >= 0) {
                reg = INREG32(M3RstCtrl);
                Osal_printf("OMAP4430DUCATI_halResetCtrl: Reset Control [0x%x]",
                            reg);

                switch (halObject->procId) {
                    case PROCTYPE_SYSM3:
                        /* Module is managed automatically by HW */
                        OUTREG32(M3ClkCtrl,
                                 CM_MPU_M3_MPU_M3_CLKCTRL_MODULEMODE_HWAUTO);
                        /* Enable the M3 clock */
                        OUTREG32(M3ClkStCtrl, CM_MPU_M3_CLKSTCTRL_CTRL_SW_WKUP);

                        counter = 10;
                        do {
                            if (TESTBITREG32(M3ClkStCtrl,
                                         CM_MPU_M3_CLKSTCTRL_CLKACTIVITY_BIT)) {
                                Osal_printf("M3 clock enabled:"
                                            "CORE_CM2_DUCATI_CLKSTCTRL = 0x%x",
                                            INREG32(M3ClkStCtrl));
                                break;
                            }
                        } while (--counter);

                        if (counter == 0) {
                            Osal_printf("FAILED TO ENABLE DUCATI M3 CLOCK !");
                            return PROCESSOR_E_OSFAILURE;
                        }

                        /* Check that releasing resets would indeed be
                         * effective */
                        reg = INREG32(M3RstCtrl);
                        resets = RM_MPU_M3_RST3 | RM_MPU_M3_RST2 | RM_MPU_M3_RST1;
                        if (reg != resets) {
                            Osal_printf("OMAP4430DUCATI_halResetCtrl: "
                                        "Resets in not proper state! [0x%x]",
                                        reg);
                            OUTREG32(M3RstCtrl,resets);
                            counter = 10;
                            do {
                                if ((INREG32(M3RstCtrl) & resets) == resets)
                                    break;
                            } while (--counter);

                            if (counter == 0) {
                                status = PROCESSOR_E_FAIL;
                                GT_setFailureReason (curTrace, GT_4CLASS,
                                        "OMAP4430DUCATI_halResetCtrl",
                                        status,
                                        "Failed to put resets in proper state");
                            }
                        }

                        if (status >= 0) {
                            /* De-assert RST3, and clear the Reset status */
                            Osal_printf("De-assert RST3");
                            CLRBITREG32(M3RstCtrl, RM_MPU_M3_RST3_BIT);

                            counter = 10;
                            do {
                                if (INREG32(M3RstSt) & RM_MPU_M3_RST3ST)
                                    break;
                            } while (--counter);
                            if (counter == 0) {
                                status = PROCESSOR_E_FAIL;
                                GT_setFailureReason (curTrace, GT_4CLASS,
                                                  "OMAP4430DUCATI_halResetCtrl",
                                                  status,
                                                  "Failed to release RST3");
                            }
                            else {
                                Osal_printf("RST3 released!");
                                SETBITREG32(M3RstSt, RM_MPU_M3_RST3ST_BIT);
                            }
                        }
                        break;
                    case PROCTYPE_APPM3:
                        break;
                    default:
                        Osal_printf("proc4430_start: ERROR input");
                        break;
                }
            }
        }
        break;

        case Processor_ResetCtrlCmd_Release:
        {
            switch (halObject->procId) {
                case PROCTYPE_SYSM3:
                    /* Enable the GPT3 clock, which is used by CORE0 */
                    ret = ipu_pm_gpt_enable(GPTIMER_3);
                    if (ret != EOK) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace, GT_4CLASS,
                                             "OMAP4430DUCATI_halResetCtrl",
                                             status,
                                             "Failed to enable gpt 3");
                    }
                    else {
                        restore_gpt_context(GPTIMER_3);
                        ret = ipu_pm_gpt_start(GPTIMER_3);
                        if (ret != EOK) {
                            status = PROCESSOR_E_FAIL;
                            GT_setFailureReason (curTrace, GT_4CLASS,
                                                 "OMAP4430DUCATI_halResetCtrl",
                                                 status,
                                                 "Failed to start gpt 3");
                        }
                        else {
                            /* De-assert RST1, and clear the Reset status */
                            Osal_printf("De-assert RST1");
                            CLRBITREG32(M3RstCtrl, RM_MPU_M3_RST1_BIT);

                            counter = 10;
                            do {
                                if (INREG32(M3RstSt) & RM_MPU_M3_RST1)
                                    break;
                            } while (--counter);
                            if (counter == 0) {
                                status = PROCESSOR_E_FAIL;
                                GT_setFailureReason (curTrace, GT_4CLASS,
                                                  "OMAP4430DUCATI_halResetCtrl",
                                                  status,
                                                  "Failed to release RST1");
                            }
                            else {
                                Osal_printf("RST1 released!");
                                SETBITREG32(M3RstSt, RM_MPU_M3_RST1ST_BIT);

                                /* Setting to HW_AUTO Mode */
                                reg = INREG32(M3ClkStCtrl);
                                reg &= ~CM_MPU_M3_CLKSTCTRL_CTRL_BITMASK;
                                reg |= CM_MPU_M3_CLKSTCTRL_CTRL_HW_AUTO;
                                OUTREG32(M3ClkStCtrl, reg);
                            }
                        }
                    }
                    break;
                case PROCTYPE_APPM3:
#ifndef SYSLINK_SYSBIOS_SMP
                    /* Enable the GPT4 clock, which is used by CORE1 */
                    ret = ipu_pm_gpt_enable(GPTIMER_4);
                    if (ret != EOK) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace, GT_4CLASS,
                                             "OMAP4430DUCATI_halResetCtrl",
                                             status,
                                             "Failed to enable gpt 4");
                    }
                    else {
                        restore_gpt_context(GPTIMER_4);
                        ipu_pm_gpt_start(GPTIMER_4);
#endif

                        /* De-assert RST2, and clear the Reset status */
                        CLRBITREG32(M3RstCtrl, RM_MPU_M3_RST2_BIT);

                        counter = 10;
                        do {
                            if (INREG32(M3RstSt) & RM_MPU_M3_RST2)
                                break;
                        } while (--counter);
                        if (counter == 0) {
                            status = PROCESSOR_E_FAIL;
                            GT_setFailureReason (curTrace, GT_4CLASS,
                                                 "OMAP4430DUCATI_halResetCtrl",
                                                 status,
                                                 "Failed to release RST2");
                        }
                        else {
                            Osal_printf("RST2 released!");
                            SETBITREG32(M3RstSt, RM_MPU_M3_RST2ST_BIT);
                            /* Wait until ducati is in idle */
                            //while(TESTBITREG32(M3ClkStCtrl,
                            //            CM_MPU_M3_CLKSTCTRL_CLKACTIVITY_BIT));
                        }
#ifndef SYSLINK_SYSBIOS_SMP
                    }
#endif
                    break;
                default:
                    Osal_printf("OMAP430DUCATI_halResetCtrl: ERROR input");
                    break;
            }
        }
        break;

        default:
        {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATI_halResetCtrl",
                                 status,
                                 "Unsupported reset ctrl cmd specified");
        }
        break;
    }
    OsalDrv_ioUnmap(M3ClkStCtrl, sizeof(ULONG));
    OsalDrv_ioUnmap(M3ClkCtrl, sizeof(ULONG));
    OsalDrv_ioUnmap(M3RstCtrl, sizeof(ULONG));
    OsalDrv_ioUnmap(M3RstSt, sizeof(ULONG));
    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATI_halResetCtrl", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */

    return status;
}


#if defined (__cplusplus)
}
#endif
