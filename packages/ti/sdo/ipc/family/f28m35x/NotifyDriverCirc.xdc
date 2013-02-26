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
 *  ======== NotifyDriverCirc.xdc ================
 */

import ti.sdo.utils.MultiProc;
import ti.sdo.ipc.interfaces.INotifyDriver;
import ti.sdo.ipc.notifyDrivers.IInterrupt;
import ti.sdo.ipc.Notify;

import xdc.rov.ViewInfo;

import xdc.runtime.Assert;

/*!
 *  ======== NotifyDriverCirc ========
 *  Shared memory driver using circular buffer for F28M35x devices.
 *
 *  This is a {@link ti.sdo.ipc.Notify} driver that utilizes shared memory
 *  and inter-processor hardware interrupts for notification between cores.
 *
 *  This driver is designed to work with only F28M35x family of devices.
 *  This module needs to be plugged with an appropriate module that implements
 *  the {@link ti.sdo.ipc.notifyDrivers.IInterrupt} interface for a given
 *  device.
 *
 *  The driver utilizes shared memory in the manner indicated by the following
 *  diagram.
 *
 *  @p(code)
 *
 *  NOTE: Processor '0' corresponds to the M3 and '1' corresponds to the C28
 *
 * sharedAddr -> --------------------------- bytes
 *               |  eventEntry0  (0)       | 8
 *               |  eventEntry1  (0)       | 8
 *               |  ...                    |
 *               |  eventEntry15 (0)       | 8
 *               |                         |
 *               |-------------------------|
 *               |  eventEntry16 (0)       | 8
 *               |  eventEntry17 (0)       | 8
 *               |  ...                    |
 *               |  eventEntry31 (0)       | 8
 *               |                         |
 *               |-------------------------|
 *               |  putWriteIndex (0)      | 4
 *               |                         |
 *               |-------------------------|
 *               |  getReadIndex (1)       | 4
 *               |                         |
 *               |-------------------------|
 *               |  eventEntry0  (1)       | 8
 *               |  eventEntry1  (1)       | 8
 *               |  ...                    |
 *               |  eventEntry15 (1)       | 8
 *               |                         |
 *               |-------------------------|
 *               |  eventEntry16 (1)       | 8
 *               |  eventEntry17 (1)       | 8
 *               |  ...                    |
 *               |  eventEntry31 (1)       | 8
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
 *
 *  @p
 */

@InstanceFinalize

module NotifyDriverCirc inherits ti.sdo.ipc.interfaces.INotifyDriver
{
    /*! @_nodoc */
    metaonly struct BasicView {
        String      remoteProcName;
        UInt        bufSize;
        UInt        spinCount;
        UInt        maxSpinWait;
    }

    /*! @_nodoc */
    metaonly struct EventDataView {
        UInt        index;
        String      buffer;
        Ptr         addr;
        UInt        eventId;
        Ptr         payload;
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
     *  Assert raised when trying to use Notify_[enable/disable]Event with
     *  NotifyDriverCirc
     */
    config Assert.Id A_notSupported =
        {msg: "A_notSupported: [enable/disable]Event not supported by NotifyDriverCirc"};

    /*! @_nodoc
     *  ======== numMsgs ========
     *  The number of messages or slots in the circular buffer
     *
     *  This is use to determine the size of the put and get buffers.
     *  Each eventEntry is two 32bits wide, therefore the total size
     *  of each circular buffer is [numMsgs * sizeof(eventEntry)].
     */
    config UInt numMsgs = 16;

    /*!
     *  ======== sharedMemReq ========
     *  Amount of shared memory required for creation of each instance
     *
     *  @param(params)      Pointer to parameters that will be used in the
     *                      create
     *
     *  @a(returns)         Number of MAUs in shared memory needed to create
     *                      the instance.
     */
    SizeT sharedMemReq(const Params *params);

    /*! @_nodoc
     *  ======== sharedMemReqMeta ========
     *  Amount of shared memory required
     *
     *  @param(params)      Pointer to the parameters that will be used in
     *                      create.
     *
     *  @a(returns)         Size of shared memory in MAUs on local processor.
     */
    metaonly SizeT sharedMemReqMeta(const Params *params);

instance:

    /*!
     *  ======== readAddr ========
     *  Address in shared memory where buffer is placed
     *
     *  Use {@link #sharedMemReq} to determine the amount of shared memory
     *  required.
     */
    config Ptr readAddr = null;

    /*!
     *  ======== writeAddr ========
     *  Address in shared memory where buffer is placed
     *
     *  Use {@link #sharedMemReq} to determine the amount of shared memory
     *  required.
     */
    config Ptr writeAddr = null;

internal:

    /*!
     *  ======== localIntId ========
     *  Local interrupt ID for interrupt line
     *
     *  For devices that support multiple inter-processor interrupt lines, this
     *  configuration parameter allows selecting a specific line to use for
     *  receiving an interrupt.  The value specified here corresponds to the
     *  incoming interrupt line on the local processor.
     */
    config UInt localIntId;

    /*!
     *  ======== remoteIntId ========
     *  Remote interrupt ID for interrupt line
     *
     *  For devices that support multiple inter-processor interrupt lines, this
     *  configuration parameter allows selecting a specific line to use for
     *  receiving an interrupt.  The value specified here corresponds to the
     *  incoming interrupt line on the remote processor.
     */
    config UInt remoteIntId;

    /*! The max index set to (numMsgs - 1) */
    config UInt maxIndex;

    /*!
     *  The modulo index value. Set to (numMsgs / 4).
     *  Used in the isr for doing cache_wb of readIndex.
     */
    config UInt modIndex;

    /*!
     *  enable IPC interrupt
     */
    Void intEnable();

    /*!
     *  disable IPC interrupt
     */
    Void intDisable();

    /*!
     *  trigger IPC interrupt
     */
    Void intSend();

    /*!
     *  clear IPC interrupt
     */
    UInt intClear();

    /*!
     *  executes the callback functions according to event priority
     */
    Void isr(UArg arg);

    /*!
     *  Structure for each event. This struct is placed in shared memory.
     */
    struct EventEntry {
        volatile Bits32 eventid;
        volatile Bits32 payload;
    }

    /*! Instance state structure */
    struct Instance_State {
        EventEntry       *putBuffer;     /* buffer used to put events        */
        Bits32           *putReadIndex;  /* ptr to readIndex for put buffer  */
        Bits32           *putWriteIndex; /* ptr to writeIndex for put buffer */
        EventEntry       *getBuffer;     /* buffer used to get events        */
        Bits32           *getReadIndex;  /* ptr to readIndex for get buffer  */
        Bits32           *getWriteIndex; /* ptr to writeIndex for put buffer */
        Bits32           evtRegMask;     /* local event register mask        */
        Notify.Handle    notifyHandle;   /* Handle to front-end object       */
        UInt16           remoteProcId;   /* Remote MultiProc id              */
    }
}
