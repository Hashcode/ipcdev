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

var Hwi         = null;
var Core        = null;
var Ipu     = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    Hwi              = xdc.useModule("ti.sysbios.family.arm.m3.Hwi");
    Core             = xdc.useModule("ti.sysbios.family.arm.ducati.Core");
    Ipc              = xdc.useModule("ti.sdo.ipc.Ipc");
    Ipu              = xdc.useModule("ti.sdo.ipc.family.vayu.InterruptIpu");
    Xbar             = xdc.useModule("ti.sysbios.hal.vayu.IntXbar");
    TableInit        = xdc.useModule("ti.sdo.ipc.family.vayu.TableInit");

    /* Initisalize procIdTable */
    TableInit.initProcId(Ipu);

    /* Initialize mailboxTable */
    TableInit.generateTable(Ipu);

    /* Initialize mailbox base address table */
    this.mailboxBaseAddr[0]  = 0x9208B000;
    this.mailboxBaseAddr[1]  = 0x9208C000;
    this.mailboxBaseAddr[2]  = 0x9208D000;
    this.mailboxBaseAddr[3]  = 0x9218B000;
    this.mailboxBaseAddr[4]  = 0x9218C000;
    this.mailboxBaseAddr[5]  = 0x9218D000;
    this.mailboxBaseAddr[6]  = 0x9228B000;
    this.mailboxBaseAddr[7]  = 0x9228C000;
    this.mailboxBaseAddr[8]  = 0x9228D000;
    this.mailboxBaseAddr[9]  = 0x9238B000;
    this.mailboxBaseAddr[10] = 0x9238C000;
    this.mailboxBaseAddr[11] = 0x9238D000;
    this.mailboxBaseAddr[12] = 0x48844000;
    this.mailboxBaseAddr[13] = 0x48842000;
    this.mailboxBaseAddr[14] = 0x48840000;

    this.IpuInterruptTable[0] = 64; /* EVE1 */
    this.IpuInterruptTable[1] = 65; /* EVE2 */
    this.IpuInterruptTable[2] = 66; /* EVE3 */
    this.IpuInterruptTable[3] = 67; /* EVE4 */
    this.IpuInterruptTable[4] = 68; /* DSP1 */
    this.IpuInterruptTable[5] = 68; /* DSP2 */
    this.IpuInterruptTable[6] = 69; /* Ipu1 */
    this.IpuInterruptTable[7] = 69; /* Ipu2 */
    this.IpuInterruptTable[8] = 69; /* HOST */

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
}
