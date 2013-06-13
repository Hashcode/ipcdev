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
 *  ======== InterruptDsp.c ========
 *  Mailbox based interrupt manager
 */
#include <xdc/std.h>
#include <xdc/runtime/Startup.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Assert.h>

#include <ti/sysbios/family/shared/vayu/IntXbar.h>

#include <ti/sysbios/family/c64p/Hwi.h>
#include <ti/sysbios/family/c64p/EventCombiner.h>

#include <ti/sdo/utils/_MultiProc.h>
#include <ti/sdo/ipc/_Ipc.h>

#include <ti/sdo/ipc/notifyDrivers/IInterrupt.h>

#include "package/internal/InterruptDsp.xdc.h"

/* Register access method. */
#define REG16(A)   (*(volatile UInt16 *) (A))
#define REG32(A)   (*(volatile UInt32 *) (A))

#define PROCID(IDX)               (InterruptDsp_procIdTable[IDX])
#define MBX_TABLE_IDX(SRC, DST)   ((PROCID(SRC) * InterruptDsp_NUM_CORES) + \
                                    PROCID(DST))
#define SUBMBX_IDX(IDX)           (InterruptDsp_mailboxTable[IDX] & 0xFF)
#define MBX_USER_IDX(IDX)         ((InterruptDsp_mailboxTable[IDX] >> 8) & 0xFF)
#define MBX_BASEADDR_IDX(IDX)     ((InterruptDsp_mailboxTable[IDX] >> 16) & 0xFFFF)

#define MAILBOX_REG_VAL(M)        (0x1 << (2 * M))

#define MAILBOX_MESSAGE(IDX)      (InterruptDsp_mailboxBaseAddr[  \
                                    MBX_BASEADDR_IDX(IDX)] + 0x40 \
                                    + (0x4 * SUBMBX_IDX(IDX)))
#define MAILBOX_STATUS(IDX)       (InterruptDsp_mailboxBaseAddr[  \
                                    MBX_BASEADDR_IDX(IDX)] + 0xC0 \
                                    + (0x4 * SUBMBX_IDX(IDX)))

#define MAILBOX_IRQSTATUS_CLR_DSP(IDX)   (InterruptDsp_mailboxBaseAddr[ \
                                           MBX_BASEADDR_IDX(IDX)] +     \
                                           (0x10 * MBX_USER_IDX(IDX)) + 0x104)
#define MAILBOX_IRQENABLE_SET_DSP(IDX)   (InterruptDsp_mailboxBaseAddr[ \
                                           MBX_BASEADDR_IDX(IDX)] +     \
                                           (0x10 * MBX_USER_IDX(IDX)) + 0x108)
#define MAILBOX_IRQENABLE_CLR_DSP(IDX)   (InterruptDsp_mailboxBaseAddr[ \
                                           MBX_BASEADDR_IDX(IDX)] +     \
                                           (0x10 * MBX_USER_IDX(IDX)) + 0x10C)
#define MAILBOX_EOI_REG(IDX)             (InterruptDsp_mailboxBaseAddr[ \
                                           MBX_BASEADDR_IDX(IDX)] + 0x140)
#define EVENT_GROUP_SIZE                 32

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*
 *  ======== InterruptDsp_Module_startup ========
 */
Int InterruptDsp_Module_startup(Int phase)
{
    extern cregister volatile UInt DNUM;

    if (IntXbar_Module_startupDone()) {
        /* connect mailbox interrupts at startup */
        if (DNUM == 0) {               /* DSP1 */
            IntXbar_connect(24, 284);  // eve1 mailbox 0 user 1
            IntXbar_connect(25, 293);  // eve2 mailbox 0 user 1
            IntXbar_connect(26, 249);  // system mailbox 5 user 0

            InterruptDsp_module->interruptTable[6] = 57;    // IPU1-0
            InterruptDsp_module->interruptTable[9] = 57;    // IPU1-1

            /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
            if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
                (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
                IntXbar_connect(27, 302);  // eve3 mailbox 0 user 1
                IntXbar_connect(28, 311);  // eve4 mailbox 0 user 1
            }

            /* plug mbx7 only if DSP2 or IPU2 exists */
            if ((MultiProc_getId("DSP2") != MultiProc_INVALIDID) ||
                (MultiProc_getId("IPU2") != MultiProc_INVALIDID) ||
                (MultiProc_getId("IPU2-0") != MultiProc_INVALIDID)) {
                IntXbar_connect(29, 257);  // system mailbox 7 user 0
                InterruptDsp_module->interruptTable[7] = 60;    // IPU2-0
            }

            /* plug mbx8 only if IPU2-1 exists */
            if (MultiProc_getId("IPU2-1") != MultiProc_INVALIDID) {
                IntXbar_connect(30, 261);  // system mailbox 8 user 0
                InterruptDsp_module->interruptTable[10] = 61;    // IPU2-1
            }
        }
        else if (DNUM == 1) {          /* DSP2 */
            IntXbar_connect(24, 287);  // eve1 mailbox 1 user 1
            IntXbar_connect(25, 296);  // eve2 mailbox 1 user 1
            IntXbar_connect(26, 253);  // system mailbox 6 user 0

            InterruptDsp_module->interruptTable[7] = 57;    // IPU2-0
            InterruptDsp_module->interruptTable[10] = 57;   // IPU2-1

            /* plug eve3 and eve4 mbxs only if eve3 and eve4 exists */
            if ((MultiProc_getId("EVE3") != MultiProc_INVALIDID) ||
                (MultiProc_getId("EVE4") != MultiProc_INVALIDID)) {
                IntXbar_connect(27, 305);  // eve3 mailbox 1 user 1
                IntXbar_connect(28, 314);  // eve4 mailbox 1 user 1
            }

            /* plug mbx7 only if DSP1 or IPU1 exists */
            if ((MultiProc_getId("DSP1") != MultiProc_INVALIDID) ||
                (MultiProc_getId("IPU1") != MultiProc_INVALIDID) ||
                (MultiProc_getId("IPU1-0") != MultiProc_INVALIDID)) {
                IntXbar_connect(29, 258);  // system mailbox 7 user 1
                InterruptDsp_module->interruptTable[6] = 60;    // IPU1-0
            }

            /* plug mbx8 only if IPU1-1 exists */
            if (MultiProc_getId("IPU1-1") != MultiProc_INVALIDID) {
                IntXbar_connect(30, 262);  // system mailbox 8 user 1
                InterruptDsp_module->interruptTable[9] = 61;    // IPU1-1
            }
        }
        return (Startup_DONE);
    }

    return (Startup_NOTDONE);
}

/*
 *  ======== InterruptDsp_intEnable ========
 *  Enable remote processor interrupt
 */
Void InterruptDsp_intEnable(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt16 index;

    Assert_isTrue(((remoteProcId < MultiProc_getNumProcsInCluster()) &&
            (remoteProcId != MultiProc_self())), ti_sdo_ipc_Ipc_A_internal);

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    REG32(MAILBOX_IRQENABLE_SET_DSP(index))=MAILBOX_REG_VAL(SUBMBX_IDX(index));
}

/*
 *  ======== InterruptDsp_intDisable ========
 *  Disables remote processor interrupt
 */
Void InterruptDsp_intDisable(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt16 index;

    Assert_isTrue(((remoteProcId < MultiProc_getNumProcsInCluster()) &&
            (remoteProcId != MultiProc_self())), ti_sdo_ipc_Ipc_A_internal);

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    REG32(MAILBOX_IRQENABLE_CLR_DSP(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));
}

/*
 *  ======== InterruptDsp_intRegister ========
 */
Void InterruptDsp_intRegister(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo,
                              Fxn func, UArg arg)
{
    UInt        key;
    UInt        eventId;
    UInt        combinedEventId;
    Int         index;
    Hwi_Params  params;
    InterruptDsp_FxnTable *table;

    Assert_isTrue(((remoteProcId < MultiProc_getNumProcsInCluster()) &&
                   (remoteProcId != MultiProc_self())), ti_sdo_ipc_Ipc_A_internal);

    index = PROCID(remoteProcId);

    /* Disable global interrupts */
    key = Hwi_disable();

    table = &(InterruptDsp_module->fxnTable[index]);
    table->func = func;
    table->arg  = arg;

    InterruptDsp_intClear(remoteProcId, intInfo);

    /* Make sure the interrupt only gets plugged once */
    eventId = InterruptDsp_module->interruptTable[index];

    InterruptDsp_module->numPlugged++;

    if (InterruptDsp_module->numPlugged == 1) {
        EventCombiner_dispatchPlug(eventId,
            (Hwi_FuncPtr)InterruptDsp_intShmStub, eventId, TRUE);

        Hwi_Params_init(&params);
        combinedEventId = eventId / EVENT_GROUP_SIZE;
        params.eventId = combinedEventId;
        params.arg = combinedEventId;
        params.enableInt = TRUE;
        Hwi_create(intInfo->intVectorId, &ti_sysbios_family_c64p_EventCombiner_dispatch,
                   &params, NULL);
        Hwi_enableInterrupt(intInfo->intVectorId);
    }
    else {
        EventCombiner_dispatchPlug(eventId,
                (Hwi_FuncPtr)InterruptDsp_intShmStub, eventId, TRUE);
    }

    /* Enable the mailbox interrupt to the DSP */
    InterruptDsp_intEnable(remoteProcId, intInfo);

    /* Restore global interrupts */
    Hwi_restore(key);
}

/*
 *  ======== InterruptDsp_intUnregister ========
 */
Void InterruptDsp_intUnregister(UInt16 remoteProcId,
                                IInterrupt_IntInfo *intInfo)
{
    Hwi_Handle  hwiHandle;
    Int         index;
    UInt        eventId;
    InterruptDsp_FxnTable *table;

    Assert_isTrue(((remoteProcId < MultiProc_getNumProcsInCluster()) &&
                   (remoteProcId == MultiProc_self())), ti_sdo_ipc_Ipc_A_internal);

    index = PROCID(remoteProcId);

    /* Disable the mailbox interrupt source */
    InterruptDsp_intDisable(remoteProcId, intInfo);

    /* Make sure the interrupt only gets plugged once */
    eventId = InterruptDsp_module->interruptTable[index];

    InterruptDsp_module->numPlugged--;

    EventCombiner_disableEvent(eventId);

    if (InterruptDsp_module->numPlugged == 0) {
        /* Delete the Hwi */
        hwiHandle = Hwi_getHandle(intInfo->intVectorId);
        Hwi_delete(&hwiHandle);
    }

    table = &(InterruptDsp_module->fxnTable[index]);
    table->func = NULL;
    table->arg  = NULL;
}

/*
 *  ======== InterruptDsp_intSend ========
 *  Send interrupt to the remote processor
 */
Void InterruptDsp_intSend(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo,
                          UArg arg)
{
    UInt key;
    UInt16 index;

    index = MBX_TABLE_IDX(MultiProc_self(), remoteProcId);

    /*
     *  Before writing to a mailbox, check whehter it already contains a message
     *  If so, then don't write to the mailbox since we want one and only one
     *  message per interrupt.  Disable interrupts between reading
     *  the MSGSTATUS_X register and writing to the mailbox to protect from
     *  another thread doing an intSend at the same time
     *
     *  Note regarding possible race condition between local 'intSend' and
     *  remote 'intClear':
     *  It is possible that we we read the MAILBOX_MSGSTATUS_X register during
     *  the remote side's intClear.  Therefore, we might choose _not_ to send
     *  write to the mailbox even though the mailbox is about to be cleared a
     *  few cycles later. In this case, the interrupt will be lost.
     *  This is OK, however. intClear should always be called by the Notify
     *  driver _before_ shared memory is read, so the event will be picked up
     *  anyway by the previous interrupt that caused intClear to be called.
     */
    key = Hwi_disable();
    if (REG32(MAILBOX_STATUS(index)) == 0) {
        REG32(MAILBOX_MESSAGE(index)) = arg;
    }
    Hwi_restore(key);
}

/*
 *  ======== InterruptDsp_intPost ========
 */
Void InterruptDsp_intPost(UInt16 srcProcId, IInterrupt_IntInfo *intInfo,
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
 *  ======== InterruptDsp_intClear ========
 *  Clear interrupt
 */
UInt InterruptDsp_intClear(UInt16 remoteProcId, IInterrupt_IntInfo *intInfo)
{
    UInt arg;
    UInt16 index;

    index = MBX_TABLE_IDX(remoteProcId, MultiProc_self());

    arg = REG32(MAILBOX_MESSAGE(index));
    REG32(MAILBOX_IRQSTATUS_CLR_DSP(index)) = MAILBOX_REG_VAL(SUBMBX_IDX(index));

    /* Write to EOI (End Of Interrupt) register */
    REG32(MAILBOX_EOI_REG(index)) = 0x1;

    return (arg);
}

/*
 *************************************************************************
 *                      Internals functions
 *************************************************************************
 */

/*
 *  ======== InterruptDsp_intShmStub ========
 */
Void InterruptDsp_intShmStub(UArg arg)
{
    UInt16 index;
    UInt16 selfIdx;
    UInt16 loopIdx;
    InterruptDsp_FxnTable *table;

    selfIdx = MultiProc_self();

    /*
     * Loop through each Sub-mailbox to determine which one generated
     * interrupt.
     */
    for (loopIdx = 0; loopIdx < MultiProc_getNumProcsInCluster(); loopIdx++) {

        if (loopIdx == selfIdx) {
            continue;
        }

        index = MBX_TABLE_IDX(loopIdx, selfIdx);

        if ((REG32(MAILBOX_STATUS(index)) != 0) &&
            (REG32(MAILBOX_IRQENABLE_SET_DSP(index)) &
             MAILBOX_REG_VAL(SUBMBX_IDX(index)))) {
            table = &(InterruptDsp_module->fxnTable[PROCID(loopIdx)]);
            (table->func)(table->arg);
        }
    }
}
