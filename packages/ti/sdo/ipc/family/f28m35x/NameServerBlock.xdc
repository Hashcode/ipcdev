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
 *  ======== NameServerBlock.xdc ========
 */

import xdc.runtime.Error;
import xdc.runtime.Assert;
import ti.sysbios.knl.Swi;
import ti.sysbios.knl.Semaphore;
import ti.sdo.utils.INameServerRemote;

/*! @_nodoc
 *  ======== NameServerBlock ========
 *  Used by NameServer to communicate to remote processors.
 *
 *  This module is used by {@link ti.sdo.utils.NameServer} to communicate
 *  to remote processors using {@link ti.sdo.ipc.Notify} and shared memory.
 *  There needs to be one instance between each two cores in the system.
 *  Interrupts must be enabled before using this module.  Currently
 *  supports transferring up to 44-bytes between two cores.
 */
@InstanceInitError
@InstanceFinalize

module NameServerBlock inherits INameServerRemote
{
    /* structure in shared memory for retrieving value */
    struct Message {
        Bits32  request;            /* if this is a request set to 1    */
        Bits32  response;           /* if this is a response set to 1   */
        Bits32  requestStatus;      /* if request sucessful set to 1    */
        Bits32  value;              /* holds value if len <= 4          */
        Bits32  valueLen;           /* len of value                     */
        Bits32  instanceName[8];    /* name of NameServer instance      */
        Bits32  name[8];            /* name of NameServer entry         */
        Bits32  valueBuf[bufLen];   /* supports up to 44-byte value     */
    };

    /*!
     *  Assert raised when length of value larger then 44 bytes.
     */
    config xdc.runtime.Assert.Id A_invalidValueLen =
        {msg: "A_invalidValueLen: Invalid valueLen (too large)"};

    /*! @_nodoc
     *  ======== notifyEventId ========
     *  The Notify event ID.
     */
    config UInt32 notifyEventId = 4;

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

    /*!
     *  ======== readAddr ========
     *  Read address of the shared memory
     *
     *  The address must be specified in the local core's memory space.
     *  It must point to the same physical address as the writeAddr for
     *  the remote processor.
     */
    config Ptr readAddr = null;

    /*!
     *  ======== writeAddr ========
     *  Write address of the shared memory
     *
     *  The address must be specified in the local core's memory space.
     *  It must point to the same physical address as the readAddr for
     *  the remote processor.
     */
    config Ptr writeAddr = null;

internal:

    /*! used by Message struct for length of valueBuf */
    const UInt bufLen = 11;

    /*!
     *  ======== cbFxn ========
     *  The call back function registered with Notify.
     *
     *  This function is registered with Notify as a call back function
     *  when the specified event is triggered.  This function simply posts
     *  a Swi which will process the event.
     *
     *  @param(procId)          Source proc id
     *  @param(lineId)          Interrupt line id
     *  @param(eventId)         the Notify event id.
     *  @param(arg)             the argument for the function.
     *  @param(payload)         a 32-bit payload value.
     */
    Void cbFxn(UInt16 procId, UInt16 lineId, UInt32 eventId, UArg arg,
               UInt32 payload);

    /*!
     *  ======== strncpy ========
     *  Copies the source string into the destination string.
     *
     *  For the M3, this function calls the 'C' runtime strncpy().
     *  For the C28, this function packs the source string into
     *  the destination string.
     *
     *  @param(dest)        Destination string.
     *  @param(src)         Source string.
     *  @param(len)         len of string to copy.
     *
     *  @a(returns)         Destination string.
     */
    Char* strncpy(Char *dest, Char *src, SizeT len);

    /*!
     *  ======== swiFxn ========
     *  The swi function that will be executed during the call back.
     *
     *  @param(arg)     argument to swi function
     */
    Void swiFxn(UArg arg);

    /* instance state */
    struct Instance_State {
        Message             *readRequest;
        Message             *readResponse;
        Message             *writeRequest;  /* Ptrs to messages in shared mem */
        Message             *writeResponse; /* Ptrs to messages in shared mem */
        UInt16              regionId;       /* SharedRegion ID                */
        Semaphore.Object    semRemoteWait;  /* sem to wait on remote proc     */
        Semaphore.Object    semMultiBlock;  /* sem to block multiple threads  */
        Swi.Object          swiObj;         /* instance swi object            */
        UInt16              remoteProcId;   /* remote MultiProc id            */
    };
}
