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
 *  ======== IpcMgr.c ========
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>

#include <ti/ipc/Ipc.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/namesrv/NameServerRemoteRpmsg.h>
#include <ti/ipc/transports/TransportVirtioSetup.h>

/*
 *  ======== IpcMgr_ipcStartup ========
 */
Void IpcMgr_ipcStartup()
{
    UInt procId = MultiProc_getId("HOST");
    Int status;

    /* TransportVirtioSetup will busy wait until host kicks ready to recv: */
    status = TransportVirtioSetup_attach(procId, 0);
    Assert_isTrue(status >= 0, NULL);

    /* Sets up to comminicate with host's NameServer: */
    status = NameServerRemoteRpmsg_attach(procId, 0);
    Assert_isTrue(status >= 0, NULL);
}

/*
 *  ======== IpcMgr_callIpcStart ========
 *  Calls the Ipc_start command.  This must be done
 *  after IpcMgr_ipcStartup().
 */
Void IpcMgr_callIpcStart()
{
    Int status;

    /*
     *  Ipc_start() calls Ipc_attach() to synchronize all remote processors
     *  if 'Ipc.procSync' is set to 'Ipc.ProcSync_ALL' in *.cfg
     *  HOST is skipped, thanks to overriding NotifySetup_numIntLines().
     */
    status = Ipc_start();
    Assert_isTrue(status >= 0, NULL);
}
