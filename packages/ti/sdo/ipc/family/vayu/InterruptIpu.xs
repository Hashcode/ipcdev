/*
 * Copyright (c) 2013, Texas Instruments Incorporated
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
 *  ======== InterruptIpu.xs ========
 *
 */

var BIOS    = null;
var Hwi     = null;
var Core    = null;
var Ipu     = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    BIOS            = xdc.useModule("ti.sysbios.BIOS");
    Hwi             = xdc.useModule("ti.sysbios.family.arm.m3.Hwi");
    Core            = xdc.useModule("ti.sysbios.family.arm.ducati.Core");
    Ipu             = xdc.useModule("ti.sdo.ipc.family.vayu.InterruptIpu");
    Xbar            = xdc.useModule("ti.sysbios.family.shared.vayu.IntXbar");
    TableInit       = xdc.useModule("ti.sdo.ipc.family.vayu.TableInit");

    /* Initisalize procIdTable */
    TableInit.initProcId(Ipu);

    /* Initialize mailboxTable */
    TableInit.generateTable(Ipu);

    /* Initialize mailbox base addrs */
    if (this.mailboxBaseAddr[0] == undefined) {
        this.mailboxBaseAddr[0]  = 0x6208B000;  /* EVE1 MBOX0 - PA: 0x4208B000 */
    }
    if (this.mailboxBaseAddr[1] == undefined) {
        this.mailboxBaseAddr[1]  = 0x6208C000;  /* EVE1 MBOX1 - PA: 0x4208C000 */
    }
    if (this.mailboxBaseAddr[2] == undefined) {
        this.mailboxBaseAddr[2]  = 0;           /* EVE1 MBOX2 - PA: N/A        */
    } else {
        this.$logWarning("InterruptIpu.mailboxBaseAddr[2] is the EVE1 MBOX2, " +
                "which is not used for IPU communication and should not be " +
                "configured.", this);
    }
    if (this.mailboxBaseAddr[3] == undefined) {
        this.mailboxBaseAddr[3]  = 0x6218B000;  /* EVE2 MBOX0 - PA: 0x4218B000 */
    }
    if (this.mailboxBaseAddr[4] == undefined) {
        this.mailboxBaseAddr[4]  = 0x6218C000;  /* EVE2 MBOX1 - PA: 0x4218C000 */
    }
    if (this.mailboxBaseAddr[5] == undefined) {
        this.mailboxBaseAddr[5]  = 0;           /* EVE2 MBOX2 - PA: N/A        */
    } else {
        this.$logWarning("InterruptIpu.mailboxBaseAddr[5] is the EVE2 MBOX2, " +
                "which is not used for IPU communication and should not be " +
                "configured.", this);
    }

    if (this.mailboxBaseAddr[6] == undefined) {
        this.mailboxBaseAddr[6]  = 0x6228B000;  /* EVE3 MBOX0 - PA: 0x4228B000 */
    }
    if (this.mailboxBaseAddr[7] == undefined) {
        this.mailboxBaseAddr[7]  = 0x6228C000;  /* EVE3 MBOX1 - PA: 0x4228C000 */
    }
    if (this.mailboxBaseAddr[8] == undefined) {
        this.mailboxBaseAddr[8]  = 0;           /* EVE3 MBOX2 - PA: N/A        */
    } else {
        this.$logWarning("InterruptIpu.mailboxBaseAddr[8] is the EVE3 MBOX2, " +
                "which is not used for IPU communication and should not be " +
                "configured.", this);
    }

    if (this.mailboxBaseAddr[9] == undefined) {
        this.mailboxBaseAddr[9]  = 0x6238B000;  /* EVE4 MBOX0 - PA: 0x4238B000 */
    }
    if (this.mailboxBaseAddr[10] == undefined) {
        this.mailboxBaseAddr[10]  = 0x6238C000; /* EVE4 MBOX1 - PA: 0x4238C000 */
    }
    if (this.mailboxBaseAddr[11] == undefined) {
        this.mailboxBaseAddr[11]  = 0;          /* EVE4 MBOX2 - PA: N/A        */
    } else {
        this.$logWarning("InterruptIpu.mailboxBaseAddr[11] is the EVE4 MBOX2, " +
                "which is not used for IPU communication and should not be " +
                "configured.", this);
    }

    if (this.mailboxBaseAddr[12] == undefined) {
        this.mailboxBaseAddr[12] = 0x68840000;  /* System Mailbox 5 */
    }
    if (this.mailboxBaseAddr[13] == undefined) {
        this.mailboxBaseAddr[13] = 0x68842000;  /* System Mailbox 6 */
    }
    if (this.mailboxBaseAddr[14] == undefined) {
        this.mailboxBaseAddr[14] = 0x68844000;  /* System Mailbox 7 */
    }
    if (this.mailboxBaseAddr[15] == undefined) {
        this.mailboxBaseAddr[15] = 0x68846000;  /* System Mailbox 8 */
    }

    /*
     * In case of a spec change, follow the process shown below:
     * 1. Update the mailboxBaseAddr Table.
     * 2. Update the dspInterruptTable.
     * 3. Update Virtual Index assignment.
     * 4. Update numCores, numEves and eveMbx2BaseIdx variables
     *    in order to correctly intialize the mailboxTable.
     */
}

/*
 *  ======== module$static$init ========
 *  Initialize module values.
 */
function module$static$init(mod, params)
{
    var remoteProcId;
    var mbxId;

    for (remoteProcId = 0; remoteProcId < Ipu.procIdTable.length; remoteProcId++) {
        mod.fxnTable[remoteProcId].func  = null;
        mod.fxnTable[remoteProcId].arg   = 0;
    }

    for (mbxId = 0; mbxId < Ipu.mailboxBaseAddr.length; mbxId++) {
        mod.numPlugged[mbxId] = 0;
    }

    /* Initialize Interrupt Event Ids for communicating with this processor */
    if (Core.id == 0) {
        mod.interruptTable[0] = 64; /* EVE1 */
        mod.interruptTable[1] = 65; /* EVE2 */
        mod.interruptTable[2] = 67; /* EVE3 */
        mod.interruptTable[3] = 68; /* EVE4 */

        /* These are not known at config time and is set a runtime */
        mod.interruptTable[4] = 0; /* DSP1 */
        mod.interruptTable[5] = 0; /* DSP2 */
        mod.interruptTable[6] = 0; /* Ipu1-0 */
        mod.interruptTable[7] = 0; /* Ipu2-0 */
        mod.interruptTable[8] = 0; /* HOST */
        mod.interruptTable[9] = 0; /* Ipu1-1 */
        mod.interruptTable[10] = 0; /* Ipu2-1 */
    }
    else {
        mod.interruptTable[0] = 71; /* EVE1 */
        mod.interruptTable[1] = 72; /* EVE2 */
        mod.interruptTable[2] = 74; /* EVE3 */
        mod.interruptTable[3] = 75; /* EVE4 */

        /* These are not known at config time and is set a runtime */
        mod.interruptTable[4] = 0; /* DSP1 */
        mod.interruptTable[5] = 0; /* DSP2 */
        mod.interruptTable[6] = 0; /* Ipu1-0 */
        mod.interruptTable[7] = 0; /* Ipu2-0 */
        mod.interruptTable[8] = 0; /* HOST */
        mod.interruptTable[9] = 0; /* Ipu1-1 */
        mod.interruptTable[10] = 0; /* Ipu2-1 */

    }
}
