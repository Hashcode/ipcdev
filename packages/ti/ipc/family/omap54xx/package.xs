/*
 * Copyright (c) 2011-2013, Texas Instruments Incorporated
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
 *  ======== package.xs ========
 *
 */

/*
 *  ======== close ========
 */
function close()
{
    if (xdc.om.$name != "cfg") {
        return;
    }

    /* bring in modules we use in this package */
    xdc.useModule('ti.ipc.remoteproc.Resource');
    xdc.loadPackage('ti.pm');
    xdc.useModule('ti.sysbios.gates.GateAll');
    xdc.useModule('ti.sysbios.hal.Cache');
    xdc.useModule('ti.sysbios.knl.Semaphore');
    xdc.useModule('ti.sysbios.knl.Swi');
}

/*
 *  ======== getLibs ========
 */
function getLibs(prog)
{
    var file;
    var libAry = [];
    var profile = this.profile;
    var smp = "";

    var suffix = prog.build.target.findSuffix(this);
    if (suffix == null) {
        /* no matching lib found in this package, return "" */
        $trace("Unable to locate a compatible library, returning none.",
                1, ['getLibs']);
        return ("");
    }

    var BIOS = xdc.module('ti.sysbios.BIOS');
    if (BIOS.smpEnabled) {
        smp = "_smp";
    }

    /* make sure the library exists, else fallback to a built library */
    file = "lib/" + profile + "/ti.ipc.family.omap54xx" + smp + ".a" + suffix;
    if (java.io.File(this.packageBase + file).exists()) {
        libAry.push(file);
    }
    else {
        file = "lib/release/ti.ipc.family.omap54xx" + smp + ".a" + suffix;
        if (java.io.File(this.packageBase + file).exists()) {
            libAry.push(file);
        }
        else {
            /* fallback to a compatible library built by this package */
            for (var p in this.build.libDesc) {
                if (suffix == this.build.libDesc[p].suffix) {
                    libAry.push(p);
                    break;
                }
            }
        }
    }

    return libAry.join(";");
}

/*
 *  ======== validate ========
 */
function validate()
{
    var BIOS = xdc.module('ti.sysbios.BIOS');
    var suffix = prog.build.target.findSuffix(this);

    if (!BIOS.smpEnabled && (suffix != "e64T")) {
        throw new Error(Pkg.$name+" must have BIOS.smpEnabled set to true.");
    }
}
