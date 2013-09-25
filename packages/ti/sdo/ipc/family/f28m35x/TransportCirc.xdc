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
 *  ======== TransportCirc.xdc ========
 */

import ti.sysbios.knl.Swi;
import ti.sdo.ipc.interfaces.IMessageQTransport;

import xdc.rov.ViewInfo;

/*!
 *  ======== TransportCirc ========
 *  Transport for MessageQ that uses a circular buffer.
 *
 *  This is a {@link ti.sdo.ipc.MessageQ} transport that utilizes shared
 *  memory for passing messages between multiple processors.
 *
 *  The transport utilizes shared memory in the manner indicated by the
 *  following diagram.
 *
 *  @p(code)
 *
 *  NOTE: Processor '0' corresponds to the M3 and '1' corresponds to the C28
 *
 * sharedAddr -> --------------------------- bytes
 *               |  entry0  (0) [Put]      | msgSize
 *               |  entry1  (0)            | msgSize
 *               |  ...                    |
 *               |  entryN  (0)            | msgSize
 *               |                         |
 *               |-------------------------|
 *               |  putWriteIndex (0)      | 4
 *               |                         |
 *               |-------------------------|
 *               |  getReadIndex (1)       | 4
 *               |                         |
 *               |-------------------------|
 *               |  entry0  (1) [Get]      | msgSize
 *               |  entry1  (1)            | msgSize
 *               |  ...                    |
 *               |  entryN  (1)            | msgSize
 *               |                         |
 *               |-------------------------|
 *               |  putWriteIndex (1)      | 4
 *               |                         |
 *               |-------------------------|
 *               |  getReadIndex (0)       | 4
 *               |                         |
 *               |-------------------------|
 *
 *
 *  Legend:
 *  (0), (1) : belongs to the respective processor
 *  (N)      : length of buffer
 *
 *  @p
 */

@InstanceFinalize
@InstanceInitError

module TransportCirc inherits IMessageQTransport
{
    /*! @_nodoc */
    metaonly struct BasicView {
        String      remoteProcName;
    }

    /*! @_nodoc */
    metaonly struct EventDataView {
        UInt        index;
        String      buffer;
        Ptr         message;
    }

    /*!
     *  ======== rovViewInfo ========
     */
    @Facet
    metaonly config ViewInfo.Instance rovViewInfo =
        ViewInfo.create({
            viewMap: [
                ['Basic',
                    {
                        type: ViewInfo.INSTANCE,
                        viewInitFxn: 'viewInitBasic',
                        structName: 'BasicView'
                    }
                ],
                ['Events',
                    {
                        type: ViewInfo.INSTANCE_DATA,
                        viewInitFxn: 'viewInitData',
                        structName: 'EventDataView'
                    }
                ],
            ]
        });

    /*!
     *  ======== close ========
     *  Close an opened instance
     *
     *  Closing an instance will free local memory consumed by the opened
     *  instance.  Instances that are opened should be closed before the
     *  instance is deleted.
     *
     *  @param(handle)  handle that is returned from an {@link #openByAddr}
     */
    Void close(Handle *handle);

    /*! @_nodoc
     *  ======== notifyEventId ========
     *  Notify event ID for transport.
     */
    config UInt16 notifyEventId = 2;

    /*! @_nodoc
     *  ======== numMsgs ========
     *  The maximum number of outstanding messages
     *
     *  This number must be greater than 0 and a power of 2.
     *  If the transport reaches this threshold, it spins waiting for
     *  another message slot to be freed by the remote processor.
     */
    config UInt numMsgs = 4;

    /*! @_nodoc
     *  ======== maxMsgSizeInBytes ========
     *  The maximum message size (in bytes) that is supported
     */
    config UInt maxMsgSizeInBytes = 128;

    /*! @_nodoc
     *  ======== sharedMemReq ========
     *  Amount of shared memory required for creation of each instance
     *
     *  @param(params)      Pointer to the parameters that will be used in
     *                      create.
     *
     *  @a(returns)         Number of MAUs needed to create the instance.
     */
    SizeT sharedMemReq(const Params *params);

    /*! @_nodoc
     *  ======== sharedMemReqMeta ========
     *  Amount of shared memory required for creation of each instance
     *
     *  @param(params)      Pointer to the parameters that will be used in
     *                      create.
     *
     *  @a(returns)         Size of shared memory in MAUs on local processor.
     */
    metaonly SizeT sharedMemReqMeta(const Params *params);

instance:

    /*! @_nodoc
     *  ======== openFlag ========
     *  Set to 'true' by the open() call. No one else should touch this!
     */
    config Bool openFlag = false;

    /*!
     *  ======== readAddr ========
     *  Physical address of the read address in shared memory
     *
     *  This address should be specified in the local processor's memory
     *  space.  It must point to the same physical write address of the
     *  remote processor its communicating with.
     */
    config Ptr readAddr = null;

    /*!
     *  ======== writeAddr ========
     *  Physical address of the write address in shared memory
     *
     *  This address should be specified in the local processor's memory
     *  space.  It must point to the same physical read address of the
     *  remote processor its communicating with.
     */
    config Ptr writeAddr = null;

    /*!
     *  ======== swiPriority ========
     *  The priority of the Transport Swi object created
     */
    config UInt swiPriority = 1;

internal:

    /*! The max index set to (numMsgs - 1) */
    config UInt maxIndex;

    /*!
     *  The message size calculated based on the target.
     */
    config UInt msgSize;

    /*!
     *  ======== defaultErrFxn ========
     *  This is the default error function.
     *
     *  This function is an empty function that does nothing.
     *
     *  @param(reason)  reason for error function
     *  @param(handle)  handle of transport that had error
     *  @param(ptr)     pointer to the message
     *  @param(arg)     argument passed to error function
     */
    Void defaultErrFxn(IMessageQTransport.Reason reason,
                       IMessageQTransport.Handle handle, Ptr ptr, UArg arg);

    /*!
     *  ======== swiFxn ========
     *  This function takes the messages from the transport ListMP and
     *  calls MessageQ_put to send them to their destination queue.
     *  This function is posted by the NotifyFxn.
     *
     *  @param(arg)     argument for the function
     */
    Void swiFxn(UArg arg);

    /*!
     *  ======== notifyFxn ========
     *  This is a callback function registered with Notify.  It is called
     *  when a remote processor does a Notify_sendEvent().  It is executed
     *  at ISR level.  It posts the instance Swi object to execute swiFxn.
     *
     *  @param(procId)  remote processor id
     *  @param(lineId)  Notify line id
     *  @param(eventId) Notify event id
     *  @param(arg)     argument for the function
     *  @param(payload) 32-bit payload value.
     */
    Void notifyFxn(UInt16 procId, UInt16 lineId, UInt32 eventId, UArg arg,
                   UInt32 payload);

    /* Instance State object */
    struct Instance_State {
        Ptr             *putBuffer;     /* buffer used to put message       */
        Bits32          *putReadIndex;  /* ptr to readIndex for put buffer  */
        Bits32          *putWriteIndex; /* ptr to writeIndex for put buffer */
        Ptr             *getBuffer;     /* buffer used to get message       */
        Bits32          *getReadIndex;  /* ptr to readIndex for get buffer  */
        Bits32          *getWriteIndex; /* ptr to writeIndex for put buffer */
        Swi.Object      swiObj;         /* Each instance has a swi          */
        SizeT           allocSize;      /* Shared memory allocated          */
        UInt16          remoteProcId;   /* dst proc id                      */
        UInt16          priority;       /* priority to register             */
    };

    struct Module_State {
        ErrFxn errFxn;                  /* error function */
    };
}
