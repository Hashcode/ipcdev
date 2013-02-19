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
 *  ======== Build.xs ========
 */

var BIOS = null;
var Build = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    BIOS = xdc.module("ti.sysbios.BIOS");
    Build = this;

    /* inform getLibs() about location of library */
    switch (BIOS.libType) {
        case BIOS.LibType_Instrumented:
            this.$private.libraryName = "/ipc.a" + Program.build.target.suffix;
            this.$private.outputDir = this.$package.packageBase + "lib/"
                        + (BIOS.smpEnabled ? "smpipc/instrumented/" : "ipc/instrumented/");
            break;

        case BIOS.LibType_NonInstrumented:
            this.$private.libraryName = "/ipc.a" + Program.build.target.suffix;
            this.$private.outputDir = this.$package.packageBase + "lib/"
                        + (BIOS.smpEnabled ? "smpipc/nonInstrumented/" : "ipc/nonInstrumented/");
            break;

        case BIOS.LibType_Custom:
            this.$private.libraryName = "/ipc.a" + Program.build.target.suffix;
            var SourceDir = xdc.useModule("xdc.cfg.SourceDir");
            /* if building a pre-built library */
            if (BIOS.buildingAppLib == false) {
                var appName = Program.name.substring(0, Program.name.lastIndexOf('.'));
                this.$private.libDir = this.$package.packageBase + Build.libDir;
                if (!java.io.File(this.$private.libDir).exists()) {
                    java.io.File(this.$private.libDir).mkdir();
                }
            }
            /*
             * If building an application in CCS or package.bld world
             * and libDir has been specified
             */
            if ((BIOS.buildingAppLib == true) && (Build.libDir !== null)) {
                SourceDir.outputDir = Build.libDir;
                var src = SourceDir.create("ipc");
                src.libraryName = this.$private.libraryName.substring(1);
                this.$private.outputDir = src.getGenSourceDir();
            }
            else {
                var curPath = java.io.File(".").getCanonicalPath();
                /* If package.bld world AND building an application OR pre-built lib */
                if (java.io.File(curPath).getName() != "configPkg") {
                    var appName = Program.name.substring(0, Program.name.lastIndexOf('.'));
                    appName = appName + "_p" + Program.build.target.suffix + ".src";
                    SourceDir.outputDir = "package/cfg/" + appName;
                    SourceDir.toBuildDir = ".";
                    var src = SourceDir.create("ipc");
                    src.libraryName = this.$private.libraryName.substring(1);
                    this.$private.outputDir = src.getGenSourceDir();
                }
                /* Here ONLY if building an application in CCS world */
                else {
                    /* request output source directory for generated files */
                    var appName = Program.name.substring(0, Program.name.lastIndexOf('.'));
                    appName = appName + "_" + Program.name.substr(Program.name.lastIndexOf('.')+1);
                    SourceDir.toBuildDir = "..";
                    var src = SourceDir.create("ipc/");
                    src.libraryName = this.$private.libraryName.substring(1);

                    /* save this directory in our private state (to be read during
                    * generation, see Gen.xdt)
                    */
                    this.$private.outputDir = src.getGenSourceDir();
                }
            }
            break;
    }
}

/*
 * Add pre-built Instrumented and Non-Intrumented release libs
 */

var ipcSources  =  "ipc/GateMP.c " +
                   "ipc/ListMP.c " +
                   "ipc/SharedRegion.c " +
                   "ipc/MessageQ.c " +
                   "ipc/Notify.c ";

var gatesSources = "ipc/gates/GatePeterson.c " +
                   "ipc/gates/GatePetersonN.c " +
                   "ipc/gates/GateMPSupportNull.c ";

var heapsSources = "ipc/heaps/HeapBufMP.c " +
                   "ipc/heaps/HeapMemMP.c " +
                   "ipc/heaps/HeapMultiBufMP.c ";

var notifyDriverSources =
                   "ipc/notifyDrivers/NotifyDriverCirc.c " +
                   "ipc/notifyDrivers/NotifySetupNull.c " +
                   "ipc/notifyDrivers/NotifyDriverShm.c ";

var nsremoteSources =
                   "ipc/nsremote/NameServerRemoteNotify.c " +
                   "ipc/nsremote/NameServerMessageQ.c ";

var transportsSources =
                   "ipc/transports/TransportShm.c " +
                   "ipc/transports/TransportShmCircSetup.c " +
                   "ipc/transports/TransportShmNotifySetup.c " +
                   "ipc/transports/TransportShmCirc.c " +
                   "ipc/transports/TransportShmNotify.c " +
                   "ipc/transports/TransportShmSetup.c " +
                   "ipc/transports/TransportNullSetup.c " ;

var utilsSources = "utils/MultiProc.c " +
                   "utils/List.c " +
                   "utils/NameServerRemoteNull.c " +
                   "utils/NameServer.c ";

var commonSources = ipcSources +
                    gatesSources +
                    heapsSources +
                    notifyDriverSources +
                    nsremoteSources +
                    transportsSources +
                    utilsSources;

var C64PSources  =
                   "ipc/gates/GateAAMonitor.c " +
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/gates/GateHWSem.c " +
                   "ipc/family/dm6446/NotifySetup.c " +
                   "ipc/family/dm6446/NotifyCircSetup.c " +
                   "ipc/family/dm6446/InterruptDsp.c " +
                   "ipc/family/omap3530/NotifySetup.c " +
                   "ipc/family/omap3530/NotifyCircSetup.c " +
                   "ipc/family/omap3530/InterruptDsp.c ";

var C66Sources   = "ipc/gates/GateHWSem.c " +
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/tci663x/Interrupt.c " +
                   "ipc/family/tci663x/MultiProcSetup.c " +
                   "ipc/family/tci663x/NotifyCircSetup.c " +
                   "ipc/family/tci663x/NotifySetup.c " +
                   "ipc/family/vayu/InterruptDsp.c " +
                   "ipc/family/vayu/NotifySetup.c ";

var C674Sources  =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/da830/NotifySetup.c " +
                   "ipc/family/da830/NotifyCircSetup.c " +
                   "ipc/family/da830/InterruptDsp.c " +
                   "ipc/family/arctic/NotifySetup.c " +
                   "ipc/family/arctic/NotifyCircSetup.c " +
                   "ipc/family/arctic/InterruptDsp.c " +
                   "ipc/family/ti81xx/NotifySetup.c " +
                   "ipc/family/ti81xx/NotifyCircSetup.c " +
                   "ipc/family/ti81xx/InterruptDsp.c " +
                   "ipc/family/ti81xx/NotifyMbxSetup.c " +
                   "ipc/family/ti81xx/NotifyDriverMbx.c " +
                   "ipc/family/c6a8149/NotifySetup.c " +
                   "ipc/family/c6a8149/NotifyCircSetup.c " +
                   "ipc/family/c6a8149/InterruptDsp.c " +
                   "ipc/family/c6a8149/NotifyMbxSetup.c " +
                   "ipc/family/c6a8149/NotifyDriverMbx.c ";


var C647xSources = "ipc/family/c647x/Interrupt.c " +
                   "ipc/family/c647x/NotifyCircSetup.c " +
                   "ipc/family/c647x/MultiProcSetup.c " +
                   "ipc/family/c647x/NotifySetup.c ";

var C64TSources  =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/omap4430/NotifyCircSetup.c " +
                   "ipc/family/omap4430/NotifySetup.c " +
                   "ipc/family/omap4430/InterruptDsp.c ";

var C28Sources   = "ipc/family/f28m35x/NotifyDriverCirc.c " +
                   "ipc/family/f28m35x/IpcMgr.c " +
                   "ipc/family/f28m35x/TransportCirc.c " +
                   "ipc/family/f28m35x/NameServerBlock.c ";

var M3Sources    =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/omap4430/NotifySetup.c " +
                   "ipc/family/omap4430/NotifyCircSetup.c " +
                   "ipc/family/omap4430/InterruptDucati.c " +
                   "ipc/family/ti81xx/NotifySetup.c " +
                   "ipc/family/ti81xx/NotifyCircSetup.c " +
                   "ipc/family/ti81xx/InterruptDucati.c " +
                   "ipc/family/ti81xx/NotifyMbxSetup.c " +
                   "ipc/family/ti81xx/NotifyDriverMbx.c " +
                   "ipc/family/c6a8149/NotifySetup.c " +
                   "ipc/family/c6a8149/NotifyCircSetup.c " +
                   "ipc/family/c6a8149/InterruptDucati.c " +
                   "ipc/family/c6a8149/NotifyMbxSetup.c " +
                   "ipc/family/c6a8149/NotifyDriverMbx.c " +
                   "ipc/family/f28m35x/IpcMgr.c " +
                   "ipc/family/f28m35x/NotifyDriverCirc.c " +
                   "ipc/family/f28m35x/TransportCirc.c " +
                   "ipc/family/f28m35x/NameServerBlock.c " +
                   "ipc/family/vayu/InterruptIpu.c " +
                   "ipc/family/vayu/NotifySetup.c ";

var M4Sources    =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/vayu/InterruptIpu.c " +
                   "ipc/family/vayu/NotifySetup.c ";

var Arm9Sources  = "ipc/family/dm6446/NotifySetup.c " +
                   "ipc/family/dm6446/NotifyCircSetup.c " +
                   "ipc/family/dm6446/InterruptArm.c " +
                   "ipc/family/da830/NotifySetup.c " +
                   "ipc/family/da830/NotifyCircSetup.c " +
                   "ipc/family/da830/InterruptArm.c ";

var A8FSources   =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/ti81xx/NotifySetup.c " +
                   "ipc/family/ti81xx/NotifyCircSetup.c " +
                   "ipc/family/ti81xx/InterruptHost.c " +
                   "ipc/family/ti81xx/NotifyMbxSetup.c " +
                   "ipc/family/ti81xx/NotifyDriverMbx.c " +
                   "ipc/family/c6a8149/NotifySetup.c " +
                   "ipc/family/c6a8149/NotifyCircSetup.c " +
                   "ipc/family/c6a8149/InterruptHost.c " +
                   "ipc/family/c6a8149/NotifyMbxSetup.c " +
                   "ipc/family/c6a8149/NotifyDriverMbx.c " +
                   "ipc/family/omap3530/NotifySetup.c " +
                   "ipc/family/omap3530/NotifyCircSetup.c " +
                   "ipc/family/omap3530/InterruptHost.c ";

var A8gSources  =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/ti81xx/NotifySetup.c " +
                   "ipc/family/ti81xx/NotifyCircSetup.c " +
                   "ipc/family/ti81xx/InterruptHost.c " +
                   "ipc/family/ti81xx/NotifyMbxSetup.c " +
                   "ipc/family/ti81xx/NotifyDriverMbx.c " +
                   "ipc/family/c6a8149/NotifySetup.c " +
                   "ipc/family/c6a8149/NotifyCircSetup.c " +
                   "ipc/family/c6a8149/InterruptHost.c " +
                   "ipc/family/c6a8149/NotifyMbxSetup.c " +
                   "ipc/family/c6a8149/NotifyDriverMbx.c " +
                   "ipc/family/omap3530/NotifySetup.c " +
                   "ipc/family/omap3530/NotifyCircSetup.c " +
                   "ipc/family/omap3530/InterruptHost.c ";


var A15gSources  = "ipc/family/vayu/InterruptHost.c " +
                   "ipc/family/vayu/NotifySetup.c " +
                   "ipc/gates/GateHWSpinlock.c ";

var ARP32Sources   =
                   "ipc/gates/GateHWSpinlock.c " +
                   "ipc/family/arctic/NotifySetup.c " +
                   "ipc/family/arctic/NotifyCircSetup.c " +
                   "ipc/family/arctic/InterruptArp32.c " +
                   "ipc/family/c6a8149/NotifySetup.c " +
                   "ipc/family/c6a8149/NotifyCircSetup.c " +
                   "ipc/family/c6a8149/InterruptEve.c " +
                   "ipc/family/vayu/InterruptArp32.c " +
                   "ipc/family/vayu/NotifySetup.c ";

var cList = {
    "ti.targets.C28_large"              : commonSources + C28Sources,
    "ti.targets.C28_float"              : commonSources + C28Sources,

    "ti.targets.C64P"                   : commonSources + C647xSources + C64PSources,
    "ti.targets.C64P_big_endian"        : commonSources + C647xSources + C64PSources,
    "ti.targets.C674"                   : commonSources + C674Sources,

    "ti.targets.elf.C64P"               : commonSources + C647xSources + C64PSources,
    "ti.targets.elf.C64P_big_endian"    : commonSources + C647xSources + C64PSources,
    "ti.targets.elf.C674"               : commonSources + C674Sources,
    "ti.targets.elf.C64T"               : commonSources + C64TSources,
    "ti.targets.elf.C66"                : commonSources + C647xSources + C66Sources,
    "ti.targets.elf.C66_big_endian"     : commonSources + C647xSources + C66Sources,

    "ti.targets.arp32.elf.ARP32"        : commonSources + ARP32Sources,
    "ti.targets.arp32.elf.ARP32_far"    : commonSources + ARP32Sources,

    "ti.targets.arm.elf.Arm9"           : commonSources + Arm9Sources,
    "ti.targets.arm.elf.A8F"            : commonSources + A8FSources,
    "ti.targets.arm.elf.A8Fnv"          : commonSources + A8FSources,
    "ti.targets.arm.elf.M3"             : commonSources + M3Sources,
    "ti.targets.arm.elf.M4"             : commonSources + M4Sources,
    "ti.targets.arm.elf.M4F"            : commonSources + M4Sources,

    "gnu.targets.arm.A15F"              : commonSources + A15gSources,
    "gnu.targets.arm.A8F"               : commonSources + A8gSources,
    "gnu.targets.arm.M3"                : commonSources + M3Sources,
    "gnu.targets.arm.M4"                : commonSources + M4Sources,
    "gnu.targets.arm.M4F"               : commonSources + M4Sources,
};

var ipcPackages = [
    "ti.sdo.ipc",
    "ti.sdo.ipc.family.omap4430",
    "ti.sdo.ipc.family.omap3530",
    "ti.sdo.ipc.family.da830",
    "ti.sdo.ipc.family.dm6446",
    "ti.sdo.ipc.family.ti81xx",
    "ti.sdo.ipc.family.arctic",
    "ti.sdo.ipc.family.f28m35x",
    "ti.sdo.ipc.family.c647x",
    "ti.sdo.ipc.family.c6a8149",
    "ti.sdo.ipc.family.tci663x",
    "ti.sdo.ipc.family.vayu",
    "ti.sdo.ipc.gates",
    "ti.sdo.ipc.heaps",
    "ti.sdo.ipc.notifyDrivers",
    "ti.sdo.ipc.nsremote",
    "ti.sdo.ipc.transports",
    "ti.sdo.utils",
];

var asmListNone = [
];

var asmList64P = [
    "ipc/gates/GateAAMonitor_asm.s64P",
];

var asmList = {
    "ti.targets.C28_large"              : asmListNone,
    "ti.targets.C28_float"              : asmListNone,

    "ti.targets.C64P"                   : asmList64P,
    "ti.targets.C64P_big_endian"        : asmList64P,
    "ti.targets.C674"                   : asmList64P,

    "ti.targets.elf.C64P"               : asmList64P,
    "ti.targets.elf.C64P_big_endian"    : asmList64P,
    "ti.targets.elf.C674"               : asmList64P,

    "ti.targets.elf.C64T"               : asmListNone,
    "ti.targets.elf.C66"                : asmListNone,
    "ti.targets.elf.C66_big_endian"     : asmListNone,

    "ti.targets.arp32.elf.ARP32"        : asmListNone,
    "ti.targets.arp32.elf.ARP32_far"    : asmListNone,

    "ti.targets.arm.elf.Arm9"           : asmListNone,
    "ti.targets.arm.elf.A8F"            : asmListNone,
    "ti.targets.arm.elf.A8Fnv"          : asmListNone,
    "ti.targets.arm.elf.M3"             : asmListNone,
    "ti.targets.arm.elf.M4"             : asmListNone,
    "ti.targets.arm.elf.M4F"            : asmListNone,

    "gnu.targets.arm.M3"                : asmListNone,
    "gnu.targets.arm.M4"                : asmListNone,
    "gnu.targets.arm.M4F"               : asmListNone,
    "gnu.targets.arm.A8F"               : asmListNone,
    "gnu.targets.arm.A9F"               : asmListNone,
    "gnu.targets.arm.A15F"              : asmListNone,
};

var cFiles = {};

/*
 *  ======== getCFiles ========
 */
function getCFiles(target)
{
    var localSources = "ipc/Ipc.c ";

    /*
     * logic to trim the C files down to just what the application needs
     * 3/2/11 disabled for now ...
     */
    if (BIOS.buildingAppLib == true) {
        for each (var mod in Program.targetModules()) {
            var mn = mod.$name;
            var pn = mn.substring(0, mn.lastIndexOf("."));

            /* sanity check package path */
            var packageMatch = false;

            for (var i = 0; i < ipcPackages.length; i++) {
                if (pn == ipcPackages[i]) {
                    packageMatch = true;
                    break;
                }
            }

            if (packageMatch && !mn.match(/Proxy/) &&
               (mn != "ti.sdo.ipc.Ipc")) {
                if (cFiles[mn] === undefined) {
                    var prefix = mn.substr(mn.indexOf("sdo")+4);
                    var mod = mn.substr(mn.lastIndexOf(".")+1);
                    prefix = prefix.substring(0, prefix.lastIndexOf('.')+1);
                    prefix = prefix.replace(/\./g, "/");
                    localSources += prefix + mod + ".c ";
                }
                else {
                    for (i in cFiles[mn].cSources) {
                        var prefix = mn.substr(mn.indexOf("sdo")+8);
                        prefix = prefix.substring(0, prefix.lastIndexOf('.')+1);
                        prefix = prefix.replace(/\./g, "/");
                        localSources += prefix + cFiles[mn].cSources[i] + " ";
                    }
                }
            }
        }
    }
    else {
        localSources += cList[target];
    }

    /* remove trailing " " */
    localSources = localSources.substring(0, localSources.length-1);

    return (localSources);
}

/*
 *  ======== getAsmFiles ========
 */
function getAsmFiles(target)
{
    return (asmList[target]);
}

/*
 *  ======== getDefs ========
 */
function getDefs()
{
    var Defaults = xdc.module('xdc.runtime.Defaults');
    var Diags = xdc.module("xdc.runtime.Diags");
    var BIOS = xdc.module("ti.sysbios.BIOS");
    var MessageQ = xdc.module("ti.sdo.ipc.MessageQ");

    var defs = "";

    if ((BIOS.assertsEnabled == false) ||
        ((Defaults.common$.diags_ASSERT == Diags.ALWAYS_OFF)
            && (Defaults.common$.diags_INTERNAL == Diags.ALWAYS_OFF))) {
        defs += " -Dxdc_runtime_Assert_DISABLE_ALL";
    }

    if (BIOS.logsEnabled == false) {
        defs += " -Dxdc_runtime_Log_DISABLE_ALL";
    }

    defs += " -Dti_sdo_ipc_MessageQ_traceFlag__D=" + (MessageQ.traceFlag ? "TRUE" : "FALSE");

    var InterruptDucati = xdc.module("ti.sdo.ipc.family.ti81xx.InterruptDucati");

    /*
     * If we truely know which platform we're building against,
     * add these application specific -D's
     */
    if (BIOS.buildingAppLib == true) {
        defs += " -Dti_sdo_ipc_family_ti81xx_InterruptDucati_videoProcId__D=" + InterruptDucati.videoProcId;
        defs += " -Dti_sdo_ipc_family_ti81xx_InterruptDucati_hostProcId__D=" + InterruptDucati.hostProcId;
        defs += " -Dti_sdo_ipc_family_ti81xx_InterruptDucati_vpssProcId__D=" + InterruptDucati.vpssProcId;
        defs += " -Dti_sdo_ipc_family_ti81xx_InterruptDucati_dspProcId__D=" + InterruptDucati.dspProcId;
        defs += " -Dti_sdo_ipc_family_ti81xx_InterruptDucati_ducatiCtrlBaseAddr__D=" + InterruptDucati.ducatiCtrlBaseAddr;
        defs += " -Dti_sdo_ipc_family_ti81xx_InterruptDucati_mailboxBaseAddr__D=" + InterruptDucati.mailboxBaseAddr;
    }

    return (defs);
}

/*
 *  ======== getLibs ========
 */
function getLibs(pkg)
{
    var BIOS = xdc.module("ti.sysbios.BIOS");

    if (BIOS.libType != BIOS.LibType_Debug) {
        return null;
    }

    var lib = "";
    var name = pkg.$name + ".a" + prog.build.target.suffix;

    if (BIOS.smpEnabled == true) {
        lib = "lib/smpipc/debug/" + name;
    }
    else {
        lib = "lib/ipc/debug/" + name;
    }

    if (java.io.File(pkg.packageBase + lib).exists()) {
        return lib;
    }

    /* could not find any library, throw exception */
    throw Error("Library not found: " + name);
}


/*
 *  ======== getProfiles ========
 *  Determines which profiles to build for.
 *
 *  Any argument in XDCARGS which does not contain platform= is treated
 *  as a profile. This way multiple build profiles can be specified by
 *  separating them with a space.
 */
function getProfiles(xdcArgs)
{
    /*
     * cmdlProf[1] gets matched to "whole_program,debug" if
     * ["abc", "profile=whole_program,debug"] is passed in as xdcArgs
     */
    var cmdlProf = (" " + xdcArgs.join(" ") + " ").match(/ profile=([^ ]+) /);

    if (cmdlProf == null) {
        /* No profile=XYZ found */
        return [];
    }

    /* Split "whole_program,debug" into ["whole_program", "debug"] */
    var profiles = cmdlProf[1].split(',');

    return profiles;
}

/*
 *  ======== buildLibs ========
 *  This function generates the makefile goals for the libraries
 *  produced by a ti.sysbios package.
 */
function buildLibs(objList, relList, filter, xdcArgs)
{
    for (var i = 0; i < xdc.module('xdc.bld.BuildEnvironment').targets.length; i++) {
        var targ = xdc.module('xdc.bld.BuildEnvironment').targets[i];

        /* skip target if not supported */
        if (!supportsTarget(targ, filter)) {
            continue;
        }

        var profiles = getProfiles(xdcArgs);

        /* If no profiles were assigned, use only the default one. */
        if (profiles.length == 0) {
            profiles[0] = "debug";
        }

        for (var j = 0; j < profiles.length; j++) {
            var ccopts = "";
            var asmopts = "";

            if (profiles[j] == "smp") {
                var libPath = "lib/smpipc/debug/";
                ccopts += " -Dti_sysbios_BIOS_smpEnabled__D=TRUE";
                asmopts += " -Dti_sysbios_BIOS_smpEnabled__D=TRUE";
            }
            else {
                var libPath = "lib/ipc/debug/";
                /* build all package libs using Hwi macros */
                ccopts += " -Dti_sysbios_Build_useHwiMacros";
                ccopts += " -Dti_sysbios_BIOS_smpEnabled__D=FALSE";
                asmopts += " -Dti_sysbios_BIOS_smpEnabled__D=FALSE";
            }

            /* confirm that this target supports this profile */
            if (targ.profiles[profiles[j]] !== undefined) {
                var profile = profiles[j];
                var lib = Pkg.addLibrary(libPath + Pkg.name,
                                targ, {
                                profile: profile,
                                copts: ccopts,
                                aopts: asmopts,
                                releases: relList
                                });
                lib.addObjects(objList);
            }
        }
    }
}


/*
 *  ======== supportsTarget ========
 *  Returns true if target is in the filter object. If filter
 *  is null or empty, that's taken to mean all targets are supported.
 */
function supportsTarget(target, filter)
{
    var list, field;

    if (filter == null) {
        return true;
    }

    /*
     * For backwards compatibility, we support filter as an array of
     * target names.  The preferred approach is to specify filter as
     * an object with 'field' and 'list' elements.
     *
     * Old form:
     *     var trgFilter = [ "Arm9", "Arm9t", "Arm9t_big_endian" ]
     *
     * New (preferred) form:
     *
     *     var trgFilter = {
     *         field: "isa",
     *         list: [ "v5T", "v7R" ]
     *     };
     *
     */
    if (filter instanceof Array) {
        list = filter;
        field = "name";
    }
    else {
        list = filter.list;
        field = filter.field;
    }

    if (list == null || field == null) {
        throw("invalid filter parameter, must specify list and field!");
    }

    if (field == "noIsa") {
        if (String(','+list.toString()+',').match(','+target["isa"]+',')) {
            return (false);
        }
        return (true);
    }

    /*
     * add ',' at front and and tail of list and field strings to allow
     * use of simple match API.  For example, the string is updated to:
     * ',v5T,v7R,' to allow match of ',v5t,'.
     */
    if (String(','+list.toString()+',').match(','+target[field]+',')) {
        return (true);
    }

    return (false);
}
