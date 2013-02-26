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
 *  ======== InterruptArp32.xs ========
 *
 */

var Hwi         = null;
var Core        = null;
var MultiProc   = null;
var InterruptArp32       = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    Hwi              = xdc.useModule("ti.sysbios.family.arp32.Hwi");
    MultiProc        = xdc.useModule("ti.sdo.utils.MultiProc");
    Ipc              = xdc.useModule("ti.sdo.ipc.Ipc");
    InterruptArp32   = xdc.useModule("ti.sdo.ipc.family.vayu.InterruptArp32");
    TableInit        = xdc.useModule("ti.sdo.ipc.family.vayu.TableInit");

    /* Initialize procIdTable */
    TableInit.initProcId(InterruptArp32);

    /* Initialize mailboxTable */
    TableInit.generateTable(InterruptArp32);

    /* Initialize mailbox base address table */
    this.mailboxBaseAddr[0]  = 0x4008B000;
    this.mailboxBaseAddr[1]  = 0x4008C000;
    this.mailboxBaseAddr[2]  = 0x4008D000;
    this.mailboxBaseAddr[3]  = 0x4008B000;
    this.mailboxBaseAddr[4]  = 0x4008C000;
    this.mailboxBaseAddr[5]  = 0x4218D000;
    this.mailboxBaseAddr[6]  = 0x4008B000;
    this.mailboxBaseAddr[7]  = 0x4008C000;
    this.mailboxBaseAddr[8]  = 0x4228D000;
    this.mailboxBaseAddr[9]  = 0x4008B000;
    this.mailboxBaseAddr[10] = 0x4008C000;
    this.mailboxBaseAddr[11] = 0x4238D000;
    this.mailboxBaseAddr[12] = 0x48844000;
    this.mailboxBaseAddr[13] = 0x48842000;
    this.mailboxBaseAddr[14] = 0x48840000;

    if (MultiProc.id == this.eve2ProcId) {
        this.mailboxBaseAddr[2] = 0x4208D000;
        this.mailboxBaseAddr[5] = 0x4008D000;
    }
    else if (MultiProc.id == this.eve3ProcId) {
        this.mailboxBaseAddr[2]  = 0x4208D000;
        this.mailboxBaseAddr[8] = 0x4008D000;
    }
    else if (MultiProc.id == this.eve4ProcId) {
        this.mailboxBaseAddr[2]  = 0x4208D000;
        this.mailboxBaseAddr[11] = 0x4008D000;
    }

    this.eveInterruptTable[0] = 60; /* EVE1 - Group1/INTC1 */
    this.eveInterruptTable[1] = 60; /* EVE2 - Group1/INTC1 */
    this.eveInterruptTable[2] = 60; /* EVE3 - Group1/INTC1 */
    this.eveInterruptTable[3] = 60; /* EVE4 - Group1/INTC1 */
    this.eveInterruptTable[4] = 29; /* DSP1 - Group0/INTC0 */
    this.eveInterruptTable[5] = 30; /* DSP2 - Group0/INTC0 */
    this.eveInterruptTable[6] = 29; /* IPU1 */
    this.eveInterruptTable[7] = 30; /* IPU2 */
    this.eveInterruptTable[8] = 29; /* HOST */

    /*
     * In case of a spec change, follow the process shown below:
     * 1. Update the mailboxBaseAddr Table.
     * 2. Update the dspInterruptTable.
     * 3. Update Virtual Index assignment.
     * 4. Update NUMCORES, NUMEVES and EVEMBX2BASEIDX variables
     *    in order to correctly intialize the mailboxTable.
     */
}

/*
 *  ======== module$static$init ========
 *  Initialize module values.
 */
function module$static$init(mod, params)
{
    var idx;
    var remoteProcId;

    for (remoteProcId=0; remoteProcId < InterruptArp32.procIdTable.length; remoteProcId++) {
        mod.fxnTable[remoteProcId].func  = null;
        mod.fxnTable[remoteProcId].arg   = 0;
    }

    for (idx = 0; idx < (InterruptArp32.NUM_EVE_MBX/InterruptArp32.NUM_EVES); idx++) {
        mod.numPlugged[idx] = 0;
    }
}
