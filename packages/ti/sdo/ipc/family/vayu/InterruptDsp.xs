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
 *  ======== InterruptDsp.xs ========
 */

var Hwi             = null;
var InterruptDsp    = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    Hwi              = xdc.useModule("ti.sysbios.family.c64p.Hwi");
    EventCombiner    = xdc.useModule("ti.sysbios.family.c64p.EventCombiner");
    Ipc              = xdc.useModule("ti.sdo.ipc.Ipc");
    InterruptDsp     = xdc.useModule("ti.sdo.ipc.family.vayu.InterruptDsp");
    Xbar             = xdc.useModule("ti.sysbios.hal.vayu.IntXbar");
    TableInit        = xdc.useModule("ti.sdo.ipc.family.vayu.TableInit");

    /* Initialize procIdTable */
    TableInit.initProcId(InterruptDsp);

    /* Initialize mailboxTable */
    TableInit.generateTable(InterruptDsp);

    /* Initialize mailbox base address table */
    this.mailboxBaseAddr[0]  = 0x4208B000;
    this.mailboxBaseAddr[1]  = 0x4208C000;
    this.mailboxBaseAddr[2]  = 0x4208D000;
    this.mailboxBaseAddr[3]  = 0x4218B000;
    this.mailboxBaseAddr[4]  = 0x4218C000;
    this.mailboxBaseAddr[5]  = 0x4218D000;
    this.mailboxBaseAddr[6]  = 0x4228B000;
    this.mailboxBaseAddr[7]  = 0x4228C000;
    this.mailboxBaseAddr[8]  = 0x4228D000;
    this.mailboxBaseAddr[9]  = 0x4238B000;
    this.mailboxBaseAddr[10] = 0x4238C000;
    this.mailboxBaseAddr[11] = 0x4238D000;
    this.mailboxBaseAddr[12] = 0x48844000;
    this.mailboxBaseAddr[13] = 0x48842000;
    this.mailboxBaseAddr[14] = 0x48840000;

    /* Initialize Dsp Interrupt Id Table */
    this.dspInterruptTable[0] = 64; /* EVE1 */
    this.dspInterruptTable[1] = 65; /* EVE2 */
    this.dspInterruptTable[2] = 66; /* EVE3 */
    this.dspInterruptTable[3] = 67; /* EVE4 */
    this.dspInterruptTable[4] = 69; /* DSP1 */
    this.dspInterruptTable[5] = 69; /* DSP2 */
    this.dspInterruptTable[6] = 68; /* IPU1 */
    this.dspInterruptTable[7] = 68; /* IPU2 */
    this.dspInterruptTable[8] = 69; /* HOST */

    /*
     * In case of a spec change, follow the process shown below:
     * 1. Update the mailboxBaseAddr Table.
     * 2. Update the dspInterruptTable.
     * 3. Update Virtual Index assignment.
     * 4. Update NUMCORES, NUMEVES and EVEMBX2BASEIDX variables
     *    in order to correctly intialize the mailboxTable.
     */
}

function module$static$init(mod, params)
{
    var remoteProcId;
    var idx;

    for (remoteProcId = 0; remoteProcId < InterruptDsp.procIdTable.length; remoteProcId++) {
        mod.fxnTable[remoteProcId].func  = null;
        mod.fxnTable[remoteProcId].arg   = 0;
    }

    /* Intialize numPlugged */
    mod.numPlugged = 0;
}
