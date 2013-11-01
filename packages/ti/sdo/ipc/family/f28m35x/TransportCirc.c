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
 *  ======== TransportCirc.c ========
 */

#include <string.h>  /* for memcpy() */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sdo/ipc/interfaces/IMessageQTransport.h>

#include <ti/sdo/ipc/family/f28m35x/IpcMgr.h>

#include "package/internal/TransportCirc.xdc.h"

#include <ti/sdo/utils/_MultiProc.h>
#include <ti/sdo/ipc/_Notify.h>
#include <ti/sdo/ipc/_MessageQ.h>

/* Need to use reserved notify events */
#undef TransportCirc_notifyEventId
#define TransportCirc_notifyEventId \
    ti_sdo_ipc_family_f28m35x_TransportCirc_notifyEventId + \
    (UInt32)((UInt32)Notify_SYSTEMKEY << 16)

/*
 *************************************************************************
 *                       Instance functions
 *************************************************************************
 */

/*
 *  ======== TransportCirc_Instance_init ========
 */
Int TransportCirc_Instance_init(TransportCirc_Object *obj,
        UInt16 remoteProcId, const TransportCirc_Params *params,
        Error_Block *eb)
{
    Int        status;
    Bool       flag;
    Swi_Handle swiHandle;
    Swi_Params swiParams;
    SizeT      circBufSize;

    swiHandle = TransportCirc_Instance_State_swiObj(obj);

    /* init the object fields */
    obj->priority     = params->priority;
    obj->remoteProcId = remoteProcId;

    /* construct the swi with lowest priority */
    Swi_Params_init(&swiParams);
    swiParams.arg0 = (UArg)obj;
    swiParams.priority = TransportCirc_swiPriority;
    Swi_construct(Swi_struct(swiHandle),
                 (Swi_FuncPtr)TransportCirc_swiFxn,
                 &swiParams, eb);

    /* calculate the circular buffer size one-way */
    circBufSize = TransportCirc_msgSize * TransportCirc_numMsgs;

    /* Init put/get buffer and index pointers. */
    obj->putBuffer = params->writeAddr;

    obj->putWriteIndex = (Bits32 *)((UInt32)obj->putBuffer + circBufSize);

    obj->getBuffer = params->readAddr;

    obj->getWriteIndex = (Bits32 *)((UInt32)obj->getBuffer + circBufSize);

    obj->putReadIndex = (Bits32 *)((UInt32)obj->getWriteIndex + sizeof(Bits32));

    obj->getReadIndex = (Bits32 *)((UInt32)obj->putWriteIndex + sizeof(Bits32));

    /* init the putWrite and getRead Index to 0 */
    obj->putWriteIndex[0] = 0;
    obj->getReadIndex[0] = 0;

    /* register the event with Notify */
    status = Notify_registerEventSingle(
                 remoteProcId,    /* remoteProcId */
                 0,         /* lineId */
                 TransportCirc_notifyEventId,
                 (Notify_FnNotifyCbck)TransportCirc_notifyFxn,
                 (UArg)swiHandle);

    if (status < 0) {
        Error_raise(eb, IpcMgr_E_internal, 0, 0);
        return (1);
    }

    /* Register the transport with MessageQ */
    flag = ti_sdo_ipc_MessageQ_registerTransport(
        TransportCirc_Handle_upCast(obj), remoteProcId, params->priority);

    if (flag == FALSE) {
        Error_raise(eb, IpcMgr_E_internal, 0, 0);
        return (2);
    }

    return (0);
}

/*
 *  ======== TransportCirc_Instance_finalize ========
 */
Void TransportCirc_Instance_finalize(TransportCirc_Object* obj, Int status)
{
    Swi_Handle     swiHandle;

    switch(status) {
        case 0:
            /* MessageQ_registerTransport succeeded */
            ti_sdo_ipc_MessageQ_unregisterTransport(obj->remoteProcId,
                obj->priority);

            /* OK to fall thru */

        case 1: /* Notify_registerEventSingle failed */

            /* OK to fall thru */

        case 2: /* MessageQ_registerTransport failed */
            Notify_unregisterEventSingle(
                obj->remoteProcId,
                0,
                TransportCirc_notifyEventId);
            break;
    }

    /* Destruct the swi */
    swiHandle = TransportCirc_Instance_State_swiObj(obj);
    if (swiHandle != NULL) {
        Swi_destruct(Swi_struct(swiHandle));
    }
}

/*
 *  ======== TransportCirc_put ========
 *  Assuming MessageQ_put is making sure that the arguments are ok
 */
Bool TransportCirc_put(TransportCirc_Object *obj, Ptr msg)
{
    Int status;
    UInt msgSize;
    UInt hwiKey;
    Ptr writeAddr;
    UInt writeIndex, readIndex;

    do {
        /* disable interrupts */
        hwiKey = Hwi_disable();

        /* get the writeIndex and readIndex */
        readIndex  = obj->putReadIndex[0];
        writeIndex = obj->putWriteIndex[0];

        /* if slot available 'break' out of loop */
        if (((writeIndex + 1) & TransportCirc_maxIndex) != readIndex) {
            break;
        }

        /* restore interrupts */
        Hwi_restore(hwiKey);

    } while (1);

    /* interrupts are disabled at this point */

    /* get a free buffer and copy the message into it */
    writeAddr = (Ptr)((UInt32)obj->putBuffer +
                (writeIndex * TransportCirc_msgSize));

    /* get the size of the message */
    msgSize = MessageQ_getMsgSize(msg);

    Assert_isTrue(msgSize <= TransportCirc_msgSize, IpcMgr_A_internal);

    /* copy message to the write buffer */
    memcpy(writeAddr, (Ptr)msg, msgSize);

    /* update the writeIndex */
    obj->putWriteIndex[0] = (writeIndex + 1) & TransportCirc_maxIndex;

    /* restore interrupts */
    Hwi_restore(hwiKey);

    /* free the app's message */
    if (((MessageQ_Msg)msg)->heapId != ti_sdo_ipc_MessageQ_STATICMSG) {
        MessageQ_free(msg);
    }

    /* Notify the remote processor */
    status = Notify_sendEvent(
                 obj->remoteProcId,
                 0,
                 TransportCirc_notifyEventId,
                 (UInt32)NULL,
                 FALSE);

    /* check the status of the sendEvent */
    if (status < 0) {
        return (FALSE);
    }

    return (TRUE);
}

/*
 *  ======== TransportCirc_control ========
 */
Bool TransportCirc_control(TransportCirc_Object *obj, UInt cmd,
    UArg cmdArg)
{
    return (FALSE);
}

/*
 *  ======== TransportCirc_getStatus ========
 */
Int TransportCirc_getStatus(TransportCirc_Object *obj)
{
    return (0);
}

/*
 *************************************************************************
 *                       Module functions
 *************************************************************************
 */

/*
 *  ======== TransportCirc_notifyFxn ========
 */
Void TransportCirc_notifyFxn(UInt16 procId,
                             UInt16 lineId,
                             UInt32 eventId,
                             UArg arg,
                             UInt32 payload)
{
    Swi_Handle swiHandle;

    /* Swi_Handle was passed as arg in register */
    swiHandle = (Swi_Handle)arg;

    /* post the Swi */
    Swi_post(swiHandle);
}

/*
 *  ======== TransportCirc_swiFxn ========
 */
Void TransportCirc_swiFxn(UArg arg)
{
    UInt32 queueId;
    TransportCirc_Object *obj = (TransportCirc_Object *)arg;
    MessageQ_Msg msg = NULL;
    MessageQ_Msg buf = NULL;
    SizeT msgSize;
    UInt writeIndex, readIndex;
    UInt offset;

    /* Make sure the TransportCirc_Object is not NULL */
    Assert_isTrue(obj != NULL, IpcMgr_A_internal);

    /* get the writeIndex and readIndex */
    writeIndex = obj->getWriteIndex[0];
    readIndex  = obj->getReadIndex[0];

    while (writeIndex != readIndex) {
        /* determine where the message from remote core is */
        offset = (readIndex * TransportCirc_msgSize);

        /* get the message */
        msg = (MessageQ_Msg)((UInt32)obj->getBuffer + offset);

#ifdef xdc_target__isaCompatible_28
        /*
         *  The message size needs to be halved because it was
         *  specified in bytes and the units on the c28 is 16-bit
         *  words.
         */
        msgSize = MessageQ_getMsgSize(msg) >> 1;
#else
        /*
         *  The message size needs to be doubled because it was
         *  specified in 16-bit words and the units on the m3 is
         *  in bytes.
         */
        msgSize = MessageQ_getMsgSize(msg) << 1;
#endif

        /* alloc a message from msg->heapId to copy the msg to */
        buf = MessageQ_alloc(msg->heapId, msgSize);

        /* Make sure buf is not NULL */
        if (buf == NULL) {
            TransportCirc_module->errFxn(TransportCirc_Reason_FAILEDALLOC,
                ti_sdo_ipc_family_f28m35x_TransportCirc_Handle_upCast(obj),
                NULL,
                (UArg)msg);

            return;
        }

        /* copy the message to the buffer allocated, set the heap id */
        memcpy((Ptr)buf, (Ptr)msg, msgSize);

        /* overwrite the msgSize in the msg */
        buf->msgSize = msgSize;

        /* retrieve the detination queue id */
        queueId = MessageQ_getDstQueue(msg);

        /* put the message to the destination queue */
        MessageQ_put(queueId, buf);

        /* update the local readIndex. */
        readIndex = ((readIndex + 1) & TransportCirc_maxIndex);

        /* set the readIndex */
        obj->getReadIndex[0] = readIndex;
    }
}

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*
 *  ======== TransportCirc_close ========
 */
Void TransportCirc_close(TransportCirc_Handle *handle)
{
    TransportCirc_delete(handle);
}

/*
 *  ======== TransportCirc_sharedMemReq ========
 */
SizeT TransportCirc_sharedMemReq(const TransportCirc_Params *params)
{
    SizeT memReq;

    /* Ensure that params is non-NULL */
    Assert_isTrue(params != NULL, IpcMgr_A_internal);

    /*
     *  Amount of shared memory:
     *  1 putBuffer (msgSize * numMsgs) +
     *  1 putWriteIndex ptr             +
     *  1 putReadIndex ptr              +
     */
    memReq = (TransportCirc_msgSize * TransportCirc_numMsgs) +
             (2 * sizeof(Bits32));

    return (memReq);
}

/*
 *  ======== TransportCirc_defaultErrFxn ========
 */
Void TransportCirc_defaultErrFxn(IMessageQTransport_Reason reason,
                                 IMessageQTransport_Handle handle,
				 Ptr ptr,
				 UArg arg)
{
}

/*
 *  ======== TransportCirc_setErrFxn ========
 */
Void TransportCirc_setErrFxn(TransportCirc_ErrFxn errFxn)
{
    TransportCirc_module->errFxn = errFxn;
}
