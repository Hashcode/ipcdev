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
 *  ======== TransportCirc.xs ========
 */

var TransportCirc = null;
var MessageQ     = null;
var Notify       = null;
var MultiProc    = null;
var Swi          = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    TransportCirc = this;
    MessageQ     = xdc.useModule("ti.sdo.ipc.MessageQ");
    Notify       = xdc.useModule("ti.sdo.ipc.Notify");
    MultiProc    = xdc.useModule("ti.sdo.utils.MultiProc");
    Swi          = xdc.useModule("ti.sysbios.knl.Swi");
}

/*
 * ======== module$static$init ========
 */
function module$static$init(mod, params)
{
    var target = Program.build.target;
    var bitsPerByte = 8;
    var bitsPerChar = target.bitsPerChar;

    /* calculate the msgSize */
    TransportCirc.msgSize = TransportCirc.maxMsgSizeInBytes *
                              (bitsPerByte / bitsPerChar);

    /* calculate the maxIndex */
    TransportCirc.maxIndex = TransportCirc.numMsgs - 1;

    /* determine numMsgs is a power of 2 */
    if (TransportCirc.numMsgs & (TransportCirc.maxIndex)) {
        TransportCirc.$logFatal("TransportCirc.numMsgs: " +
                TransportCirc.numMsgs +
                " is not a power of 2", TransportCirc);
    }
}

/*
 *  ======== module$validate ========
 */
function module$validate()
{
    if (Notify.numEvents <= TransportCirc.notifyEventId) {
        TransportCirc.$logFatal("TransportCirc.notifyEventId (" +
                TransportCirc.notifyEventId +
                ") is too big: Notify.numEvents = " + Notify.numEvents,
                TransportCirc);
    }
}

/*
 *  ======== sharedMemReqMeta ========
 */
function sharedMemReqMeta(params)
{
    var target = Program.build.target;
    var bitsPerByte = 8;
    var bitsPerChar = target.bitsPerChar;

    /* calculate the msgSize */
    var msgSize = TransportCirc.maxMsgSizeInBytes *
                  (bitsPerByte / bitsPerChar);

    /*
     *  Amount of shared memory:
     *  1 putBuffer (msgSize * numMsgs) +
     *  1 putWriteIndex ptr             +
     *  1 putReadIndex ptr              +
     */
    var memReq = (msgSize * TransportCirc.numMsgs) +
                 (2 *  Program.build.target.stdTypes['t_Int32'].size);

    return (memReq);
}

/*
 *************************************************************************
 *                       ROV View functions
 *************************************************************************
 */

/*
 *  ======== viewInitBasic ========
 */
function viewInitBasic(view, obj)
{
    var MultiProc = xdc.useModule('ti.sdo.utils.MultiProc');
    var MultiProcCfg = Program.getModuleConfig('ti.sdo.utils.MultiProc');

    /* view.remoteProcName */
    try {
        view.remoteProcName = MultiProc.getName$view(obj.remoteProcId -
                                  MultiProcCfg.baseIdOfCluster);
    }
    catch(e) {
        Program.displayError(view, 'remoteProcName',
                             "Problem retrieving proc name: " + e);
    }
}

/*
 *  ======== getEventData ========
 *  Helper function for use within viewInitData
 */
function getEventData(view, obj, bufferPtr, putIndex, getIndex)
{
    var TransportCirc =
        xdc.useModule('ti.sdo.ipc.family.f28m35x.TransportCirc');
    var modCfg =
        Program.getModuleConfig('ti.sdo.ipc.family.f28m35x.TransportCirc');
    var ScalarStructs   = xdc.useModule('xdc.rov.support.ScalarStructs');

    if (bufferPtr == obj.putBuffer) {
        var bufferName = "put";
    }
    else {
        var bufferName = "get";
    }

    try {
        var putBuffer = Program.fetchArray(ScalarStructs.S_Bits32$fetchDesc,
                                           bufferPtr,
                                           modCfg.numMsgs);
    }
    catch(e) {
        throw (new Error("Error fetching putBuffer struct from shared memory"));
    }

    var i = getIndex;

    while (i != putIndex) {
        /* The event is registered */
        var elem = Program.newViewStruct(
                'ti.sdo.ipc.family.f28m35x.TransportCirc',
                'Events');

        elem.index = i;
        elem.buffer = bufferName;
        elem.message = utils.toHex(Number(bufferPtr) + (i * modCfg.msgSize));

        /* Create a new row in the instance data view */
        view.elements.$add(elem);

        i++;
        if ((i % modCfg.numMsgs) == 0) {
            i = 0;
        }
    }
}


/*
 *  ======== viewInitData ========
 *  Instance data view.
 */
function viewInitData(view, obj)
{
    var Program         = xdc.useModule('xdc.rov.Program');
    var ScalarStructs   = xdc.useModule('xdc.rov.support.ScalarStructs');
    var MultiProc       = xdc.useModule('ti.sdo.utils.MultiProc');

    /* Display the instance label in the tree */
    view.label = "remoteProcId = " + obj.remoteProcId;

    /* Fetch put/get index's */
    try {
        var putWriteIndex = Program.fetchStruct(ScalarStructs.S_Bits32$fetchDesc,
                                                obj.putWriteIndex);
    }
    catch(e) {
        throw (new Error("Error fetching putWriteIndex struct from shared memory"));
    }

    try {
        var putReadIndex = Program.fetchStruct(ScalarStructs.S_Bits32$fetchDesc,
                                               obj.putReadIndex);
    }
    catch(e) {
        throw (new Error("Error fetching putReadIndex struct from shared memory"));
    }

    try {
        var getWriteIndex = Program.fetchStruct(ScalarStructs.S_Bits32$fetchDesc,
                                                obj.getWriteIndex);
    }
    catch(e) {
        throw (new Error("Error fetching getWriteIndex struct from shared memory"));
    }

    try {
        var getReadIndex = Program.fetchStruct(ScalarStructs.S_Bits32$fetchDesc,
                                               obj.getReadIndex);
    }
    catch(e) {
        throw (new Error("Error fetching getReadIndex struct from shared memory"));
    }

    /* Get event data for the put buffer */
    getEventData(view, obj, obj.putBuffer, putWriteIndex.elem, putReadIndex.elem);

    /* Get event data for the get buffer */
    getEventData(view, obj, obj.getBuffer, getWriteIndex.elem, getReadIndex.elem);
}
