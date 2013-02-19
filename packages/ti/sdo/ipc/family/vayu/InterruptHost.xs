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
 *  ======== InterruptHost.xs ========
 *
 */

var Hwi         = null;
var Host        = null;
var Ipc         = null;
var Xbar        = null;
var Mmu         = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    Hwi              = xdc.useModule("ti.sysbios.family.arm.gic.Hwi");
    Ipc              = xdc.useModule("ti.sdo.ipc.Ipc");
    Host             = xdc.useModule("ti.sdo.ipc.family.vayu.InterruptHost");
    Xbar             = xdc.useModule("ti.sysbios.hal.vayu.IntXbar");
    Mmu              = xdc.useModule("ti.sysbios.family.arm.a15.Mmu");
    TableInit        = xdc.useModule("ti.sdo.ipc.family.vayu.TableInit");

    /* Initisalize procIdTable */
    TableInit.initProcId(Host);

    /* Initialize mailboxTable */
    TableInit.generateTable(Host);

    /* Initialize mailbox base address table */
    this.mailboxBaseAddr[0]  = 0x4208B000;  /* EVE1 Internal Mailbox 0 */
    this.mailboxBaseAddr[1]  = 0x4208C000;  /* EVE1 Internal Mailbox 1 */
    this.mailboxBaseAddr[2]  = 0x4208D000;  /* EVE1 Internal Mailbox 2 */
    this.mailboxBaseAddr[3]  = 0x4218B000;  /* EVE2 Internal Mailbox 0 */
    this.mailboxBaseAddr[4]  = 0x4218C000;  /* EVE2 Internal Mailbox 1 */
    this.mailboxBaseAddr[5]  = 0x4218D000;  /* EVE2 Internal Mailbox 2 */
    this.mailboxBaseAddr[6]  = 0x4228B000;  /* EVE3 Internal Mailbox 0 */
    this.mailboxBaseAddr[7]  = 0x4228C000;  /* EVE3 Internal Mailbox 1 */
    this.mailboxBaseAddr[8]  = 0x4228D000;  /* EVE3 Internal Mailbox 2 */
    this.mailboxBaseAddr[9]  = 0x4238B000;  /* EVE4 Internal Mailbox 0 */
    this.mailboxBaseAddr[10] = 0x4238C000;  /* EVE4 Internal Mailbox 1 */
    this.mailboxBaseAddr[11] = 0x4238D000;  /* EVE4 Internal Mailbox 2 */
    this.mailboxBaseAddr[12] = 0x48844000;  /* System Mailbox 7 */
    this.mailboxBaseAddr[13] = 0x48842000;  /* System Mailbox 6 */
    this.mailboxBaseAddr[14] = 0x48840000;  /* System Mailbox 5 */

    this.hostInterruptTable[0] = 79;        /* EVE1 */
    this.hostInterruptTable[1] = 80;        /* EVE2 */
    this.hostInterruptTable[2] = 81;        /* EVE3 */
    this.hostInterruptTable[3] = 82;        /* EVE4 */
    this.hostInterruptTable[4] = 83;        /* DSP1 */
    this.hostInterruptTable[5] = 83;        /* DSP2 */
    this.hostInterruptTable[6] = 84;        /* IPU1 */
    this.hostInterruptTable[7] = 84;        /* IPU2 */

    /*
     * In case of a spec change, follow the process shown below:
     * 1. Update the mailboxBaseAddr Table.
     * 2. Update the dspInterruptTable.
     * 3. Update Virtual Index assignment.
     * 4. Update numCores, numEves and eveMbx2BaseIdx variables
     *    in order to correctly intialize the mailboxTable.
     */

    /* Add mailbox addresses to the Mmu table */
    /* Force mailbox addresses to be NON cacheable */
    var peripheralAttrs = {
        type : Mmu.DescriptorType_BLOCK,  // BLOCK descriptor
        accPerm    : 0,                   // read/write at PL1
        noExecute  : true,                // not executable
        attrIndx   : 1                    // MAIR0 Byte1 describes mem attr
    };

    /* Configure the corresponding MMU page descriptor accordingly */
    Mmu.setSecondLevelDescMeta(0x42000000,
                               0x42000000,
                               peripheralAttrs);

    Mmu.setSecondLevelDescMeta(0x42200000,
                               0x42200000,
                               peripheralAttrs);

    Mmu.setSecondLevelDescMeta(0x48800000,
                               0x48800000,
                               peripheralAttrs);
}

/*
 *  ======== module$static$init ========
 *  Initialize module values.
 */
function module$static$init(mod, params)
{
    var remoteProcId;
    var mbxId;

    for (remoteProcId = 0; remoteProcId < Host.procIdTable.length; remoteProcId++) {
        mod.fxnTable[remoteProcId].func  = null;
        mod.fxnTable[remoteProcId].arg   = 0;
    }

    for (mbxId = 0; mbxId < Host.mailboxBaseAddr.length; mbxId++) {
        mod.numPlugged[mbxId] = 0;
    }
}
