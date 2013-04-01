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
    Program.exportModule('ti.sysbios.hal.Cache');
    Program.exportModule('ti.sysbios.knl.Idle');

    var device = Program.cpu.deviceName;

    switch (device) {
        case "OMAP5430": /* OMAP5 */
            xdc.loadPackage('ti.ipc.family.omap54xx');
            break;

        case "OMAPL138":
            xdc.useModule('ti.ipc.family.omapl138.VirtQueue');
            break;

        case "TMS320TCI6614":
            xdc.useModule('ti.ipc.family.tci6614.VirtQueue');
            break;

        case "Kepler":
        case "TMS320TCI6638":
            xdc.useModule('ti.ipc.family.tci6638.VirtQueue');
            break;

        case "Vayu": /* Vayu */
            xdc.loadPackage('ti.ipc.family.vayu');
            break;

        default:
            throw new Error("Unspported device: " + device);
            break;
    }

    xdc.useModule('xdc.runtime.Assert');
    xdc.useModule('xdc.runtime.Diags');
    xdc.useModule('xdc.runtime.Log');
    xdc.useModule('xdc.runtime.Memory');
    xdc.useModule('xdc.runtime.Registry');
    xdc.useModule('xdc.runtime.System');

    xdc.useModule('ti.sysbios.BIOS');
    xdc.useModule('ti.sysbios.gates.GateSwi');
    xdc.useModule('ti.sysbios.heaps.HeapBuf');
    xdc.useModule('ti.sysbios.knl.Semaphore');
    xdc.useModule('ti.sysbios.knl.Swi');
}

/*
 *  ======== getLibs ========
 */
function getLibs(prog)
{
    var device = prog.cpu.deviceName;
    var platform = "";
    var smp = "";

    var suffix = prog.build.target.findSuffix(this);
    if (suffix == null) {
        /* no matching lib found in this package, return "" */
        $trace("Unable to locate a compatible library, returning none.",
                1, ['getLibs']);
        return ("");
    }

    var BIOS = xdc.module('ti.sysbios.BIOS');
    if (BIOS.smpEnabled == true) {
        smp = "_smp";
    }


    switch (device) {
        case "OMAP4430":
        case "OMAP5430":
            /* OMAP5 */
            platform = "omap5";
            break;

        case "OMAPL138":
            platform = "omapl138";
            break;

        case "Kepler":
        case "TMS320TCI6638":
            platform = "tci6638";
            break;

        case "Vayu":
            platform = "vayu";
            break;

        default:
            throw ("Unspported device: " + device);
            break;
    }

    /* the location of the libraries are in lib/<profile>/* */
    var name = this.$name + "_" + platform + smp + ".a" + suffix;
    var lib = "lib/" + this.profile + "/" + name;


    /*
     * If the requested profile doesn't exist, we return the 'release' library.
     */
    if (!java.io.File(this.packageBase + lib).exists()) {
        $trace("Unable to locate lib for requested '" + this.profile +
                "' profile.  Using 'release' profile.", 1, ['getLibs']);
        lib = "lib/release/" + name;
    }

    return (lib);
}
