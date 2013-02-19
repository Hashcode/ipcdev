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
 *  ======== NotifySetup.xs ========
 *
 */

var NotifyDriverShm = null;
var Notify          = null;
var MultiProc       = null;
var NotifySetup     = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    NotifyDriverShm = xdc.useModule('ti.sdo.ipc.notifyDrivers.NotifyDriverShm');
    Notify          = xdc.useModule('ti.sdo.ipc.Notify');
    MultiProc       = xdc.useModule('ti.sdo.utils.MultiProc');
    NotifySetup     = this;

    var loopIdx;

    for (loopIdx = 0; loopIdx < this.NUM_CORES; loopIdx++) {
        this.procIdTable[loopIdx] = -1;
    }

    this.eve1ProcId     = MultiProc.getIdMeta("EVE1");
    this.eve2ProcId     = MultiProc.getIdMeta("EVE2");
    this.eve3ProcId     = MultiProc.getIdMeta("EVE3");
    this.eve4ProcId     = MultiProc.getIdMeta("EVE4");
    this.dsp1ProcId     = MultiProc.getIdMeta("DSP1");
    this.dsp2ProcId     = MultiProc.getIdMeta("DSP2");
    this.ipu1ProcId     = MultiProc.getIdMeta("IPU1");
    this.ipu2ProcId     = MultiProc.getIdMeta("IPU2");
    this.hostProcId     = MultiProc.getIdMeta("HOST");

    if (this.eve1ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.eve1ProcId] = 0;
    }
    if (this.eve2ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.eve2ProcId] = 1;
    }
    if (this.eve3ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.eve3ProcId] = 2;
    }
    if (this.eve4ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.eve4ProcId] = 3;
    }
    if (this.dsp1ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.dsp1ProcId] = 4;
    }
    if (this.dsp2ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.dsp2ProcId] = 5;
    }
    if (this.ipu1ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.ipu1ProcId] = 6;
    }
    if (this.ipu2ProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.ipu2ProcId] = 7;
    }
    if (this.hostProcId != MultiProc.INVALIDID) {
        this.procIdTable[this.hostProcId] = 8;
    }
}
