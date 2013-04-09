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
 *  ======== MmServiceMgr.c ========
 */

#include <string.h>

#include <xdc/std.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/System.h>

#include <ti/grcm/RcmServer.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/mm/MmType.h>
#include <ti/srvmgr/ServiceMgr.h>
#include <ti/srvmgr/omaprpc/OmapRpc.h>

#include "MmServiceMgr.h"

#define MmServiceMgr_PORT (59)

typedef struct {
    RcmServer_Params    rcmParams;
    Int                 aryLen;
    OmapRpc_FuncSignature *sigAry;
} MmServiceMgr_Client;

static Int MmServiceMgr_ref = 0;

/* temporary: needs to be removed */
extern Int32 OmapRpc_GetSvrMgrHandle(Void *srvc, Int32 num, Int32 *params);

/*
 *  ======== MmServiceMgr_init ========
 */
Int MmServiceMgr_init(Void)
{
    if (MmServiceMgr_ref++ != 0) {
        return(MmServiceMgr_S_SUCCESS); /* module already initialized */
    }

    RcmServer_init();
//  ServiceMgr_init();

    return(MmServiceMgr_S_SUCCESS);
}

/*
 *  ======== MmServiceMgr_exit ========
 */
Void MmServiceMgr_exit(Void)
{
    if (--MmServiceMgr_ref != 0) {
        return; /* module still in use */
    }

    RcmServer_exit();
}

/*
 *  ======== MmServiceMgr_register ========
 */
Int MmServiceMgr_register(const String name, RcmServer_Params *rcmParams,
        MmType_FxnSigTab *fxnSigTab, MmServiceMgr_DelFxn delFxn)
{
#if 1
    Int status = MmServiceMgr_S_SUCCESS;
    OmapRpc_Handle handle;

    handle = OmapRpc_createChannel(name, MultiProc_getId("HOST"),
            MmServiceMgr_PORT, rcmParams, fxnSigTab, delFxn);

    if (handle == NULL) {
        status = MmServiceMgr_E_FAIL;
    }

    return(status);
#else
    Int status = MmServiceMgr_S_SUCCESS;
    MmServiceMgr_Client *obj;
    RcmServer_FxnDesc *fxnAry;
    Int func;

    System_printf("MmServiceMgr_register: -->\n");

    obj = Memory_calloc(NULL, sizeof(MmServiceMgr_Client), 0, NULL);

    if (obj == NULL) {
        System_printf("MmServiceMgr_register: Error: out of memory\n");
        status = MmServiceMgr_E_FAIL;
        goto leave;
    }

    /* Temporary: make a local copy of server create params in order
     * to add one more function to the function table. Will be removed.
     */
    memcpy(&obj->rcmParams, rcmParams, sizeof(RcmServer_Params));
    obj->rcmParams.fxns.length = rcmParams->fxns.length + 1;

    obj->rcmParams.fxns.elem = Memory_calloc(NULL, obj->rcmParams.fxns.length *
            sizeof(RcmServer_FxnDesc), 0, NULL);

    if (obj->rcmParams.fxns.elem == NULL) {
        System_printf("MmServiceMgr_register: Error: out of memory\n");
        status = MmServiceMgr_E_FAIL;
        goto leave;
    }

    /* Temporary: make a local copy of signature array in order
     * to add the "first function" signature. Will be removed.
     */
    obj->aryLen = aryLen + 1;

    obj->sigAry = Memory_calloc(NULL, obj->aryLen *
            sizeof(OmapRpc_FuncSignature), 0, NULL);

    if (obj->sigAry == NULL) {
        System_printf("MmServiceMgr_register: Error: out of memory\n");
        status = MmServiceMgr_E_FAIL;
        goto leave;
    }

    /* Temporary: insert the "first function" in slot 0, then copy
     * in the caller's functions. Eventually, the create function will be
     * removed.
     */
    obj->rcmParams.fxns.elem[0].name =
            OmapRpc_Stringerize(OmapRpc_GetSvrMgrHandle);
    obj->rcmParams.fxns.elem[0].addr.createFxn =
            (RcmServer_MsgCreateFxn)OmapRpc_GetSvrMgrHandle;
    strncpy(obj->sigAry[0].name, obj->rcmParams.fxns.elem[0].name,
            OMAPRPC_MAX_CHANNEL_NAMELEN);
    obj->sigAry[0].numParam = 0;

    fxnAry = rcmParams->fxns.elem;

    for (func = 0; func < rcmParams->fxns.length; func++) {
        obj->rcmParams.fxns.elem[func+1].name = fxnAry[func].name;
        obj->rcmParams.fxns.elem[func+1].addr.fxn = fxnAry[func].addr.fxn;

        memcpy(&obj->sigAry[func+1], &sigAry[func],
                sizeof(OmapRpc_FuncSignature));
    }

    if (!ServiceMgr_register(name, &obj->rcmParams)) {
        System_printf("MmServiceMgr_register: Error: service register failed, "
                "status=%d\n");
        status = MmServiceMgr_E_FAIL;
        goto leave;
    }

leave:
    if (status < 0) {
        if ((obj != NULL) && (obj->sigAry != NULL)) {
            Memory_free(NULL, obj->sigAry,
                    obj->aryLen * sizeof(OmapRpc_FuncDeclaration));
        }
        if ((obj != NULL) && (obj->rcmParams.fxns.elem != NULL)) {
            Memory_free(NULL, obj->rcmParams.fxns.elem,
                    obj->rcmParams.fxns.length * sizeof(RcmServer_FxnDesc));
        }
        if (obj != NULL) {
            Memory_free(NULL, obj, sizeof(MmServiceMgr_Client));
        }
    }

    System_printf("MmServiceMgr_register: <--, status=%d\n", status);
    return(status);
#endif
}

#if 0
/*
 *  ======== MmServiceMgr_start ========
 */
Int MmServiceMgr_start(const String name, Int aryLen,
        OmapRpc_FuncSignature *sigAry)
{
    extern Int OmapRpc_start(const String name, Int port, Int aryLen,
        OmapRpc_FuncSignature *sigAry);

    Int status = MmServiceMgr_S_SUCCESS;

    OmapRpc_start(name, MmServiceMgr_PORT, aryLen, sigAry);

    return(status);
}
#endif
