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
 *  ======== TableInit.xs ========
 *
 */

/*
 * When assigning virtual indexes to each core make sure
 * to assign even virtual indexes to DSP/M4 cores with
 * even Core Ids, and assign odd virtual indexes to DSP/M4
 * cores with odd Core Ids.
 *
 * Example:
 *     DSP physical Core Id = 0 -> Virtual Index = 4;
 *     DSP physical Core Id = 1 -> Virtual Index = 5;
 *
 * Virtual Index Assignment:
 *
 * | EVE1 -> 0 | EVE2 -> 1 | EVE3 -> 2 | EVE4 -> 3 |
 * | DSP1 -> 4 | DSP2 -> 5 | IPU1 -> 6 | IPU2 -> 7 |
 * | HOST -> 8
 *
 */
var eve1VirtId    = 0;
var eve2VirtId    = 1;
var eve3VirtId    = 2;
var eve4VirtId    = 3;
var dsp1VirtId    = 4;
var dsp2VirtId    = 5;
var ipu1VirtId    = 6;
var ipu2VirtId    = 7;
var hostVirtId    = 8;

/*
 * Function to initialize coreIds.
 */
function initProcId(InterruptCore)
{
    var MultiProc        = xdc.useModule("ti.sdo.utils.MultiProc");

    for (var loopIdx=0 ;loopIdx<InterruptCore.procIdTable.length ;loopIdx++) {
        InterruptCore.procIdTable[loopIdx] = -1;
    }

    InterruptCore.eve1ProcId     = MultiProc.getIdMeta("EVE1");
    InterruptCore.eve2ProcId     = MultiProc.getIdMeta("EVE2");
    InterruptCore.eve3ProcId     = MultiProc.getIdMeta("EVE3");
    InterruptCore.eve4ProcId     = MultiProc.getIdMeta("EVE4");
    InterruptCore.dsp1ProcId     = MultiProc.getIdMeta("DSP1");
    InterruptCore.dsp2ProcId     = MultiProc.getIdMeta("DSP2");
    InterruptCore.ipu1ProcId     = MultiProc.getIdMeta("IPU1");
    InterruptCore.ipu2ProcId     = MultiProc.getIdMeta("IPU2");
    InterruptCore.hostProcId     = MultiProc.getIdMeta("HOST");

    if (InterruptCore.eve1ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.eve1ProcId] = eve1VirtId;
    }
    if (InterruptCore.eve2ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.eve2ProcId] = eve2VirtId;
    }
    if (InterruptCore.eve3ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.eve3ProcId] = eve3VirtId;
    }
    if (InterruptCore.eve4ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.eve4ProcId] = eve4VirtId;
    }
    if (InterruptCore.dsp1ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.dsp1ProcId] = dsp1VirtId;
    }
    if (InterruptCore.dsp2ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.dsp2ProcId] = dsp2VirtId;
    }
    if (InterruptCore.ipu1ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.ipu1ProcId] = ipu1VirtId;
    }
    if (InterruptCore.ipu2ProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.ipu2ProcId] = ipu2VirtId;
    }
    if (InterruptCore.hostProcId != MultiProc.INVALIDID) {
        InterruptCore.procIdTable[InterruptCore.hostProcId] = hostVirtId;
    }
}

/*
 * Function to generate mailbox table
 */
function generateTable(InterruptCore)
{
    var SYS_MBX5_OFFSET = 2;
    var SYS_MBX6_OFFSET = 1;
    var SYS_MBX7_OFFSET = 0;
    var eveMbx2BaseIdx = 2;

    var subMbxIdx;
    var tableEntry;
    var mbxUserIdx;
    var mbxBaseAddrIdx;

    /*
     * Each entry in the mailbox table stores 3 indexes.
     * The breakup of each entry is shown below:
     * Entry format : 0xAAAABBCC
     *         AAAA : Mailbox base address table index
     *           BB : Mailbox User Id
     *           CC : Sub-mailbox index
     *
     * In order to lookup the User Id, Sub-mailbox Index and mailbox base
     * address for a given src and dst core from the mailboxTable, the
     * procedure shown below is followed:
     *     1. Find the right entry for the given src and dst core.
     *        mailboxTable index is given by:
     *            Index = (src * NumCores) + dst
     *     2. Mbx BaseAddr Index = mailboxTable[Index] >> 16
     *     2. dst Mailbox UserId = (mailboxTable[Index] >> 8) & 0xFF
     *     3. Sub-Mailbox Index  = mailboxTable[Index] & 0xFF
     */

    /*
     * 'i' is src core index, and
     * 'j' is dst core index
     */
    for (var i = 0; i < InterruptCore.NUM_CORES; i++) {
        for (var j = 0; j < InterruptCore.NUM_CORES; j++) {
            /* EVE Internal Mailbox 2 */
            if ((i < InterruptCore.NUM_EVES) && (j < InterruptCore.NUM_EVES)) {

                /* Generate 3 masks forming a single table entry */
                mbxBaseAddrIdx = ((j * 3) + eveMbx2BaseIdx) << 16;

                /*
                 * Determined based on the send remote receive local
                 * methodology being followed for EVE-to-EVE communication.
                 */
                mbxUserIdx = 0 << 8;
                subMbxIdx = i;

                tableEntry = mbxBaseAddrIdx | mbxUserIdx | subMbxIdx;
                InterruptCore.mailboxTable[i*InterruptCore.NUM_CORES + j] = tableEntry;
                continue;
            }

            /* EVE Internal Mailbox 0/1 */
            if ((i < InterruptCore.NUM_EVES) || (j < InterruptCore.NUM_EVES)) {
                if (i < InterruptCore.NUM_EVES) {
                    mbxBaseAddrIdx = ((i * 3) + (j % 2)) << 16;

                    if ((j == dsp1VirtId) || (j == dsp2VirtId)) {
                        /* Destination is DSP1 or DSP2 */
                        mbxUserIdx = 1 << 8;
                        subMbxIdx = 0;
                    }
                    else if ((j == ipu1VirtId) || (j == ipu2VirtId)) {
                        /* Destination is IPU1 or IPU2 */
                        mbxUserIdx = 2 << 8;
                        subMbxIdx = 2;
                    }
                    else {
                        /* Destination is Host */
                        mbxUserIdx = 3 << 8;
                        subMbxIdx = 4;
                    }

                    tableEntry = mbxBaseAddrIdx | mbxUserIdx | subMbxIdx;
                    InterruptCore.mailboxTable[i*InterruptCore.NUM_CORES + j] = tableEntry;
                    continue;
                }
                else if (j < InterruptCore.NUM_EVES) {
                    mbxBaseAddrIdx = ((j * 3) + (i % 2)) << 16;
                    mbxUserIdx = 0 << 8; /* destination is always EVE */

                    if ((i == dsp1VirtId) || (i == dsp2VirtId)) {
                        /* Source is DSP1 or DSP2 */
                        subMbxIdx = 1;
                    }
                    else if ((i == ipu1VirtId) || (i == ipu2VirtId)) {
                        /* Source is IPU1 or IPU2 */
                        subMbxIdx = 3;
                    }
                    else {
                        /* Source is Host */
                        subMbxIdx = 5;
                    }

                    tableEntry = mbxBaseAddrIdx | mbxUserIdx | subMbxIdx;
                    InterruptCore.mailboxTable[i*InterruptCore.NUM_CORES + j] = tableEntry;
                    continue;
                }
            }

            /* System Mailbox 5 */
            /* For communication between HOST and IPU1/2 and inter IPU */
            if (((i == ipu1VirtId) || (i == ipu2VirtId) || (i == hostVirtId)) &&
                ((j == ipu1VirtId) || (j == ipu2VirtId) || (j == hostVirtId))) {
                mbxBaseAddrIdx = ((InterruptCore.NUM_EVES * 3) +
                                  SYS_MBX5_OFFSET) << 16;
                if (j == ipu1VirtId) {
                    mbxUserIdx = 0;
                    if (i == ipu2VirtId) {
                        subMbxIdx = 2;
                    }
                    else if (i == hostVirtId) {
                        subMbxIdx = 4;
                    }
                }
                else if (j == ipu2VirtId) {
                    mbxUserIdx = 1 << 8;
                    if (i == ipu1VirtId) {
                        subMbxIdx = 0;
                    }
                    else if (i == hostVirtId) {
                        subMbxIdx = 5;
                    }
                }
                else {
                    mbxUserIdx = 2 << 8;
                    if (i == ipu1VirtId) {
                        subMbxIdx = 1;
                    }
                    else if (i == ipu2VirtId) {
                        subMbxIdx = 3;
                    }
                }

                tableEntry = mbxBaseAddrIdx | mbxUserIdx | subMbxIdx;
                InterruptCore.mailboxTable[i*InterruptCore.NUM_CORES + j] = tableEntry;
                continue;
            }

            /* System Mailbox 6 */
            /* For communication between HOST and DSP1/2 and inter DSP */
            if (((i == dsp1VirtId) || (i == dsp2VirtId) || (i == hostVirtId)) &&
                ((j == dsp1VirtId) || (j == dsp2VirtId) || (j == hostVirtId))) {
                mbxBaseAddrIdx = ((InterruptCore.NUM_EVES * 3) +
                                   SYS_MBX6_OFFSET) << 16;

                if (j == dsp1VirtId) {
                    mbxUserIdx = 0;
                    if (i == dsp2VirtId) {
                        subMbxIdx = 2;
                    }
                    else if (i == hostVirtId) {
                        subMbxIdx = 4;
                    }
                }
                else if (j == dsp2VirtId) {
                    mbxUserIdx = 1 << 8;
                    if (i == dsp1VirtId) {
                        subMbxIdx = 0;
                    }
                    else if (i == hostVirtId) {
                        subMbxIdx = 5;
                    }
                }
                else {
                    mbxUserIdx = 2 << 8;
                    if (i == dsp1VirtId) {
                        subMbxIdx = 1;
                    }
                    else if (i == dsp2VirtId) {
                        subMbxIdx = 3;
                    }
                }

                tableEntry = mbxBaseAddrIdx | mbxUserIdx | subMbxIdx;
                InterruptCore.mailboxTable[i*InterruptCore.NUM_CORES + j] = tableEntry;
                continue;
            }

            /* System Mailbox 7 */
            /* For communication between DSP1/2 and IPU1/2 */
            /* Not to be used for Inter DSP or Inter IPU communication */
            if (((i >= dsp1VirtId) && (i <= ipu2VirtId)) &&
                ((j >= dsp1VirtId) && (j <= ipu2VirtId))) {

                mbxBaseAddrIdx = ((InterruptCore.NUM_EVES * 3) +
                                  SYS_MBX7_OFFSET) << 16;
                mbxUserIdx = (j - InterruptCore.NUM_EVES) << 8;

                /* skip all ther inter IPU and DSP communication */
                if (i == j) {
                    continue;
                }
                if ((i == ipu1VirtId && j == ipu2VirtId) ||
                    (i == ipu2VirtId && j == ipu1VirtId)) {
                    continue;
                }
                if ((i == dsp1VirtId && j == dsp2VirtId) ||
                    (i == dsp2VirtId && j == dsp1VirtId)) {
                    continue;
                }

                if (i == dsp1VirtId) {
                    subMbxIdx = j - 6;  /* j = ipu1 or ipu2 */
                }
                else if (i == dsp2VirtId) {
                    subMbxIdx = j - 4;  /* j = ipu1 or ipu2 */
                }
                else if (i == ipu1VirtId) {
                    subMbxIdx = j;      /* j = dsp1 or dsp2 */
                }
                else if (i == ipu2VirtId) {
                    subMbxIdx = j + 2;  /* j = dsp1 or dsp2 */
                }

                tableEntry = mbxBaseAddrIdx | mbxUserIdx | subMbxIdx;
                InterruptCore.mailboxTable[i*InterruptCore.NUM_CORES + j] = tableEntry;
            }
        }
    }
}
