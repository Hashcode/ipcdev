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
 *  ======== IpcMgr.xs ========
 */

var IpcMgr           = null;
var Startup          = null;
var MultiProc        = null;
var NotifyDriverCirc = null;
var NameServerBlock  = null;
var TransportCirc    = null;
var Hwi              = null;
var Boot             = null;

/*
 *  ======== module$meta$init ========
 */
function module$meta$init()
{
    /* Only process during "cfg" phase */
    if (xdc.om.$name != "cfg") {
        return;
    }

    IpcMgr = this;

    /* initialize the config parameter */
    for (var i=0; i < IpcMgr.sharedMemoryAccess.length; i++) {
        IpcMgr.sharedMemoryAccess[i] = 0;
    }

    var NameServer = xdc.useModule('ti.sdo.utils.NameServer');
    NameServer.SetupProxy = xdc.useModule('ti.sdo.ipc.family.f28m35x.NameServerBlock');

    var Startup = xdc.useModule('xdc.runtime.Startup');

    /* RAMINIT */
    Startup.firstFxns.$add(IpcMgr.init);
}

/*
 *  ======== module$use ========
 */
function module$use()
{
    MultiProc = xdc.useModule("ti.sdo.utils.MultiProc");
    NotifyDriverCirc =
        xdc.useModule("ti.sdo.ipc.family.f28m35x.NotifyDriverCirc");
    NameServerBlock =
        xdc.useModule("ti.sdo.ipc.family.f28m35x.NameServerBlock");
    TransportCirc =
        xdc.useModule("ti.sdo.ipc.family.f28m35x.TransportCirc");
    Hwi = xdc.useModule("ti.sysbios.hal.Hwi");
    Startup = xdc.useModule("xdc.runtime.Startup");

    if (Program.build.target.name.match(/M3.*/)) {
        Boot = xdc.useModule("ti.catalog.arm.cortexm3.concertoInit.Boot");
    }

    /* Init the number of messages for notify driver */
    NotifyDriverCirc.numMsgs = IpcMgr.numNotifyMsgs;

    /* Init the number of messages for messageQ transport */
    TransportCirc.numMsgs = IpcMgr.numMessageQMsgs;
    TransportCirc.maxMsgSizeInBytes = IpcMgr.messageQSize;
    TransportCirc.notifyEventId = IpcMgr.messageQEventId;

    /* Make sure that sharedMemoryOwnerMask is only configured on the M3 */
    if (!Program.build.target.name.match(/M3.*/) &&
        IpcMgr.$written("sharedMemoryOwnerMask") == true) {
        this.$logWarning("Warning: IpcMgr.sharedMemoryOwnerMask must only be " +
        "configured on the M3 core.  Configuring this on the C28 core has no " +
        "effect", this);
    }

    if ((Program.build.target.name.match(/M3.*/)) &&
        ("sharedMemoryEnable" in Boot)) {
        if (Boot.$written("sharedMemoryEnable") &&
            Boot.sharedMemoryEnable != IpcMgr.sharedMemoryEnable) {
            IpcMgr.$logWarning("Boot.sharedMemoryEnable was set to " +
            "a value different from IpcMgr.sharedMemoryEnable. " +
            "IpcMgr.sharedMemoryEnable will override the Boot setting.",
            IpcMgr, "sharedMemoryEnable");
        }

        /* override Boot's sharedMemoryEnable with IpcMgr's */
        Boot.sharedMemoryEnable = IpcMgr.sharedMemoryEnable;

        if (Boot.$written("sharedMemoryOwnerMask") &&
            Boot.sharedMemoryOwnerMask != IpcMgr.sharedMemoryOwnerMask) {
            IpcMgr.$logWarning("Boot.sharedMemoryOwnerMask was set to " +
            "a value different from IpcMgr.sharedMemoryOwnerMask. " +
            "IpcMgr.sharedMemoryOwnerMask will override the Boot setting.",
            IpcMgr, "sharedMemoryOwnerMask");
        }

        /* override Boot's sharedMemoryOwnerMask with IpcMgr's */
        Boot.sharedMemoryOwnerMask = IpcMgr.sharedMemoryOwnerMask;

        if (Boot.$written("sharedMemoryAccess")) {
            IpcMgr.$logWarning("Boot.sharedMemoryAccess was modified " +
            "but IpcMgr.sharedMemoryAccess will override the Boot setting.",
            IpcMgr, "sharedMemoryAccess");
        }

        /* override Boot's sharedMemoryAccess with IpcMgr's */
	for (var i = 0; i < IpcMgr.sharedMemoryAccess.length; i++) {
            Boot.sharedMemoryAccess[i] = IpcMgr.sharedMemoryAccess[i];
        }
    }
}

/*
 *  ======== module$static$init ========
 */
function module$static$init(mod, params)
{
    /* check to make sure readAddr and writeAddr have been set */
    if (params.readAddr == undefined) {
        IpcMgr.$logError("IpcMgr.readAddr is undefined", IpcMgr);
    }

    if (params.writeAddr == undefined) {
        IpcMgr.$logError("IpcMgr.writeAddr is undefined", IpcMgr);
    }


    /* init the interrupt ids */
    if (params.ipcSetFlag < 0 || params.ipcSetFlag > 3) {
        IpcMgr.$logError("IpcMgr.ipcSetFlag must be 0, 1, 2, or 3 " +
            "because they are the only flags associated with an interrupt.",
            IpcMgr);
    }

    /* calculate the amount of shared memory used */
    IpcMgr.sharedMemSizeUsed = NotifyDriverCirc.sharedMemReqMeta(null) +
                               NameServerBlock.sharedMemReqMeta(null) +
                               TransportCirc.sharedMemReqMeta(null);

    /* validate sharedMemoryOwnerMask is correct for the readAddr/writeAddr */
    if (Program.build.target.name.match(/M3.*/)) {
        if ((IpcMgr.writeAddr >= 0x20008000) &&
            (IpcMgr.writeAddr < 0x20018000)) {
            /*
             *  Determine segment being used for the M3 writeAddr.
             *  The shared RAM base address starts 0x20008000 to 0x20016000.
             *  Each segment with a length of 0x2000 (byte addressing).
             */
            var writeSeg = (IpcMgr.writeAddr - 0x20008000) >> 13;

            /* The M3 must be owner of writeAddr shared memory segment */
            if (IpcMgr.sharedMemoryOwnerMask & (1 << writeSeg)) {
                IpcMgr.$logError("IpcMgr.writeAddr is set to address: " +
                utils.toHex(IpcMgr.writeAddr) + "," +
                " but IpcMgr.sharedMemoryOwnerMask bit: " + writeSeg +
                " must not be set. Unset this bit to make M3 the owner.", IpcMgr);
            }
        }

        if ((IpcMgr.readAddr >= 0x20008000) &&
            (IpcMgr.readAddr < 0x20018000)) {
            /*
             *  Determine segment being used for the M3 readAddr.
             *  The shared RAM base address starts 0x20008000 to 0x20016000.
             *  Each segment with a length of 0x2000 (byte addressing).
             */
            var readSeg = (IpcMgr.readAddr - 0x20008000) >> 13;

            /* The C28 must be owner of readAddr shared memory segment */
            if (!(IpcMgr.sharedMemoryOwnerMask & (1 << readSeg))) {
                IpcMgr.$logError("IpcMgr.readAddr is set to address: " +
                utils.toHex(IpcMgr.readAddr) + "," +
                " but IpcMgr.sharedMemoryOwnerMask bit: " + readSeg +
                " is not set. This bit must be set so C28 is the owner.", IpcMgr);
            }
        }
    }
}

/*
 *  ======== module$validate ========
 */
function module$validate()
{
    if ((xdc.module('ti.sdo.ipc.Ipc').$used) ||
        (xdc.module('ti.sdo.ipc.GateMP').$used) ||
        (xdc.module('ti.sdo.ipc.SharedRegion').$used) ||
        (xdc.module('ti.sdo.ipc.ListMP').$used)) {
        IpcMgr.$logError("One or more of the following modules " +
            "[Ipc, GateMP, ListMP, SharedRegion] are being used " +
            "but are not supported on this device.", IpcMgr);
    }
}
