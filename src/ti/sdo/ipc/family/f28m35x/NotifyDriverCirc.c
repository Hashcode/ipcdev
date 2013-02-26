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
 *  ======== NotifyDriverCirc.c ========
 */

#include <xdc/std.h>

#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Timestamp.h>

#ifdef xdc_target__isaCompatible_v7M
#include <ti/sysbios/family/arm/m3/Hwi.h>
#else
#include <ti/sysbios/family/c28/Hwi.h>
#endif

#include <ti/sdo/ipc/family/f28m35x/IpcMgr.h>
#include <ti/sdo/ipc/interfaces/INotifyDriver.h>

#include "package/internal/NotifyDriverCirc.xdc.h"

#include <ti/sdo/ipc/_Notify.h>
#include <ti/sdo/utils/_MultiProc.h>

/* Bit mask operations */
#define SET_BIT(num,pos)            ((num) |= (1u << (pos)))
#define CLEAR_BIT(num,pos)          ((num) &= ~(1u << (pos)))
#define TEST_BIT(num,pos)           ((num) & (1u << (pos)))

/* For the M3 */
#define CTOMIPCACK  (0x400FB700)
#define CTOMIPCSTS  (CTOMIPCACK + 0x4)
#define MTOCIPCSET  (CTOMIPCACK + 0x8)
#define MTOCIPCCLR  (CTOMIPCACK + 0xC)
#define MTOCIPCFLG  (CTOMIPCACK + 0x10)

/* For the C28 */
#define CTOMIPCSET  (0x00004E00)
#define CTOMIPCCLR  (CTOMIPCSET + 0x2)
#define CTOMIPCFLG  (CTOMIPCSET + 0x4)
#define MTOCIPCACK  (CTOMIPCSET + 0x6)
#define MTOCIPCSTS  (CTOMIPCSET + 0x8)

/*
 **************************************************************
 *                       Instance functions
 **************************************************************
 */

/*
 *  ======== NotifyDriverCirc_Instance_init ========
 */
Void NotifyDriverCirc_Instance_init(NotifyDriverCirc_Object *obj,
    const NotifyDriverCirc_Params *params)
{
    SizeT  ctrlSize, circBufSize;
    Hwi_Params  hwiParams;

    /*
     *  This code assumes that the device's C28 and M3 MultiProc Ids
     *  are next to each other (e.g. n and n + 1) and that the first
     *  one is even (e.g. n is even).
     */
    /* set the remote processor's id */
    if (MultiProc_self() & 1) {
        /* I'm odd */
        obj->remoteProcId = MultiProc_self() - 1;
    }
    else {
        /* I'm even */
        obj->remoteProcId = MultiProc_self() + 1;
    }

    /* calculate the circular buffer size one-way */
    circBufSize = sizeof(NotifyDriverCirc_EventEntry) *
                  NotifyDriverCirc_numMsgs;

    /* calculate the control size one-way */
    ctrlSize = sizeof(Bits32);

    /*
     *  Init put/get buffer and index pointers.
     */
    obj->putBuffer = params->writeAddr;

    obj->putWriteIndex = (Bits32 *)((UInt32)obj->putBuffer + circBufSize);

    obj->getBuffer = params->readAddr;

    obj->getWriteIndex = (Bits32 *)((UInt32)obj->getBuffer + circBufSize);

    obj->putReadIndex = (Bits32 *)((UInt32)obj->getWriteIndex + ctrlSize);

    obj->getReadIndex = (Bits32 *)((UInt32)obj->putWriteIndex + ctrlSize);

    /* init the putWrite and getRead Index to 0 */
    obj->putWriteIndex[0] = 0;
    obj->getReadIndex[0] = 0;

    /* clear interrupt */
    NotifyDriverCirc_intClear();

    /* plugged interrupt */
    Hwi_Params_init(&hwiParams);
    hwiParams.arg = (UArg)obj;
    Hwi_create(NotifyDriverCirc_localIntId,
              (Hwi_FuncPtr)NotifyDriverCirc_isr,
              &hwiParams,
              NULL);

    /* Enable the interrupt */
    Hwi_enableInterrupt(NotifyDriverCirc_localIntId);
}

/*
 *  ======== NotifyDriverCirc_Instance_finalize ========
 */
Void NotifyDriverCirc_Instance_finalize(NotifyDriverCirc_Object *obj)
{
    Hwi_Handle  hwiHandle;

    /* Disable the interrupt source */
    NotifyDriverCirc_intDisable();

    /* Delete the Hwi */
    hwiHandle = Hwi_getHandle(NotifyDriverCirc_localIntId);
    Hwi_delete(&hwiHandle);
}

/*
 *  ======== NotifyDriverCirc_registerEvent ========
 */
Void NotifyDriverCirc_registerEvent(NotifyDriverCirc_Object *obj,
                                    UInt32 eventId)
{
    UInt hwiKey;

    /*
     *  Disable interrupt line to ensure that isr doesn't
     *  preempt registerEvent and encounter corrupt state
     */
    hwiKey = Hwi_disable();

    /* Set the 'registered' bit */
    SET_BIT(obj->evtRegMask, eventId);

    /* Restore the interrupt line */
    Hwi_restore(hwiKey);
}

/*
 *  ======== NotifyDriverCirc_unregisterEvent ========
 */
Void NotifyDriverCirc_unregisterEvent(NotifyDriverCirc_Object *obj,
                                      UInt32 eventId)
{
    UInt hwiKey;

    /*
     *  Disable interrupt line to ensure that isr doesn't
     *  preempt registerEvent and encounter corrupt state
     */
    hwiKey = Hwi_disable();

    /* Clear the registered bit */
    CLEAR_BIT(obj->evtRegMask, eventId);

    /* Restore the interrupt line */
    Hwi_restore(hwiKey);
}

/*
 *  ======== NotifyDriverCirc_sendEvent ========
 */
Int NotifyDriverCirc_sendEvent(NotifyDriverCirc_Object *obj,
                               UInt32 eventId,
                               UInt32 payload,
                               Bool  waitClear)
{
    Bool loop = FALSE;
    UInt hwiKey;
    UInt32 writeIndex, readIndex;
    NotifyDriverCirc_EventEntry *eventEntry;

    /* Retrieve the get Index. */
    readIndex = obj->putReadIndex[0];

    do {
        /* disable interrupts */
        hwiKey = Hwi_disable();

        /* retrieve the put index */
        writeIndex = obj->putWriteIndex[0];

        /* if slot available 'break' out of loop */
        if (((writeIndex + 1) & NotifyDriverCirc_maxIndex) != readIndex) {
            break;
        }

        /* restore interrupts */
        Hwi_restore(hwiKey);

        /* check to make sure code has looped */
        if (loop && !waitClear) {
            /* if no slot available and waitClear is 'FALSE' */
            return (Notify_E_FAIL);
        }

        /* re-read the get count */
        readIndex = obj->putReadIndex[0];

        /* convey that the code has looped around */
        loop = TRUE;

    } while (1);

    /* interrupts are disabled at this point */

    /* calculate the next available entry */
    eventEntry = (NotifyDriverCirc_EventEntry *)((UInt32)obj->putBuffer +
                 (writeIndex * sizeof(NotifyDriverCirc_EventEntry)));

    /* Set the eventId field and payload for the entry */
    eventEntry->eventid = eventId;
    eventEntry->payload = payload;

    /* update the putWriteIndex */
    obj->putWriteIndex[0] = (writeIndex + 1) & NotifyDriverCirc_maxIndex;

    /* restore interrupts */
    Hwi_restore(hwiKey);

    /* Send an interrupt to the Remote Processor */
    NotifyDriverCirc_intSend();

    return (Notify_S_SUCCESS);
}

/*
 *  ======== NotifyDriverCirc_disable ========
 */
Void NotifyDriverCirc_disable(NotifyDriverCirc_Object *obj)
{
    /* Disable the incoming interrupt line */
    NotifyDriverCirc_intDisable();
}

/*
 *  ======== NotifyDriverCirc_enable ========
 */
Void NotifyDriverCirc_enable(NotifyDriverCirc_Object *obj)
{
    /* NotifyDriverCirc_enableEvent not supported by this driver */
    Assert_isTrue(FALSE, NotifyDriverCirc_A_notSupported);
}

/*
 *  ======== NotifyDriverCirc_disableEvent ========
 *  This function disbales all events.
 */
Void NotifyDriverCirc_disableEvent(NotifyDriverCirc_Object *obj,
                                   UInt32 eventId)
{
    /* NotifyDriverCirc_disableEvent not supported by this driver */
    Assert_isTrue(FALSE, NotifyDriverCirc_A_notSupported);
}

/*
 *  ======== NotifyDriverCirc_enableEvent ========
 *  This function enables all events.
 */
Void NotifyDriverCirc_enableEvent(NotifyDriverCirc_Object *obj,
                                  UInt32 eventId)
{
    /* Enable the incoming interrupt line */
    NotifyDriverCirc_intEnable();
}

/*
 *************************************************************************
 *                       Module functions
 *************************************************************************
 */

/*
 *  ======== NotifyDriverCirc_sharedMemReq ========
 */
SizeT NotifyDriverCirc_sharedMemReq(const NotifyDriverCirc_Params *params)
{
    SizeT memReq;

    /* Ensure that params is non-NULL */
    Assert_isTrue(params != NULL, IpcMgr_A_internal);

    /*
     *  Amount of shared memory:
     *  1 putBuffer with numMsgs +
     *  1 putWriteIndex ptr      +
     *  1 putReadIndex ptr
     */
    memReq =
        (sizeof(NotifyDriverCirc_EventEntry) * NotifyDriverCirc_numMsgs)
        + (2 * sizeof(Bits32));

    return (memReq);
}

/*
 *************************************************************************
 *                       Internal functions
 *************************************************************************
 */
/*
 *  ======== NotifyDriverCirc_intEnable ========
 *  Enable remote processor interrupt
 */
Void NotifyDriverCirc_intEnable()
{
    Hwi_enableInterrupt(NotifyDriverCirc_localIntId);
}

/*
 *  ======== NotifyDriverCirc_intDisable ========
 *  Disables remote processor interrupt
 */
Void NotifyDriverCirc_intDisable()
{
    Hwi_disableInterrupt(NotifyDriverCirc_localIntId);
}

/*
 *  ======== NotifyDriverCirc_intSend ========
 *  Send interrupt to the remote processor
 */
Void NotifyDriverCirc_intSend()
{
#ifdef xdc_target__isaCompatible_v7M
    volatile UInt32 *set = (volatile UInt32 *)MTOCIPCSET;
#else
    volatile UInt16 *set = (volatile UInt16 *)CTOMIPCSET;
#endif

    *set = 1 << IpcMgr_ipcSetFlag;
}

/*
 *  ======== NotifyDriverCirc_intClear ========
 *  Clear interrupt
 */
UInt NotifyDriverCirc_intClear()
{
#ifdef xdc_target__isaCompatible_v7M
    volatile UInt32 *ack = (volatile UInt32 *)CTOMIPCACK;
#else
    volatile UInt16 *ack = (volatile UInt16 *)MTOCIPCACK;
#endif

    *ack = 1 << IpcMgr_ipcSetFlag;

    return (0);
}

/*
 *  ======== NotifyDriverCirc_isr ========
 */
Void NotifyDriverCirc_isr(UArg arg)
{
    NotifyDriverCirc_EventEntry *eventEntry;
    NotifyDriverCirc_Object     *obj;
    UInt32 writeIndex, readIndex;

    obj = (NotifyDriverCirc_Object *)arg;

    /* Make sure the NotifyDriverCirc_Object is not NULL */
    Assert_isTrue(obj != NULL, IpcMgr_A_internal);

    /* Clear the remote interrupt */
    NotifyDriverCirc_intClear();

    /* get the writeIndex and readIndex */
    writeIndex = obj->getWriteIndex[0];
    readIndex = obj->getReadIndex[0];

    /* get the event */
    eventEntry = &(obj->getBuffer[readIndex]);

    /* if writeIndex != readIndex then there is an event to process */
    while (writeIndex != readIndex) {
        /*
         *  Check to make sure event is registered. If the event
         *  is not registered, the event is not processed and is lost.
         */
        if (TEST_BIT(obj->evtRegMask, eventEntry->eventid)) {
            /* Execute the callback function */
            ti_sdo_ipc_Notify_exec(obj->notifyHandle,
                                   eventEntry->eventid,
                                   eventEntry->payload);
        }

        /* update the readIndex. */
        readIndex = ((readIndex + 1) & NotifyDriverCirc_maxIndex);

        /* set the getReadIndex */
        obj->getReadIndex[0] = readIndex;

        /* get the next event */
        eventEntry = &(obj->getBuffer[readIndex]);
    }
}

/*
 *  ======== NotifyDriverCirc_setNotifyHandle ========
 */
Void NotifyDriverCirc_setNotifyHandle(NotifyDriverCirc_Object *obj,
                                      Ptr notifyHandle)
{
    /* Internally used, so no Assert needed */
    obj->notifyHandle = (ti_sdo_ipc_Notify_Handle)notifyHandle;
}
