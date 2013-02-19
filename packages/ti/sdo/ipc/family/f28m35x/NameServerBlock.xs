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
 *  ======== NameServerBlock.xs ========
 */

var NameServerBlock = null;
var NameServer      = null;
var MultiProc       = null;
var Notify          = null;
var Semaphore       = null;
var Swi             = null;
var IpcMgr          = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    NameServerBlock = this;
    NameServer      = xdc.useModule("ti.sdo.utils.NameServer");
    MultiProc       = xdc.useModule("ti.sdo.utils.MultiProc");
    Notify          = xdc.useModule("ti.sdo.ipc.Notify");
    Semaphore       = xdc.useModule("ti.sysbios.knl.Semaphore");
    Swi             = xdc.useModule("ti.sysbios.knl.Swi");
    IpcMgr          = xdc.useModule('ti.sdo.ipc.family.f28m35x.IpcMgr');
}

/*
 *  ======== module$validate ========
 */
function module$validate()
{
    if (Notify.numEvents <= NameServerBlock.notifyEventId) {
        NameServerBlock.$logFatal(
            "NameServerBlock.notifyEventId (" +
            NameServerBlock.notifyEventId +
            ") is too big: Notify.numEvents = " + Notify.numEvents,
            NameServerBlock);
    }
}

/*
 *  ======== sharedMemReqMeta ========
 */
function sharedMemReqMeta(params)
{
    /*
     *  Four Message structs are required.
     *  One for sending request and one for sending response.
     *  One for receiving request and one for receiving response.
     */
    if (MultiProc.numProcessors > 1) {
        return (4 * NameServerBlock.Message.$sizeof());
    }

    return (0);
}
