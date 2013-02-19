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
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Startup.h>

#include <ti/sysbios/hal/Hwi.h>
#include <ti/sdo/ipc/family/f28m35x/NotifyDriverCirc.h>
#include <ti/sdo/ipc/family/f28m35x/NameServerBlock.h>
#include <ti/sdo/ipc/family/f28m35x/TransportCirc.h>
#include <ti/sdo/ipc/_MessageQ.h>
#include <ti/sdo/ipc/_Notify.h>
#include <ti/sdo/utils/_MultiProc.h>

#include "package/internal/IpcMgr.xdc.h"

/* For the M3 */
#define CTOMIPCACK  (0x400FB700)
#define CTOMIPCSTS  (CTOMIPCACK + 0x4)
#define MTOCIPCSET  (CTOMIPCACK + 0x8)
#define MTOCIPCCLR  (CTOMIPCACK + 0xC)
#define MTOCIPCFLG  (CTOMIPCACK + 0x10)

/* For the C28 */
#define CTOMIPCSET  (0x00004E00)
#define CTOMIPCCLR  (CTOMIPCSET + 0x2)
#define CTOMIPCFLG  (CTOMIPCSET + 0x4)
#define MTOCIPCACK  (CTOMIPCSET + 0x6)
#define MTOCIPCSTS  (CTOMIPCSET + 0x8)

#define M3_IPCFLAG  10
#define C28_IPCFLAG 11

/*
 *  ======== IpcMgr_Module_startup ========
 *  In this function the M3 processors is used to enable shared memory.
 *  In addition, the owner of each block of memory is initialized and
 *  the write access set based on the static configuration parameters.
 *  IPC flags are used by the M3 and C28 respectively for synchronization.
 *
 *  The M3 starts by creating its driver instances and then sets the
 *  C28's IPC flag to let the C28 proceed.  Then the M3 waits for its
 *  IPC flag to be set before proceeding.  Once its IPC flag is set
 *  by the C28, the M3 clears its IPC flag and continues.
 *
 *  The C28 starts by waiting for its IPC flag to be set by the M3.
 *  Once its IPC flag is set by the M3, the C28 clears its IPC flag and
 *  proceeds to create its driver instances.  The C28 then sets the M3's
 *  IPC flag to let the M3 proceed.
 *
 *  The shared memory usage looks like the following:
 *
 *      |--------------------|
 *      | Notify Driver      |
 *      |                    |
 *      |--------------------|
 *      | NameServer         |
 *      | Remote Driver      |
 *      |--------------------|
 *      | MessageQ Transport |
 *      |                    |
 *      |--------------------|
 */
Int IpcMgr_Module_startup(Int phase)
{
    Int status;
    SizeT memReq;
    Ptr writeAddr = (UInt32 *)IpcMgr_writeAddr;
    Ptr readAddr = (UInt32 *)IpcMgr_readAddr;
    UInt16 remoteProcId;
    NotifyDriverCirc_Params notifyDrvParams;
    TransportCirc_Params transportParams;
#ifdef xdc_target__isaCompatible_v7M
    UInt32 i;
    volatile UInt32 *memcnf  = (volatile UInt32 *)IpcMgr_MEMCNF;
    volatile UInt32 *msmsel  = (volatile UInt32 *)IpcMgr_MSxMSEL;
    volatile UInt32 *mssrcr  = (volatile UInt32 *)IpcMgr_MSxSRCR;
    volatile UInt32 *set = (volatile UInt32 *)MTOCIPCSET;
    volatile UInt32 *stat = (volatile UInt32 *)CTOMIPCSTS;
    volatile UInt32 *ack = (volatile UInt32 *)CTOMIPCACK;
#else
    volatile UInt32 *set = (volatile UInt32 *)CTOMIPCSET;
    volatile UInt32 *stat = (volatile UInt32 *)MTOCIPCSTS;
    volatile UInt32 *ack = (volatile UInt32 *)MTOCIPCACK;
#endif

    /*
     *  This code assumes that the device's C28 and M3 MultiProc Ids
     *  are next to each other (e.g. n and n + 1) and that the first
     *  one is even (e.g. n is even).
     */
    if (MultiProc_self() & 1) {
        /* I'm odd */
        remoteProcId = MultiProc_self() - 1;
    }
    else {
        /* I'm even */
        remoteProcId = MultiProc_self() + 1;
    }

    /* wait for Hwi module to initialize first because of NotifyDriverCirc  */
    if (!Hwi_Module_startupDone()) {
        return Startup_NOTDONE;
    }

#ifdef xdc_target__isaCompatible_v7M
    /*
     *  The M3 writes the shared memory enable and owner select
     *  registers before either processor starts using shared memory.
     */

    /* write the shared memory configuration register */
    *memcnf = IpcMgr_sharedMemoryEnable;

    /* write the owner select register */
    *msmsel = IpcMgr_sharedMemoryOwnerMask;

    /* init the owner write access registers */
    for (i = 0; i < 2; i++) {
        mssrcr[i] = (IpcMgr_sharedMemoryAccess[(i * 4)])           |
                    (IpcMgr_sharedMemoryAccess[(i * 4) + 1] << 8)  |
                    (IpcMgr_sharedMemoryAccess[(i * 4) + 2] << 16) |
                    (IpcMgr_sharedMemoryAccess[(i * 4) + 3] << 24);
    }

#else

    /* wait for M3 to set C28's IPC flag */
    while (!(*stat & (1 << C28_IPCFLAG))) {
    }

    /* clear own IPC flag */
    *ack = 1 << C28_IPCFLAG;

#endif

    /* determine the amount of memory required for NotifyDriverCirc */
    NotifyDriverCirc_Params_init(&notifyDrvParams);
    notifyDrvParams.writeAddr = writeAddr;
    memReq = NotifyDriverCirc_sharedMemReq(&notifyDrvParams);

    /* call NotifyCircSetup attach to remote processor */
    status = IpcMgr_notifyCircAttach(remoteProcId,
                 writeAddr, readAddr);

    Assert_isTrue(status >= 0, IpcMgr_A_internal);

    /* update the read/write address */
    writeAddr = (Ptr)((UInt32)writeAddr + memReq);
    readAddr = (Ptr)((UInt32)readAddr + memReq);

    /* determine the amount of memory required for NameServerBlock */
    memReq = NameServerBlock_sharedMemReq(NULL);

    /* call NameServerBlock attach to remote processor */
    status = IpcMgr_nameServerAttach(remoteProcId, writeAddr, readAddr);

    Assert_isTrue(status >= 0, IpcMgr_A_internal);

    /* update the read/write address */
    writeAddr = (Ptr)((UInt32)writeAddr + memReq);
    readAddr = (Ptr)((UInt32)readAddr + memReq);

    /* determine the amount of memory required for TransportCirc */
    TransportCirc_Params_init(&transportParams);
    transportParams.writeAddr = writeAddr;
    memReq = TransportCirc_sharedMemReq(&transportParams);

    /* call TransportCircSetup attach to remote processor */
    status = IpcMgr_transportCircAttach(remoteProcId,
                 writeAddr, readAddr);

    Assert_isTrue(status >= 0, IpcMgr_A_internal);

#ifdef xdc_target__isaCompatible_v7M

    /* set C28 IPC flag to tell C28 to proceed */
    *set = 1 << C28_IPCFLAG;

    /* wait for C28 to set M3's IPC flag */
    while (!(*stat & (1 << M3_IPCFLAG))) {
    }

    /* clear own IPC flag */
    *ack = 1 << M3_IPCFLAG;

#else

    /* set M3's IPC flag to tell M3 to proceed */
    *set = 1 << M3_IPCFLAG;

#endif

    return (Startup_DONE);
}

/*
 *  ======== IpcMgr_init ========
 *  Init CTOMMSGRAM and MTOCMSGRAM
 */
Void IpcMgr_init()
{
#ifdef xdc_target__isaCompatible_v7M
    volatile UInt32 *mwrallow  = (volatile UInt32 *)IpcMgr_MWRALLOW;
    volatile UInt32 *mtocrTestInit = (volatile UInt32 *)IpcMgr_MTOCRTESTINIT;
    volatile UInt32 *mtocrInitDone = (volatile UInt32 *)IpcMgr_MTOCRINITDONE;

    /* allow writes to protected registers. */
    *mwrallow = 0xA5A5A5A5;

    /* init MtoCMsgRam */
    *mtocrTestInit |= 0x1;

    /* make sure init is done */
    while ((*mtocrInitDone & 0x1) != 0x1) {
    }

    /* Disable writes to protected registers. */
    *mwrallow = 0;

#else

    volatile UInt32 *c28rTestInit = (volatile UInt32 *)IpcMgr_C28RTESTINIT;
    volatile UInt32 *c28rInitDone = (volatile UInt32 *)IpcMgr_C28RINITDONE;

    asm(" EALLOW");

    /* init CtoMMsgRam */
    *c28rTestInit |= (0x1 << 4);

    /* make sure init is done */
    while ((*c28rInitDone & (0x1 << 4)) != (0x1 << 4)) {
    }

    asm(" EDIS");

#endif
}


/*
 *  ======== IpcMgr_notifyCircAttach ========
 *  Initialize interrupt
 */
Int IpcMgr_notifyCircAttach(UInt16 remoteProcId, Ptr writeAddr, Ptr readAddr)
{
    NotifyDriverCirc_Params notifyDrvParams;
    NotifyDriverCirc_Handle notifyDrvHandle;
    ti_sdo_ipc_Notify_Handle notifyHandle;
    Error_Block eb;
    Int status = Notify_S_SUCCESS;

    /* Initialize the error block */
    Error_init(&eb);

    /* Setup the notify driver to the remote processor */
    NotifyDriverCirc_Params_init(&notifyDrvParams);

    /* set the read/write address of the param */
    notifyDrvParams.readAddr = readAddr;
    notifyDrvParams.writeAddr = writeAddr;

    /* create the notify driver instance */
    notifyDrvHandle = NotifyDriverCirc_create(&notifyDrvParams, &eb);
    if (notifyDrvHandle == NULL) {
        return (Notify_E_FAIL);
    }

    /* create the notify instance */
    notifyHandle = ti_sdo_ipc_Notify_create(
                       NotifyDriverCirc_Handle_upCast(notifyDrvHandle),
                       remoteProcId, 0, NULL, &eb);

    if (notifyHandle == NULL) {
        NotifyDriverCirc_delete(&notifyDrvHandle);
        status = Notify_E_FAIL;
    }

    return (status);
}

/*
 *  ======== IpcMgr_nameServerAttach ========
 */
Int IpcMgr_nameServerAttach(UInt16 remoteProcId, Ptr writeAddr, Ptr readAddr)
{
    NameServerBlock_Params nsbParams;
    NameServerBlock_Handle handle;
    Int status = NameServerBlock_E_FAIL;
    Error_Block eb;

    Error_init(&eb);

    /* Init the param */
    NameServerBlock_Params_init(&nsbParams);

    /* set the read/write addresses */
    nsbParams.readAddr  = readAddr;
    nsbParams.writeAddr = writeAddr;

    /* create only if notify driver has been created to remote proc */
    if (Notify_intLineRegistered(remoteProcId, 0)) {
        handle = NameServerBlock_create(remoteProcId,
                                        &nsbParams,
                                        &eb);
        if (handle != NULL) {
            status = NameServerBlock_S_SUCCESS;
        }
    }

    return (status);
}

/*
 *  ======== IpcMgr_transportCircAttach ========
 */
Int IpcMgr_transportCircAttach(UInt16 remoteProcId, Ptr writeAddr,
    Ptr readAddr)
{
    TransportCirc_Handle handle;
    TransportCirc_Params params;
    Int status = MessageQ_E_FAIL;
    Error_Block eb;

    Error_init(&eb);

    /* init the transport parameters */
    TransportCirc_Params_init(&params);
    params.readAddr = readAddr;
    params.writeAddr = writeAddr;

    /* make sure notify driver has been created */
    if (Notify_intLineRegistered(remoteProcId, 0)) {
        handle = TransportCirc_create(remoteProcId, &params, &eb);

        if (handle != NULL) {
            status = MessageQ_S_SUCCESS;
        }
    }

    return (status);
}
