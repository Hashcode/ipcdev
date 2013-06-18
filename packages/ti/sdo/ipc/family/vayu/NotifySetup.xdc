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
 *  ======== NotifySetup.xdc ========
 *
 */

import ti.sdo.utils.MultiProc;

/*!
 *  ======== NotifySetup ========
 *  Notify setup proxy for Vayu
 *
 *  This module creates and registers all drivers necessary for inter-processor
 *  notification on Vayu.
 */

module NotifySetup inherits ti.sdo.ipc.interfaces.INotifySetup
{
    /* Total number of cores on Vayu SoC */
    const UInt8 NUM_CORES = 11;

    /*!
     *  Interrupt vector id for Vayu/DSP.
     */
    config UInt dspIntVectId = 4;

    /*!
     *  Interrupt vector id for Vayu/EVE
     */
    config UInt eveIntVectId_INTC0 = -1;
    config UInt eveIntVectId_INTC1 = -1;

    config UInt32 procIdTable[NUM_CORES];

internal:

    config UInt eve1ProcId      = MultiProc.INVALIDID;
    config UInt eve2ProcId      = MultiProc.INVALIDID;
    config UInt eve3ProcId      = MultiProc.INVALIDID;
    config UInt eve4ProcId      = MultiProc.INVALIDID;
    config UInt dsp1ProcId      = MultiProc.INVALIDID;
    config UInt dsp2ProcId      = MultiProc.INVALIDID;
    config UInt ipu1ProcId      = MultiProc.INVALIDID;
    config UInt ipu2ProcId      = MultiProc.INVALIDID;
    config UInt hostProcId      = MultiProc.INVALIDID;
}
