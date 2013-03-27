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
 *  ======== NameServerRemoteRpmsg.c ========
 */

#include <xdc/std.h>
#include <string.h>

#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/knl/ISync.h>

#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sdo/utils/_NameServer.h>
#include <ti/sdo/utils/INameServerRemote.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/namesrv/_NameServerRemoteRpmsg.h>
#include <ti/ipc/rpmsg/RPMessage.h>

#include "package/internal/NameServerRemoteRpmsg.xdc.h"

/* Storage for NameServer message replies. Protected by gateMutex: */
static  NameServerRemote_Msg    NameServer_msg;

/*
 *************************************************************************
 *                       Instance functions
 *************************************************************************
 */
#define FXNN "NameServerRemoteRpmsg_Instance_init"
Void NameServerRemoteRpmsg_Instance_init(NameServerRemoteRpmsg_Object *obj,
        UInt16 remoteProcId,
        const NameServerRemoteRpmsg_Params *params)
{

    Log_print1(Diags_INFO, FXNN": remoteProc: %d", remoteProcId);

    obj->remoteProcId = remoteProcId;

    NameServerRemoteRpmsg_module->nsMsg = &NameServer_msg;

    /* register the remote driver with NameServer */
    ti_sdo_utils_NameServer_registerRemoteDriver(
        NameServerRemoteRpmsg_Handle_upCast(obj), remoteProcId);
}
#undef FXNN

/*
 *  ======== NameServerRemoteRpmsg_Instance_finalize ========
 */
Void NameServerRemoteRpmsg_Instance_finalize(NameServerRemoteRpmsg_Object *obj)
{
    /* unregister remote driver from NameServer module */
    ti_sdo_utils_NameServer_unregisterRemoteDriver(obj->remoteProcId);
}

/*
 *************************************************************************
 *                       Module functions
 *************************************************************************
 */

 /*
 *  ======== NameServerRemoteRpmsg_attach ========
 *
 *  Typically called from Ipc_attach(), but since Rpmsg doesn't use Ipc
 *  module, this may need to be called manually.
 */
#define FXNN "NameServerRemoteRpmsg_attach"
Int NameServerRemoteRpmsg_attach(UInt16 remoteProcId, Ptr sharedAddr)
{
    Int status = NameServerRemoteRpmsg_E_FAIL;
    NameServerRemoteRpmsg_Handle handle;

    Log_print1(Diags_INFO, FXNN": remoteProcId: %d", remoteProcId);

    handle = NameServerRemoteRpmsg_create(remoteProcId, NULL, NULL);
    if (handle != NULL) {
        status = NameServerRemoteRpmsg_S_SUCCESS;
    }

    return (status);
}
#undef FXNN

/*
 *  ======== NameServerRemoteRpmsg_detach ========
 *
 *  Typically called from Ipc_detach(), but since Rpmsg doesn't use Ipc
 *  module, this may need to be called manually.
 */
#define FXNN "NameServerRemoteRpmsg_detach"
Int NameServerRemoteRpmsg_detach(UInt16 remoteProcId)
{
    NameServerRemoteRpmsg_Handle handle;
    Int status = NameServerRemoteRpmsg_S_SUCCESS;

    Log_print1(Diags_INFO, FXNN": remoteProcId: %d", remoteProcId);

    for (handle = NameServerRemoteRpmsg_Object_first(); handle != NULL;
        handle = NameServerRemoteRpmsg_Object_next(handle)) {
        if (handle->remoteProcId == remoteProcId) {
            break;
        }
    }

    if (handle == NULL) {
        status = NameServerRemoteRpmsg_E_FAIL;
    }
    else {
        NameServerRemoteRpmsg_delete(&handle);
    }

    return (status);
}
#undef FXNN

/*
 *  ======== NameServerRemoteRpmsg_get ========
 */
#define FXNN "NameServerRemoteRpmsg_get"
Int NameServerRemoteRpmsg_get(NameServerRemoteRpmsg_Object *obj,
        String instanceName, String name, Ptr value, UInt32 *valueLen,
        ISync_Handle syncHandle, Error_Block *eb)
{
    Int                   status = NameServer_E_NOTFOUND;
    Int                   len;
    IArg                  key;
    NameServerRemote_Msg  msg;
    NameServerRemote_Msg  *replyMsg;
    Semaphore_Handle      semRemoteWait =
                           NameServerRemoteRpmsg_module->semRemoteWait;

    GateMutex_Handle gateMutex = NameServerRemoteRpmsg_module->gateMutex;

    /* enter gate - prevent multiple threads from entering */
    key = GateMutex_enter(gateMutex);

    /* Check that host NameServer is alive to avoid pinging the host: */
    if (NameServerRemoteRpmsg_module->nsPort == NAME_SERVER_PORT_INVALID) {
        status = NameServer_E_NOTFOUND;
        goto exit;
    }

    Log_print1(Diags_INFO, FXNN": name: %s", (IArg)name);

    /* Create request message and send to remote processor: */
    msg.request = NameServerRemoteRpmsg_REQUEST;
    msg.requestStatus = 0;

    len = strlen(instanceName);
    Assert_isTrue(len < MAXNAMEINCHAR, NameServerRemoteRpmsg_A_nameIsTooLong);
    strncpy((Char *)msg.instanceName, instanceName, len);

    len = strlen(name);
    Assert_isTrue(len < MAXNAMEINCHAR, NameServerRemoteRpmsg_A_nameIsTooLong);
    strncpy((Char *)msg.name, name, len);

    Log_print3(Diags_INFO, FXNN": Requesting from procId %d, %s:%s...\n",
               obj->remoteProcId, (IArg)msg.instanceName, (IArg)msg.name);
    RPMessage_send(obj->remoteProcId, NameServerRemoteRpmsg_module->nsPort,
               RPMSG_MESSAGEQ_PORT, (Ptr)&msg, sizeof(msg));

    /* Now pend for response */
    status = Semaphore_pend(semRemoteWait, NameServerRemoteRpmsg_timeout);

    if (status == FALSE) {
        Log_print0(Diags_INFO, FXNN": Wait for NS reply timed out\n");
        /* return timeout failure */
        return (NameServer_E_OSFAILURE);
    }

    /* get the message */
    replyMsg = NameServerRemoteRpmsg_module->nsMsg;

    if (replyMsg->requestStatus) {
        /* name is found */

        /* set length to amount of data that was copied */
        *valueLen = sizeof(Bits32);

        /* set the contents of value */
        memcpy(value, &(replyMsg->value), sizeof(Bits32));

        /* set the status to success */
        status = NameServer_S_SUCCESS;
        Log_print4(Diags_INFO, FXNN": Reply from: %d, %s:%s, value: 0x%x...\n",
              obj->remoteProcId, (IArg)msg.instanceName, (IArg)msg.name,
              *(UInt32 *)value);
    }
    else {
        /* name is not found */
        Log_print2(Diags_INFO, FXNN": value for %s:%s not found.\n",
                   (IArg)msg.instanceName, (IArg)msg.name);

        /* set status to not found */
        status = NameServer_E_NOTFOUND;
    }

exit:
    /* leave the gate */
    GateMutex_leave(gateMutex, key);

    return (status);
}
#undef FXNN

/*
 *  ======== NameServerRemoteRpmsg_sharedMemReq ========
 */
SizeT NameServerRemoteRpmsg_sharedMemReq(Ptr sharedAddr)
{
    return (0);
}

#define FXNN "NameServerRemote_processMessage"
void NameServerRemote_processMessage(NameServerRemote_Msg * msg)
{
    NameServer_Handle handle;
    Int               status = NameServer_E_FAIL;
    Semaphore_Handle  semRemoteWait =
                        NameServerRemoteRpmsg_module->semRemoteWait;
    UInt16 dstProc  = MultiProc_getId("HOST");

    Assert_isTrue(msg != NULL, NULL);

    if (msg->request == NameServerRemoteRpmsg_REQUEST) {
        Log_print1(Diags_INFO, FXNN": Request from procId %d.\n", dstProc);

        /*
         *  Message is a request. Lookup name in NameServer table.
         *  Send a response message back to source processor.
         */
        handle = NameServer_getHandle((String)msg->instanceName);

        if (handle != NULL) {
            /* Search for the NameServer entry */
            status = NameServer_getLocalUInt32(handle,
                     (String)msg->name, &msg->value);
        }

        /* set the request status */
        if (status < 0) {
            Log_print2(Diags_INFO, FXNN": Replying with: %s:%s not found\n",
                       (IArg)msg->instanceName, (IArg)msg->name);
            msg->requestStatus = 0;
        }
        else {
            Log_print3(Diags_INFO, FXNN": Replying with: %s:%s, value: 0x%x\n",
               (IArg)msg->instanceName, (IArg)msg->name, msg->value);
            msg->requestStatus = 1;
        }

        /* specify message as a response */
        msg->request = NameServerRemoteRpmsg_RESPONSE;

        /* send response message to remote processor */
        RPMessage_send(dstProc, NameServerRemoteRpmsg_module->nsPort,
                 RPMSG_MESSAGEQ_PORT, (Ptr)msg, sizeof(NameServerRemote_Msg));
    }
    else {
        Log_print0(Diags_INFO, FXNN": NameServer Reply. Posting Sem...\n");

        /* Save the response message.  */
        memcpy(NameServerRemoteRpmsg_module->nsMsg, msg,
                sizeof (NameServerRemote_Msg));
        /* Post the semaphore upon which NameServer_get() is waiting */
        Semaphore_post(semRemoteWait);
    }
}
#undef FXNN

#define FXNN "NameServerRemote_SetNameServerPort"
void NameServerRemote_SetNameServerPort(UInt port)
{
    Log_print1(Diags_INFO, FXNN": nsPort: %d", port);
    NameServerRemoteRpmsg_module->nsPort = port;
}
#undef FXNN
