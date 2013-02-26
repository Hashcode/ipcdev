/*
 * Copyright (c) 2012-2013, Texas Instruments Incorporated
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
 *  ======== InterruptArp32.c ========
 *  ARP32 mailbox based interrupt manager
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>

#include <ti/sysbios/family/arp32/Hwi.h>

#include <ti/sdo/utils/_MultiProc.h>
#include <ti/sdo/ipc/_Ipc.h>
#include <ti/sdo/ipc/notifyDrivers/IInterrupt.h>

#include "package/internal/InterruptArp32.xdc.h"

/* Register access method. */
#define REG16(A)   (*(volatile UInt16 *) (A))
#define REG32(A)   (*(volatile UInt32 *) (A))

#define PROCID(IDX)               (InterruptArp32_procIdTable[IDX])
#define MBX_TABLE_IDX(SRC, DST)   ((PROCID(SRC) * InterruptArp32_NUM_CORES) + \
                                    PROCID(DST))
#define SUBMBX_IDX(IDX)           (InterruptArp32_mailboxTable[IDX] & 0xFF)
#define MBX_USER_IDX(IDX)         ((InterruptArp32_mailboxTable[IDX] >> 8) \
                                    & 0xFF)
#define MBX_BASEADDR_IDX(IDX)     ((InterruptArp32_mailboxTable[IDX] >> 16) \
                                    & 0xFFFF)

#define MAILBOX_REG_VAL(M)   (0x1 << (2 * M))

#define MAILBOX_MESSAGE(IDX) (InterruptArp32_mailboxBaseAddr[  \
                                MBX_BASEADDR_IDX(IDX)] + 0x040 \
                                + (0x4 * SUBMBX_IDX(IDX)))
#define MAILBOX_STATUS(IDX)  (InterruptArp32_mailboxBaseAddr[  \
                                MBX_BASEADDR_IDX(IDX)] + 0x0C0 \
                                + (0x4 * SUBMBX_IDX(IDX)))

#define MAILBOX_IRQSTATUS_CLR(IDX)  (InterruptArp32_mailboxBaseAddr[   \
                                        MBX_BASEADDR_IDX(IDX)] + 0x104 \
                                        + (MBX_USER_IDX(IDX) * 0x10))
#define MAILBOX_IRQENABLE_SET(IDX)  (InterruptArp32_mailboxBaseAddr[   \
                                        MBX_BASEADDR_IDX(IDX)] + 0x108 \
                                        + (MBX_USER_IDX(IDX) * 0x10))
#define MAILBOX_IRQENABLE_CLR(IDX)  (InterruptArp32_mailboxBaseAddr[   \
                                        MBX_BASEADDR_IDX(IDX)] + 0x10C \
                                        + (MBX_USER_IDX(IDX) * 0x10))

#define MAILBOX_EOI_REG(IDX)        (InterruptArp32_mailboxBaseAddr[   \
                                        MBX_BASEADDR_IDX(IDX)] + 0x140)

/* This needs to match the virtual id from the TableInit.xs */
#define DSP1_ID     4
#define DSP2_ID     5
#define IPU1_ID     6
#define IPU2_ID     7
#define HOST_ID     8

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*!
 *  ======== InterruptArp32_intEnable ========
 *  Enable remote processor interrupt
 */
Void InterruptArp32_intEnable(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    REG32(MAILBOX_IRQENABLE_SET(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));
}

/*!
 *  ======== InterruptArp32_intDisable ========
 *  Disables remote processor interrupt
 */
Void InterruptArp32_intDisable(UInt16 remoteProcId,
                                IInterrupt_IntInfo *intInfo)
{
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    REG32(MAILBOX_IRQENABLE_CLR(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));
}

/*!
 *  ======== InterruptArp32_intRegister ========
 */
Void InterruptArp32_intRegister(UInt16 remoteProcId,
                                 IInterrupt_IntInfo *intInfo,
                                 Fxn func, UArg arg)
{
    UInt        key;
    UInt16      index;
    UInt        mbxIdx;
    Hwi_Params  hwiAttrs;
    Error_Block eb;
    InterruptArp32_FxnTable *table;

    Assert_isTrue(remoteProcId < ti_sdo_utils_MultiProc_numProcessors,
           ti_sdo_ipc_Ipc_A_internal);

    /* Assert that our MultiProc id is set correctly */
    Assert_isTrue((InterruptArp32_eve1ProcId == MultiProc_self()) ||
                  (InterruptArp32_eve2ProcId == MultiProc_self()) ||
                  (InterruptArp32_eve3ProcId == MultiProc_self()) ||
                  (InterruptArp32_eve4ProcId == MultiProc_self()),
                   ti_sdo_ipc_Ipc_A_internal);

    /* init error block */
    Error_init(&eb);

    index = PROCID(remoteProcId);

    /* Disable global interrupts */
    key = Hwi_disable();

    table = &(InterruptArp32_module->fxnTable[index]);
    table->func = func;
    table->arg  = arg;

    InterruptArp32_intClear(remoteProcId, intInfo);

    if ((index == DSP1_ID) || (index == IPU1_ID) || (index == HOST_ID)) {
        mbxIdx = 0;
    }
    else if ((index == DSP2_ID) || (index == IPU2_ID)) {
        mbxIdx = 1;
    }
    else if (index < InterruptArp32_NUM_EVES) {
        mbxIdx = 2;
    }

    /* Make sure the interrupt only gets plugged once */
    InterruptArp32_module->numPlugged[mbxIdx]++;
    if (InterruptArp32_module->numPlugged[mbxIdx] == 1) {
        /* Register interrupt to remote processor */
        Hwi_Params_init(&hwiAttrs);
        hwiAttrs.arg = arg;
        hwiAttrs.vectorNum = intInfo->intVectorId;

        Hwi_create(InterruptArp32_eveInterruptTable[index],
                  (Hwi_FuncPtr)InterruptArp32_intShmStub,
                   &hwiAttrs,
                   &eb);
        Hwi_enableInterrupt(InterruptArp32_eveInterruptTable[index]);
    }

    /* enable the mailbox and Hwi */
    InterruptArp32_intEnable(remoteProcId, intInfo);

    /* Restore global interrupts */
    Hwi_restore(key);
}

/*!
 *  ======== InterruptArp32_intUnregister ========
 */
Void InterruptArp32_intUnregister(UInt16 remoteProcId,
                                   IInterrupt_IntInfo *intInfo)
{
    UInt16 index;
    UInt   mbxIdx;
    Hwi_Handle  hwiHandle;
    InterruptArp32_FxnTable *table;

    index = PROCID(remoteProcId);

    if ((remoteProcId == DSP1_ID) || (remoteProcId == IPU1_ID) || (remoteProcId == HOST_ID)) {
        mbxIdx = 0;
    }
    else if ((remoteProcId == DSP2_ID) || (remoteProcId == IPU2_ID)) {
        mbxIdx = 1;
    }
    else if (remoteProcId < InterruptArp32_NUM_EVES) {
        mbxIdx = 2;
    }

    /* Disable the mailbox interrupt source */
    InterruptArp32_intDisable(remoteProcId, intInfo);

    InterruptArp32_module->numPlugged[mbxIdx]--;
    if (InterruptArp32_module->numPlugged[mbxIdx] == 0) {
        /* Delete the Hwi */
        hwiHandle = Hwi_getHandle(InterruptArp32_eveInterruptTable[index]);
        Hwi_delete(&hwiHandle);
    }

    /* Clear the FxnTable entry for the remote processor */
    table = &(InterruptArp32_module->fxnTable[index]);
    table->func = NULL;
    table->arg  = 0;
}


/*!
 *  ======== InterruptArp32_intSend ========
 *  Send interrupt to the remote processor
 */
Void InterruptArp32_intSend(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo,
                             UArg arg)
{
    UInt key;
    UInt16 index;

    index = MBX_TABLE_IDX(MultiProc_self(), remoteProcId);

    /* disable interrupts */
    key = Hwi_disable();

    if (REG32(MAILBOX_STATUS(index)) == 0) {
        /* write the mailbox message to remote proc */
        REG32(MAILBOX_MESSAGE(index)) = arg;
    }

    /* restore interrupts */
    Hwi_restore(key);
}


/*!
 *  ======== InterruptArp32_intPost ========
 *  Simulate an interrupt from a remote processor
 */
Void InterruptArp32_intPost(UInt16 srcProcId, IInterrupt_IntInfo *intInfo,
                             UArg arg)
{
    UInt key;
    UInt16 index;

    index = MBX_TABLE_IDX(srcProcId, MultiProc_self());

    /* disable interrupts */
    key = Hwi_disable();

    if (REG32(MAILBOX_STATUS(index)) == 0) {
        /* write the mailbox message to arp32 */
        REG32(MAILBOX_MESSAGE(index)) = arg;
    }

    /* restore interrupts */
    Hwi_restore(key);
}


/*!
 *  ======== InterruptArp32_intClear ========
 *  Clear interrupt
 */
UInt InterruptArp32_intClear(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt arg;
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    arg = REG32(MAILBOX_MESSAGE(index));
    /* clear the dsp mailbox */
    REG32(MAILBOX_IRQSTATUS_CLR(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));

    /* Write to EOI (End Of Interrupt) register */
    REG32(MAILBOX_EOI_REG(index)) = 0x1;

    return (arg);
}

/*
 *************************************************************************
 *                      Internals functions
 *************************************************************************
 */

/*!
 *  ======== InterruptArp32_intShmStub ========
 */
Void InterruptArp32_intShmStub(UArg arg)
{
    UInt16 index;
    UInt16 selfIdx;
    UInt16 loopIdx;
    InterruptArp32_FxnTable *table;

    selfIdx = MultiProc_self();

    for (loopIdx = 0; loopIdx < MultiProc_getNumProcsInCluster(); loopIdx++) {

        if (loopIdx == selfIdx) {
            continue;
        }

        index = MBX_TABLE_IDX(loopIdx, selfIdx);

        if ((REG32(MAILBOX_STATUS(index)) != 0) &&
            (REG32(MAILBOX_IRQENABLE_SET(index)) &
             MAILBOX_REG_VAL(SUBMBX_IDX(index)))) {
            /* call function with arg */
            table = &(InterruptArp32_module->fxnTable[PROCID(loopIdx)]);
            (table->func)(table->arg);
        }
    }
}
