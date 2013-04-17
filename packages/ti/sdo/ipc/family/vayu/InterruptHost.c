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
 *  ======== InterruptHost.c ========
 *  Ducati/A8 based interupt manager
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>

#include <ti/sysbios/hal/vayu/IntXbar.h>
#include <ti/sysbios/family/arm/gic/Hwi.h>
#include <ti/sdo/ipc/notifyDrivers/IInterrupt.h>
#include <ti/sdo/ipc/_Ipc.h>
#include <ti/sdo/utils/_MultiProc.h>

#include "package/internal/InterruptHost.xdc.h"

/* Register access method. */
#define REG16(A)   (*(volatile UInt16 *) (A))
#define REG32(A)   (*(volatile UInt32 *) (A))

#define PROCID(IDX)               (InterruptHost_procIdTable[(IDX)])
#define MBX_TABLE_IDX(SRC, DST)   ((PROCID(SRC) * InterruptHost_NUM_CORES) + \
                                    PROCID(DST))
#define SUBMBX_IDX(IDX)           (InterruptHost_mailboxTable[(IDX)] & 0xFF)
#define MBX_USER_IDX(IDX)         ((InterruptHost_mailboxTable[(IDX)] >> 8) \
                                    & 0xFF)
#define MBX_BASEADDR_IDX(IDX)     ((InterruptHost_mailboxTable[(IDX)] >> 16) \
                                    & 0xFFFF)

#define MAILBOX_REG_VAL(M)        (0x1 << (2 * M))

#define MAILBOX_MESSAGE(IDX)      (InterruptHost_mailboxBaseAddr[  \
                                    MBX_BASEADDR_IDX(IDX)] + 0x40 +   \
                                    (0x4 * SUBMBX_IDX(IDX)))
#define MAILBOX_STATUS(IDX)       (InterruptHost_mailboxBaseAddr[  \
                                    MBX_BASEADDR_IDX(IDX)] + 0xC0 +   \
                                    (0x4 * SUBMBX_IDX(IDX)))

#define MAILBOX_IRQSTATUS_CLR(IDX)   (InterruptHost_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + (0x10 * \
                                        MBX_USER_IDX(IDX)) + 0x104)
#define MAILBOX_IRQENABLE_SET(IDX)   (InterruptHost_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + (0x10 * \
                                        MBX_USER_IDX(IDX)) + 0x108)
#define MAILBOX_IRQENABLE_CLR(IDX)   (InterruptHost_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + (0x10 * \
                                        MBX_USER_IDX(IDX)) + 0x10C)
#define MAILBOX_EOI_REG(IDX)         (InterruptHost_mailboxBaseAddr[  \
                                        MBX_BASEADDR_IDX(IDX)] + 0x140)

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*
 *  ======== InterruptHost_Module_startup ========
 */
Int InterruptHost_Module_startup(Int phase)
{
    if (IntXbar_Module_startupDone()) {
        /* connect mailbox interrupts at startup */
        IntXbar_connect(127, 286);  // eve1 mailbox 0 user 3
        IntXbar_connect(128, 295);  // eve2 mailbox 0 user 3
        IntXbar_connect(129, 251);  // system mailbox 5 user 2

        /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
        if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
            (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
            IntXbar_connect(130, 304);  // eve3 mailbox 0 user 3
            IntXbar_connect(131, 313);  // eve4 mailbox 0 user 3
        }

        /* plug mbx6 only if DSP2 or IPU2 exists */
        if ((MultiProc_getId("DSP2") != MultiProc_INVALIDID) ||
            (MultiProc_getId("IPU2") != MultiProc_INVALIDID) ||
            (MultiProc_getId("IPU2-0") != MultiProc_INVALIDID)) {
            IntXbar_connect(132, 255);  // system mailbox 6 user 2
        }

        return (Startup_DONE);
    }

    return (Startup_NOTDONE);
}

/*
 *  ======== InterruptHost_intEnable ========
 *  Enable remote processor interrupt
 */
Void InterruptHost_intEnable(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());
    /*
     *  If the remote processor communicates via mailboxes, we should enable
     *  the Mailbox IRQ instead of enabling the Hwi because multiple mailboxes
     *  share the same Hwi
     */
    REG32(MAILBOX_IRQENABLE_SET(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));
}

/*
 *  ======== InterruptHost_intDisable ========
 *  Disables remote processor interrupt
 */
Void InterruptHost_intDisable(UInt16 remoteProcId,
                                IInterrupt_IntInfo *intInfo)
{
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());
    /*
     *  If the remote processor communicates via mailboxes, we should disable
     *  the Mailbox IRQ instead of disabling the Hwi because multiple mailboxes
     *  share the same Hwi
     */
    REG32(MAILBOX_IRQENABLE_CLR(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));
}

/*
 *  ======== InterruptHost_intRegister ========
 */
Void InterruptHost_intRegister(UInt16 remoteProcId,
                                 IInterrupt_IntInfo *intInfo,
                                 Fxn func, UArg arg)
{
    Hwi_Params  hwiAttrs;
    UInt        key;
    UInt        mbxIdx;
    Int         index;
    InterruptHost_FxnTable *table;

    Assert_isTrue(remoteProcId < ti_sdo_utils_MultiProc_numProcessors,
            ti_sdo_ipc_Ipc_A_internal);

    /* Assert that our MultiProc id is set correctly */
    Assert_isTrue((InterruptHost_hostProcId == MultiProc_self()),
                  ti_sdo_ipc_Ipc_A_internal);

    mbxIdx = MBX_BASEADDR_IDX(MBX_TABLE_IDX(remoteProcId, MultiProc_self()));

    index = PROCID(remoteProcId);

    intInfo->localIntId = InterruptHost_hostInterruptTable[index];

    /* Disable global interrupts */
    key = Hwi_disable();

    table = &(InterruptHost_module->fxnTable[index]);
    table->func = func;
    table->arg  = arg;

    InterruptHost_intClear(remoteProcId, intInfo);

    Hwi_Params_init(&hwiAttrs);
    hwiAttrs.maskSetting = Hwi_MaskingOption_LOWER;

    /* Make sure the interrupt only gets plugged once */
    InterruptHost_module->numPlugged[mbxIdx]++;
    if (InterruptHost_module->numPlugged[mbxIdx] == 1) {
        Hwi_create(intInfo->localIntId,
                   (Hwi_FuncPtr)InterruptHost_intShmStub,
                    &hwiAttrs,
                    NULL);

        /* Interrupt_intEnable won't enable the Hwi */
        Hwi_enableInterrupt(intInfo->localIntId);
    }

    InterruptHost_intEnable(remoteProcId, intInfo);

    /* Restore global interrupts */
    Hwi_restore(key);
}

/*
 *  ======== InterruptHost_intUnregister ========
 */
Void InterruptHost_intUnregister(UInt16 remoteProcId,
                                   IInterrupt_IntInfo *intInfo)
{
    UInt                       mbxIdx;
    Int                        index;
    InterruptHost_FxnTable *table;
    Hwi_Handle                 hwiHandle;

    mbxIdx = MBX_BASEADDR_IDX(MBX_TABLE_IDX(remoteProcId, MultiProc_self()));

    index = PROCID(remoteProcId);

    /* Disable the mailbox interrupt source */
    InterruptHost_intDisable(remoteProcId, intInfo);

    /* Disable the interrupt itself */
    InterruptHost_module->numPlugged[mbxIdx]--;
    if (InterruptHost_module->numPlugged[mbxIdx] == 0) {
        hwiHandle = Hwi_getHandle(intInfo->localIntId);
        Hwi_delete(&hwiHandle);
    }

    /* Clear the FxnTable entry for the remote processor */
    table = &(InterruptHost_module->fxnTable[index]);
    table->func = NULL;
    table->arg  = 0;
}


/*
 *  ======== InterruptHost_intSend ========
 *  Send interrupt to the remote processor
 */
Void InterruptHost_intSend(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo,
                             UArg arg)
{
    UInt key;
    UInt16 index;

    index = MBX_TABLE_IDX(MultiProc_self(), remoteProcId);
    key = Hwi_disable();
    if (REG32(MAILBOX_STATUS(index)) == 0) {
        REG32(MAILBOX_MESSAGE(index)) = arg;
    }
    Hwi_restore(key);
}


/*
 *  ======== InterruptHost_intPost ========
 *  Simulate an interrupt from a remote processor
 */
Void InterruptHost_intPost(UInt16 srcProcId, IInterrupt_IntInfo *intInfo,
                             UArg arg)
{
    UInt key;
    UInt16 index;

    index = MBX_TABLE_IDX(srcProcId, MultiProc_self());
    key = Hwi_disable();
    if (REG32(MAILBOX_STATUS(index)) == 0) {
        REG32(MAILBOX_MESSAGE(index)) = arg;
    }
    Hwi_restore(key);
}


/*
 *  ======== InterruptHost_intClear ========
 *  Clear interrupt
 */
UInt InterruptHost_intClear(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt arg;
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());
    arg = REG32(MAILBOX_MESSAGE(index));
    REG32(MAILBOX_IRQSTATUS_CLR(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));

    return (arg);
}

/*
 *************************************************************************
 *                      Internals functions
 *************************************************************************
 */

/*
 *  ======== InterruptHost_intShmMbxStub ========
 */
Void InterruptHost_intShmStub(UArg arg)
{
    UInt16 index;
    UInt16 selfIdx;
    UInt16 loopIdx;
    InterruptHost_FxnTable *table;

    selfIdx = MultiProc_self();

    for (loopIdx = 0; loopIdx < MultiProc_getNumProcsInCluster(); loopIdx++) {

        if (loopIdx == selfIdx) {
            continue;
        }

        index = MBX_TABLE_IDX(loopIdx, selfIdx);

        if (((REG32(MAILBOX_STATUS(index)) != 0) &&
             (REG32(MAILBOX_IRQENABLE_SET(index)) &
              MAILBOX_REG_VAL(SUBMBX_IDX(index))))) {
            table = &(InterruptHost_module->fxnTable[PROCID(loopIdx)]);
            (table->func)(table->arg);
        }
    }
}
