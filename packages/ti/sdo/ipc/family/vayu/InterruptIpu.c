/*
 * Copyright (c) 2013, Texas Instruments Incorporated
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
 */
/*
 *  ======== InterruptIpu.c ========
 *  Vayu IPU based interupt manager
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/family/shared/vayu/IntXbar.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>
#include <ti/sysbios/family/arm/ducati/Core.h>
#include <ti/sdo/ipc/notifyDrivers/IInterrupt.h>
#include <ti/sdo/utils/_MultiProc.h>

#include "package/internal/InterruptIpu.xdc.h"

/* Register access method. */
#define REG16(A)   (*(volatile UInt16 *) (A))
#define REG32(A)   (*(volatile UInt32 *) (A))

/*
 *  Ducati control register that maintains inter-core interrupt bits.
 *
 *  Using separate CORE0 and CORE1 values to do 16-bit reads/writes
 *  because we do not want to overwrite the other cores value.
 */
#define INTERRUPT_CORE0         (InterruptIpu_ducatiCtrlBaseAddr)
#define INTERRUPT_CORE1         (InterruptIpu_ducatiCtrlBaseAddr + 2)

#define PROCID(IDX)               (InterruptIpu_procIdTable[(IDX)])
#define MBX_TABLE_IDX(SRC, DST)   ((PROCID(SRC) * InterruptIpu_NUM_CORES) + \
                                    PROCID(DST))
#define SUBMBX_IDX(IDX)           (InterruptIpu_mailboxTable[(IDX)] & 0xFF)
#define MBX_USER_IDX(IDX)         ((InterruptIpu_mailboxTable[(IDX)] >> 8) \
                                    & 0xFF)
#define MBX_BASEADDR_IDX(IDX)    ((InterruptIpu_mailboxTable[(IDX)] >> 16) \
                                    & 0xFFFF)

#define MAILBOX_REG_VAL(M)        (0x1 << (2 * M))

#define MAILBOX_MESSAGE(IDX)      (InterruptIpu_mailboxBaseAddr[  \
                                    MBX_BASEADDR_IDX(IDX)] + 0x40 +   \
                                    (0x4 * SUBMBX_IDX(IDX)))
#define MAILBOX_STATUS(IDX)       (InterruptIpu_mailboxBaseAddr[  \
                                    MBX_BASEADDR_IDX(IDX)] + 0xC0 +   \
                                    (0x4 * SUBMBX_IDX(IDX)))

#define MAILBOX_IRQSTATUS_CLR(IDX)   (InterruptIpu_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + (0x10 * \
                                        MBX_USER_IDX(IDX)) + 0x104)
#define MAILBOX_IRQENABLE_SET(IDX)   (InterruptIpu_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + (0x10 * \
                                        MBX_USER_IDX(IDX)) + 0x108)
#define MAILBOX_IRQENABLE_CLR(IDX)   (InterruptIpu_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + (0x10 * \
                                        MBX_USER_IDX(IDX)) + 0x10C)
#define MAILBOX_EOI_REG(IDX)         (InterruptIpu_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + 0x140)

#define WUGENIPU        19

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*
 *  ======== InterruptIpu_Module_startup ========
 */
Int InterruptIpu_Module_startup(Int phase)
{
    if (IntXbar_Module_startupDone()) {
        /* connect mailbox interrupts at startup */
        if (Core_ipuId == 1) {
            if ((BIOS_smpEnabled) || (Core_getId() == 0)) {
                /* IPU1-0 */
                IntXbar_connect(42, 285);  // eve1 mailbox 0 user 2
                IntXbar_connect(43, 294);  // eve2 mailbox 0 user 2
                IntXbar_connect(44, 250);  // system mailbox 5 user 1
                InterruptIpu_module->interruptTable[4] = 66;    // DSP1
                InterruptIpu_module->interruptTable[8] = 66;    // HOST
                InterruptIpu_module->interruptTable[9] = 19;    // IPU1-1

                /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
                if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
                    IntXbar_connect(45, 303);  // eve3 mailbox 0 user 2
                    IntXbar_connect(46, 312);  // eve4 mailbox 0 user 2
                }

                /* plug mbx7 only if DSP2 or IPU2 exists */
                if ((MultiProc_getId("DSP2") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU2") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU2-0") != MultiProc_INVALIDID)) {
                    IntXbar_connect(47, 259);  // system mailbox 7 user 2
                    InterruptIpu_module->interruptTable[5] = 69;    // DSP2
                    InterruptIpu_module->interruptTable[7] = 69;    // IPU2-0
                }

                /* plug mbx8 only if IPU2-1 exists */
                if (MultiProc_getId("IPU2-1") != MultiProc_INVALIDID) {
                    IntXbar_connect(48, 263);  // system mailbox 8 user 2
                    InterruptIpu_module->interruptTable[10] = 70;    // IPU2-1
                }
            }
            else { /* IPU1-1 */
                IntXbar_connect(49, 289);  // eve1 mailbox 1 user 3
                IntXbar_connect(50, 298);  // eve2 mailbox 1 user 3
                IntXbar_connect(51, 252);  // system mailbox 5 user 3
                InterruptIpu_module->interruptTable[4] = 73;    // DSP1
                InterruptIpu_module->interruptTable[8] = 73;    // HOST
                InterruptIpu_module->interruptTable[6] = 19;    // IPU1-0

                /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
                if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
                    IntXbar_connect(52, 307);  // eve3 mailbox 1 user 3
                    IntXbar_connect(53, 316);  // eve4 mailbox 1 user 3
                }

                /* plug mbx8 only if DSP2 or IPU2 exists */
                if ((MultiProc_getId("DSP2") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU2") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU2-0") != MultiProc_INVALIDID)) {
                    IntXbar_connect(54, 263);  // system mailbox 8 user 2
                    InterruptIpu_module->interruptTable[5] = 76;    // DSP2
                    InterruptIpu_module->interruptTable[7] = 76;    // IPU2-0
                }
            }
        }
        else {
            if ((BIOS_smpEnabled) || (Core_getId() == 0)) {
                /* IPU2-0 */
                IntXbar_connect(42, 288);  // eve1 mailbox 1 user 2
                IntXbar_connect(43, 297);  // eve2 mailbox 1 user 2
                IntXbar_connect(44, 254);  // system mailbox 6 user 1
                InterruptIpu_module->interruptTable[5] = 66;    // DSP2
                InterruptIpu_module->interruptTable[8] = 66;    // HOST
                InterruptIpu_module->interruptTable[10] = 19;    // IPU2-1

                /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
                if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
                    IntXbar_connect(45, 306);  // eve3 mailbox 1 user 2
                    IntXbar_connect(46, 315);  // eve4 mailbox 1 user 2
                }

                /* plug mbx7 only if DSP1 or IPU1 exists */
                if ((MultiProc_getId("DSP1") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU1") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU1-0") != MultiProc_INVALIDID)) {
                    IntXbar_connect(47, 260);  // system mailbox 7 user 3
                    InterruptIpu_module->interruptTable[4] = 69;    // DSP1
                    InterruptIpu_module->interruptTable[6] = 69;    // IPU1-0
                }

                /* plug mbx8 only if IPU1-1 exists */
                if (MultiProc_getId("IPU1-1") != MultiProc_INVALIDID) {
                    IntXbar_connect(48, 264);  // system mailbox 8 user 3
                    InterruptIpu_module->interruptTable[9] = 70;    // IPU1-1
                }
            }
            else { /* IPU2-1 */
                IntXbar_connect(49, 289);  // eve1 mailbox 1 user 3
                IntXbar_connect(50, 298);  // eve2 mailbox 1 user 3
                IntXbar_connect(51, 256);  // system mailbox 6 user 3
                InterruptIpu_module->interruptTable[5] = 73;    // DSP2
                InterruptIpu_module->interruptTable[8] = 73;    // HOST
                InterruptIpu_module->interruptTable[7] = 19;    // IPU2-0

                /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
                if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
                    IntXbar_connect(52, 307);  // eve3 mailbox 1 user 3
                    IntXbar_connect(53, 316);  // eve4 mailbox 1 user 3
                }

                /* plug mbx8 only if DSP2 or IPU2 exists */
                if ((MultiProc_getId("DSP1") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU1") != MultiProc_INVALIDID) ||
                    (MultiProc_getId("IPU1-0") != MultiProc_INVALIDID)) {
                    IntXbar_connect(54, 264);  // system mailbox 8 user 3
                    InterruptIpu_module->interruptTable[4] = 76;    // DSP1
                    InterruptIpu_module->interruptTable[6] = 76;    // IPU1-0
                }
            }
        }

        return (Startup_DONE);
    }

    return (Startup_NOTDONE);
}

/*
 *  ======== InterruptIpu_intEnable ========
 *  Enable remote processor interrupt
 */
Void InterruptIpu_intEnable(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt16 index;
    Bool useMailbox = TRUE;
    UInt8 subMbxIdx;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    if (Core_ipuId == 1) {
        if ((remoteProcId == InterruptIpu_ipu1_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu1_1ProcId)) {
            Hwi_enableInterrupt(WUGENIPU);
            useMailbox = FALSE;
        }
    }
    else {
        if ((remoteProcId == InterruptIpu_ipu2_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu2_1ProcId)) {
            Hwi_enableInterrupt(WUGENIPU);
            useMailbox = FALSE;
        }
    }

    /*  If the remote processor communicates via mailboxes, we should enable
     *  the Mailbox IRQ instead of enabling the Hwi because multiple mailboxes
     *  share the same Hwi
     */
    if (useMailbox) {
        subMbxIdx = SUBMBX_IDX(index);
        REG32(MAILBOX_IRQENABLE_SET(index)) = MAILBOX_REG_VAL(subMbxIdx);
    }
}

/*
 *  ======== InterruptIpu_intDisable ========
 *  Disables remote processor interrupt
 */
Void InterruptIpu_intDisable(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt16 index;
    Bool useMailbox = TRUE;
    UInt8 subMbxIdx;

    if (Core_ipuId == 1) {
        if ((remoteProcId == InterruptIpu_ipu1_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu1_1ProcId)) {
            Hwi_disableInterrupt(WUGENIPU);
            useMailbox = FALSE;
        }
    }
    else {
        if ((remoteProcId == InterruptIpu_ipu2_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu2_1ProcId)) {
            Hwi_disableInterrupt(WUGENIPU);
            useMailbox = FALSE;
        }
    }

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    /*  If the remote processor communicates via mailboxes, we should disable
     *  the Mailbox IRQ instead of disabling the Hwi because multiple mailboxes
     *  share the same Hwi
     */
    if (useMailbox) {
        subMbxIdx = SUBMBX_IDX(index);
        REG32(MAILBOX_IRQENABLE_CLR(index)) = MAILBOX_REG_VAL(subMbxIdx);
    }
}

/*
 *  ======== InterruptIpu_intRegister ========
 */
Void InterruptIpu_intRegister(UInt16 remoteProcId,
                              IInterrupt_IntInfo *intInfo,
                              Fxn func, UArg arg)
{
    Hwi_Params  hwiAttrs;
    UInt        key;
    UInt        mbxIdx;
    Int         index;
    InterruptIpu_FxnTable *table;

    Assert_isTrue(remoteProcId < ti_sdo_utils_MultiProc_numProcessors,
            ti_sdo_utils_MultiProc_A_invalidMultiProcId);

    mbxIdx = MBX_BASEADDR_IDX(MBX_TABLE_IDX(remoteProcId, MultiProc_self()));

    index = PROCID(remoteProcId);

    intInfo->localIntId = InterruptIpu_module->interruptTable[index];

    /* Disable global interrupts */
    key = Hwi_disable();

    table = &(InterruptIpu_module->fxnTable[index]);
    table->func = func;
    table->arg  = arg;

    InterruptIpu_intClear(remoteProcId, intInfo);

    Hwi_Params_init(&hwiAttrs);
    hwiAttrs.maskSetting = Hwi_MaskingOption_LOWER;

    if (Core_ipuId == 1) {
        if ((remoteProcId == InterruptIpu_ipu1_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu1_1ProcId)) {
            Hwi_create(intInfo->localIntId,
                   (Hwi_FuncPtr)InterruptIpu_intShmDucatiStub,
                   &hwiAttrs,
                   NULL);
        }
        else {
            /* Make sure the interrupt only gets plugged once */
            InterruptIpu_module->numPlugged[mbxIdx]++;
            if (InterruptIpu_module->numPlugged[mbxIdx] == 1) {
                Hwi_create(intInfo->localIntId,
                           (Hwi_FuncPtr)InterruptIpu_intShmMbxStub,
                            &hwiAttrs,
                            NULL);

                /* Interrupt_intEnable won't enable the Hwi */
                Hwi_enableInterrupt(intInfo->localIntId);
            }
        }
    }
    else {
        if ((remoteProcId == InterruptIpu_ipu2_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu2_1ProcId)) {
            Hwi_create(intInfo->localIntId,
                   (Hwi_FuncPtr)InterruptIpu_intShmDucatiStub,
                   &hwiAttrs,
                   NULL);
        }
        else {
            /* Make sure the interrupt only gets plugged once */
            InterruptIpu_module->numPlugged[mbxIdx]++;
            if (InterruptIpu_module->numPlugged[mbxIdx] == 1) {
                Hwi_create(intInfo->localIntId,
                           (Hwi_FuncPtr)InterruptIpu_intShmMbxStub,
                            &hwiAttrs,
                            NULL);

                /* Interrupt_intEnable won't enable the Hwi */
                Hwi_enableInterrupt(intInfo->localIntId);
            }
        }
    }

    InterruptIpu_intEnable(remoteProcId, intInfo);

    /* Restore global interrupts */
    Hwi_restore(key);
}

/*
 *  ======== InterruptIpu_intUnregister ========
 */
Void InterruptIpu_intUnregister(UInt16 remoteProcId,
                                IInterrupt_IntInfo *intInfo)
{
    UInt                       mbxIdx;
    Int                        index;
    InterruptIpu_FxnTable *table;
    Hwi_Handle                 hwiHandle;

    mbxIdx = MBX_BASEADDR_IDX(MBX_TABLE_IDX(remoteProcId, MultiProc_self()));

    index = PROCID(remoteProcId);

    /* Disable the mailbox interrupt source */
    InterruptIpu_intDisable(remoteProcId, intInfo);

    if (Core_ipuId == 1) {
        if ((remoteProcId == InterruptIpu_ipu1_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu1_1ProcId)) {
            hwiHandle = Hwi_getHandle(WUGENIPU);
            Hwi_delete(&hwiHandle);
        }
        else {
            /* Disable the interrupt itself */
            InterruptIpu_module->numPlugged[mbxIdx]--;
            if (InterruptIpu_module->numPlugged[mbxIdx] == 0) {
                hwiHandle = Hwi_getHandle(intInfo->localIntId);
                Hwi_delete(&hwiHandle);
            }
        }
    }
    else {
        if ((remoteProcId == InterruptIpu_ipu2_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu2_1ProcId)) {
            hwiHandle = Hwi_getHandle(WUGENIPU);
            Hwi_delete(&hwiHandle);
        }
        else {
            /* Disable the interrupt itself */
            InterruptIpu_module->numPlugged[mbxIdx]--;
            if (InterruptIpu_module->numPlugged[mbxIdx] == 0) {
                hwiHandle = Hwi_getHandle(intInfo->localIntId);
                Hwi_delete(&hwiHandle);
            }
        }
    }

    /* Clear the FxnTable entry for the remote processor */
    table = &(InterruptIpu_module->fxnTable[index]);
    table->func = NULL;
    table->arg  = 0;
}


/*
 *  ======== InterruptIpu_intSend ========
 *  Send interrupt to the remote processor
 */
Void InterruptIpu_intSend(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo,
                             UArg arg)
{
    UInt key;
    UInt16 index;

    if (Core_ipuId == 1) {
        if ((remoteProcId == InterruptIpu_ipu1_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu1_1ProcId)) {
            if (!(BIOS_smpEnabled) && (Core_getId() == 1)) {
                /* CORE1 to CORE0 */
                REG16(INTERRUPT_CORE0) |= 0x1;
            }
            else {
                /* CORE0 to CORE1 */
                REG16(INTERRUPT_CORE1) |= 0x1;
            }
        }
        else {
            index = MBX_TABLE_IDX(MultiProc_self(), remoteProcId);
            key = Hwi_disable();
            if (REG32(MAILBOX_STATUS(index)) == 0) {
                REG32(MAILBOX_MESSAGE(index)) = arg;
            }
            Hwi_restore(key);
        }
    }
    else {
        if ((remoteProcId == InterruptIpu_ipu2_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu2_1ProcId)) {
            if (!(BIOS_smpEnabled) && (Core_getId() == 1)) {
                /* CORE1 to CORE0 */
                REG16(INTERRUPT_CORE0) |= 0x1;
            }
            else {
                /* CORE0 to CORE1 */
                REG16(INTERRUPT_CORE1) |= 0x1;
            }
        }
        else {
            index = MBX_TABLE_IDX(MultiProc_self(), remoteProcId);
            key = Hwi_disable();
            if (REG32(MAILBOX_STATUS(index)) == 0) {
                REG32(MAILBOX_MESSAGE(index)) = arg;
            }
            Hwi_restore(key);
        }
    }
}


/*
 *  ======== InterruptIpu_intPost ========
 *  Simulate an interrupt from a remote processor
 */
Void InterruptIpu_intPost(UInt16 srcProcId, IInterrupt_IntInfo *intInfo,
                             UArg arg)
{
    UInt key;
    UInt16 index;

    if (Core_ipuId == 1) {
        if ((srcProcId == InterruptIpu_ipu1_0ProcId) ||
            (srcProcId == InterruptIpu_ipu1_1ProcId)) {
            if (!(BIOS_smpEnabled) && (Core_getId() == 1)) {
                /* CORE0 to CORE1 */
                REG16(INTERRUPT_CORE1) |= 0x1;
            }
            else {
                /* CORE1 to CORE0 */
                REG16(INTERRUPT_CORE0) |= 0x1;
            }
        }
        else {
            index = MBX_TABLE_IDX(srcProcId, MultiProc_self());
            key = Hwi_disable();
            if (REG32(MAILBOX_STATUS(index)) == 0) {
                REG32(MAILBOX_MESSAGE(index)) = arg;
            }
            Hwi_restore(key);
        }
    }
    else {
        if ((srcProcId == InterruptIpu_ipu2_0ProcId) ||
            (srcProcId == InterruptIpu_ipu2_1ProcId)) {
            if (!(BIOS_smpEnabled) && (Core_getId() == 1)) {
                /* CORE0 to CORE1 */
                REG16(INTERRUPT_CORE1) |= 0x1;
            }
            else {
                /* CORE1 to CORE0 */
                REG16(INTERRUPT_CORE0) |= 0x1;
            }
        }
        else {
            index = MBX_TABLE_IDX(srcProcId, MultiProc_self());
            key = Hwi_disable();
            if (REG32(MAILBOX_STATUS(index)) == 0) {
                REG32(MAILBOX_MESSAGE(index)) = arg;
            }
            Hwi_restore(key);
        }
    }
}

/*
 *  ======== InterruptIpu_intClear ========
 *  Clear interrupt
 */
UInt InterruptIpu_intClear(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt arg;
    UInt16 index;

    if (Core_ipuId == 1) {
        if ((remoteProcId == InterruptIpu_ipu1_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu1_1ProcId)) {
            arg = REG32(InterruptIpu_ducatiCtrlBaseAddr);

            /* Look at BIOS's ducati Core id */
            if ((BIOS_smpEnabled) || (Core_getId() == 0)) {
                if ((REG16(INTERRUPT_CORE0) & 0x1) == 0x1) {
                    /* CORE1 to CORE0 */
                    REG16(INTERRUPT_CORE0) &= ~(0x1);
                }
            }
            else {
                if ((REG16(INTERRUPT_CORE1) & 0x1) == 0x1) {
                    /* CORE0 to CORE1 */
                    REG16(INTERRUPT_CORE1) &= ~(0x1);
                }
            }
        }
        else {
            index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());
            arg = REG32(MAILBOX_MESSAGE(index));
            REG32(MAILBOX_IRQSTATUS_CLR(index)) =
                MAILBOX_REG_VAL(SUBMBX_IDX(index));
        }
    }
    else {
        if ((remoteProcId == InterruptIpu_ipu2_0ProcId) ||
            (remoteProcId == InterruptIpu_ipu2_1ProcId)) {
            arg = REG32(InterruptIpu_ducatiCtrlBaseAddr);

            /* Look at BIOS's ducati Core id */
            if ((BIOS_smpEnabled) || (Core_getId() == 0)) {
                if ((REG16(INTERRUPT_CORE0) & 0x1) == 0x1) {
                    /* CORE1 to CORE0 */
                    REG16(INTERRUPT_CORE0) &= ~(0x1);
                }
            }
            else {
                if ((REG16(INTERRUPT_CORE1) & 0x1) == 0x1) {
                    /* CORE0 to CORE1 */
                    REG16(INTERRUPT_CORE1) &= ~(0x1);
                }
            }
        }
        else {
            index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());
            arg = REG32(MAILBOX_MESSAGE(index));
            REG32(MAILBOX_IRQSTATUS_CLR(index)) =
                MAILBOX_REG_VAL(SUBMBX_IDX(index));
        }
    }

    return (arg);
}

/*
 *************************************************************************
 *                      Internals functions
 *************************************************************************
 */

 /*
 *  ======== InterruptIpu_intShmDucatiStub ========
 */
Void InterruptIpu_intShmDucatiStub(UArg arg)
{
    UInt16 index;
    InterruptIpu_FxnTable *table;

    if (Core_ipuId == 1) {
        if ((BIOS_smpEnabled) || (Core_getId() == 0)) {
            index = 9;
        }
        else {
            index = 6;
        }
    }
    else {
        if ((BIOS_smpEnabled) || (Core_getId() == 0)) {
            index = 10;
        }
        else {
            index = 7;
        }
    }

    table = &(InterruptIpu_module->fxnTable[index]);
    (table->func)(table->arg);
}


/*
 *  ======== InterruptIpu_intShmMbxStub ========
 */
Void InterruptIpu_intShmMbxStub(UArg arg)
{
    UInt16 index;
    UInt16 selfIdx;
    UInt16 loopIdx;
    InterruptIpu_FxnTable *table;

    selfIdx = MultiProc_self();

    for (loopIdx = 0; loopIdx < MultiProc_getNumProcsInCluster(); loopIdx++) {

        if (loopIdx == selfIdx) {
            continue;
        }

        index = MBX_TABLE_IDX(loopIdx, selfIdx);

        /* skip mailbox if it's not being used */
        if (InterruptIpu_mailboxTable[index] == (UInt32)(-1)) {
            continue;
        }

        if (((REG32(MAILBOX_STATUS(index)) != 0) &&
             (REG32(MAILBOX_IRQENABLE_SET(index)) &
              MAILBOX_REG_VAL(SUBMBX_IDX(index))))) {
            table = &(InterruptIpu_module->fxnTable[PROCID(loopIdx)]);
            (table->func)(table->arg);
        }
    }
}
