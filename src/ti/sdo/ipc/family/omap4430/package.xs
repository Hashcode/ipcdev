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
 *  ======== package.xs ========
 */

/*
 *  ======== Package.validate ========
 */
function validate()
{
    if (!Program.build.target.$name.match(/M3/)) {
        /* This validation only needs to be done for "M3" */
        return;
    }

    var GateDualCore = xdc.module("ti.sysbios.family.arm.ducati.GateDualCore");
    var Core         = xdc.module("ti.sysbios.family.arm.ducati.Core");
    var MultiProc    = xdc.module("ti.sdo.utils.MultiProc");

    /* Check for a mismatch between Core.id and MultiProc name */
    if (MultiProc.nameList[MultiProc.id] == "CORE0" && Core.id != 0) {
        Core.$logError("CORE0 application should have Core.id " +
            " set to 0", Core, "id");
    }

    if (MultiProc.nameList[MultiProc.id] == "CORE1") {
        if (Core.id != 1) {
            Core.$logError("CORE1 application should have " +
                "Core.id set to 1", Core, "id");
        }

        if (MultiProc.getIdMeta("CORE0") == MultiProc.INVALIDID &&
            GateDualCore.initGates == false) {
            GateDualCore.$logWarning("If CORE0 core is not being used, " +
                "CORE1 application must be configured to initialize " +
                "GateDualCore at startup.  Set GateDualCore.initGates to " +
                "'true' to configure this.", GateDualCore, "initGates");
        }
    }
}

/*
 *  ======== Package.getLibs ========
 *  This function is called when a program's configuration files are
 *  being generated and it returns the name of a library appropriate
 *  for the program's configuration.
 */

function getLibs(prog)
{
    var Build = xdc.module("ti.sdo.ipc.Build");

    /* use shared getLibs() */
    return (Build.getLibs(this));
}

/*
 *  ======== package.close ========
 */
function close()
{
    if (xdc.om.$name != 'cfg') {
        return;
    }
}
