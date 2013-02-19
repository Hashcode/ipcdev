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
 *  ======== NameServerRemoteNotify.c ========
 */

/* Remove Windows strncpy warning */
#define _CRT_SECURE_NO_DEPRECATE 1

#include <xdc/std.h>
#include <string.h>
#include <stdlib.h>

#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/knl/ISync.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Cache.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "package/internal/NameServerRemoteNotify.xdc.h"

#include <ti/sdo/ipc/Ipc.h>
#include <ti/sdo/ipc/_Notify.h>
#include <ti/sdo/ipc/_GateMP.h>
#include <ti/sdo/ipc/_SharedRegion.h>
#include <ti/sdo/utils/_MultiProc.h>
#include <ti/sdo/utils/_NameServer.h>

/* Need to use reserved notify events */
#undef NameServerRemoteNotify_notifyEventId
#define NameServerRemoteNotify_notifyEventId \
        ti_sdo_ipc_nsremote_NameServerRemoteNotify_notifyEventId + \
                    (UInt32)((UInt32)Notify_SYSTEMKEY << 16)

/*
 *************************************************************************
 *                       Instance functions
 *************************************************************************
 */
Int NameServerRemoteNotify_Instance_init(NameServerRemoteNotify_Object *obj,
        UInt16 remoteProcId,
        const NameServerRemoteNotify_Params *params,
        Error_Block *eb)
{
    Int               offset = 0;
    Int               status;
    Semaphore_Params  semParams;
    Semaphore_Handle  semRemoteWait;
    Semaphore_Handle  semMultiBlock;
    Swi_Handle        swiHandle;
    Swi_Params        swiParams;

    /* Assert that a NameServerRemoteNotify_Params has been supplied */
    Assert_isTrue(params != NULL, Ipc_A_nullArgument);

    /* Assert that remoteProcId is valid */
    Assert_isTrue(remoteProcId != MultiProc_self() &&
                  remoteProcId != MultiProc_INVALIDID,
                  Ipc_A_invParam);

    /* determine the offset based upon own id and remote's id */
    if (MultiProc_self() > remoteProcId) {
        offset = 1;
    }

    obj->regionId = SharedRegion_getId(params->sharedAddr);

    /* Assert that sharedAddr is cache aligned */
    Assert_isTrue(((UInt32)params->sharedAddr %
                  SharedRegion_getCacheLineSize(obj->regionId) == 0),
                  Ipc_A_addrNotCacheAligned);

    semRemoteWait = NameServerRemoteNotify_Instance_State_semRemoteWait(obj);
    semMultiBlock = NameServerRemoteNotify_Instance_State_semMultiBlock(obj);
    swiHandle = NameServerRemoteNotify_Instance_State_swiObj(obj);

    obj->msg[0] = (NameServerRemoteNotify_Message *)(params->sharedAddr);
    obj->msg[1] = (NameServerRemoteNotify_Message *)((UInt32)obj->msg[0] +
                          sizeof(NameServerRemoteNotify_Message));
    obj->gate = params->gate;
    obj->remoteProcId = remoteProcId;

    /* construct the semaphore */
    Semaphore_Params_init(&semParams);
    Semaphore_construct(Semaphore_struct(semRemoteWait), 0, &semParams);

    /* construct the semaphore */
    Semaphore_construct(Semaphore_struct(semMultiBlock), 1, &semParams);

    /* swi created with lowest priority and fxn = swiFxn */
    Swi_Params_init(&swiParams);
    swiParams.arg0 = (UArg)obj;
    swiParams.priority = 0;
    Swi_construct(Swi_struct(swiHandle),
                 (ti_sysbios_knl_Swi_FuncPtr)NameServerRemoteNotify_swiFxn,
                 &swiParams, eb);

    /* initialize own side of message struct only */
    obj->msg[offset]->request = 0;
    obj->msg[offset]->response = 0;
    obj->msg[offset]->requestStatus = 0;
    obj->msg[offset]->value = 0;
    obj->msg[offset]->valueLen = 0;

    memset(obj->msg[offset]->instanceName, 0,
               sizeof(obj->msg[offset]->instanceName));
    memset(obj->msg[offset]->name, 0,
               sizeof(obj->msg[offset]->name));

    /* determine cacheability of the object from the regionId */
    obj->cacheEnable = SharedRegion_isCacheEnabled(obj->regionId);
    if (obj->cacheEnable) {
        /* write back shared memory that was modified */
        Cache_wbInv(obj->msg[offset], sizeof(NameServerRemoteNotify_Message),
                    Cache_Type_ALL, TRUE);
    }

    /* register the call back function and event Id with notify */
    status = Notify_registerEventSingle(
                remoteProcId,
                0,
                NameServerRemoteNotify_notifyEventId,
                (Notify_FnNotifyCbck)NameServerRemoteNotify_cbFxn,
                (UArg)swiHandle);

    /* if not successful return */
    if (status < 0) {
        Error_raise(eb, ti_sdo_ipc_Ipc_E_internal, 0, 0);
        return (1);
    }

    /* register the remote driver with NameServer */
    ti_sdo_utils_NameServer_registerRemoteDriver(
            NameServerRemoteNotify_Handle_upCast(obj), remoteProcId);

    return (0);
}

/*
 *  ======== NameServerRemoteNotify_Instance_finalize ========
 */
Void NameServerRemoteNotify_Instance_finalize(NameServerRemoteNotify_Object *obj,
    Int status)
{
    Semaphore_Handle  semRemoteWait;
    Semaphore_Handle  semMultiBlock;
    Swi_Handle        swiHandle;

    if (status == 0) {
        /* unregister remote driver from NameServer module */
        ti_sdo_utils_NameServer_unregisterRemoteDriver(obj->remoteProcId);

        /* unregister event from Notify module */
        Notify_unregisterEventSingle(
                       obj->remoteProcId,
                       0,
                       NameServerRemoteNotify_notifyEventId);
    }

    semRemoteWait = NameServerRemoteNotify_Instance_State_semRemoteWait(obj);
    if (semRemoteWait != NULL) {
        Semaphore_destruct(Semaphore_struct(semRemoteWait));
    }

    semMultiBlock = NameServerRemoteNotify_Instance_State_semMultiBlock(obj);
    if (semMultiBlock != NULL) {
        Semaphore_destruct(Semaphore_struct(semMultiBlock));
    }

    swiHandle = NameServerRemoteNotify_Instance_State_swiObj(obj);
    if (swiHandle != NULL) {
        Swi_destruct(Swi_struct(swiHandle));
    }
}

/*
 *************************************************************************
 *                       Module functions
 *************************************************************************
 */

/*
 *  ======== NameServerRemoteNotify_attach ========
 */
Int NameServerRemoteNotify_attach(UInt16 remoteProcId, Ptr sharedAddr)
{
    NameServerRemoteNotify_Params nsrParams;
    NameServerRemoteNotify_Handle handle;
    Int status = NameServerRemoteNotify_E_FAIL;
    Error_Block eb;

    /* Assert that the default GateMP is not NULL */
    Assert_isTrue(GateMP_getDefaultRemote() != NULL, Ipc_A_internal);

    Error_init(&eb);

    /* Use default GateMP */
    NameServerRemoteNotify_Params_init(&nsrParams);
    nsrParams.gate = (ti_sdo_ipc_GateMP_Handle)GateMP_getDefaultRemote();

    /* determine the address */
    nsrParams.sharedAddr = sharedAddr;

    /* create only if notify driver has been created to remote proc */
    if (Notify_intLineRegistered(remoteProcId, 0)) {
        handle = NameServerRemoteNotify_create(remoteProcId,
                                               &nsrParams,
                                               &eb);
        if (handle != NULL) {
            status = NameServerRemoteNotify_S_SUCCESS;
        }
    }

    return (status);
}

/*
 *  ======== NameServerRemoteNotify_cbFxn ========
 */
Void NameServerRemoteNotify_cbFxn(UInt16 procId,
                                  UInt16 lineId,
                                  UInt32 eventId,
                                  UArg arg,
                                  UInt32 payload)
{
    Swi_Handle swiHandle;

    /* Swi_Handle was passed as arg in register */
    swiHandle = (Swi_Handle)arg;

    /* post the Swi */
    Swi_post(swiHandle);
}

/*
 *  ======== NameServerRemoteNotify_detach ========
 */
Int NameServerRemoteNotify_detach(UInt16 remoteProcId)
{
    NameServerRemoteNotify_Handle handle;
    Int status = NameServerRemoteNotify_S_SUCCESS;

    for (handle = NameServerRemoteNotify_Object_first(); handle != NULL;
        handle = NameServerRemoteNotify_Object_next(handle)) {
        if (handle->remoteProcId == remoteProcId) {
            break;
        }
    }

    if (handle == NULL) {
        status = NameServerRemoteNotify_E_FAIL;
    }
    else {
        NameServerRemoteNotify_delete(&handle);
    }

    return (status);
}

/*
 *  ======== NameServerRemoteNotify_get ========
 */
Int NameServerRemoteNotify_get(NameServerRemoteNotify_Object *obj,
                               String instanceName,
                               String name,
                               Ptr value,
                               UInt32 *valueLen,
                               ISync_Handle syncHandle,
                               Error_Block *eb)
{
    Int len;
    Int retval = NameServer_E_NOTFOUND;
    Int offset = 0;
    Int status;
    Int notifyStatus;
    IArg key;
    Semaphore_Handle semRemoteWait;
    Semaphore_Handle semMultiBlock;

    Assert_isTrue(*valueLen <= 300,
                      NameServerRemoteNotify_A_invalidValueLen);

    semRemoteWait = NameServerRemoteNotify_Instance_State_semRemoteWait(obj);
    semMultiBlock = NameServerRemoteNotify_Instance_State_semMultiBlock(obj);

    /* prevent multiple threads from entering */
    Semaphore_pend(semMultiBlock, BIOS_WAIT_FOREVER);

    if (MultiProc_self() > obj->remoteProcId) {
            offset = 1;
    }

    if (obj->cacheEnable) {
        /* Make sure there's no outstanding message */
        Cache_inv(obj->msg[offset], sizeof(NameServerRemoteNotify_Message),
                  Cache_Type_ALL, TRUE);
    }

    /* enter gate so nobody can be modifying message in shared memory */
    key = GateMP_enter((GateMP_Handle)obj->gate);

    /* this is a request message */
    obj->msg[offset]->request = 1;
    obj->msg[offset]->response = 0;
    obj->msg[offset]->requestStatus = 0;
    obj->msg[offset]->valueLen = *valueLen;

    /* copy the name of instance into shared memory */
    len = strlen(instanceName);
    strncpy((Char *)obj->msg[offset]->instanceName, instanceName, len + 1);

    /* copy the name of nameserver entry into shared memory */
    len = strlen(name);
    strncpy((Char *)obj->msg[offset]->name, name, len + 1);

    if (obj->cacheEnable) {
        Cache_wbInv(obj->msg[offset], sizeof(NameServerRemoteNotify_Message),
                    Cache_Type_ALL, TRUE);
    }

    /*
     *  Send the notification to remote processor. Do not wait here since
     *  we hold the GateMP.
     */
    notifyStatus = Notify_sendEvent(
                       obj->remoteProcId,
                       0,
                       NameServerRemoteNotify_notifyEventId,
                       0,
                       FALSE);

    /* now we can leave the gate after we send the notification */
    GateMP_leave((GateMP_Handle)obj->gate, key);

    if (notifyStatus < 0) {
        /* undo previous options */
        obj->msg[offset]->request = 0;
        obj->msg[offset]->valueLen = 0;

        /* post the semaphore to make sure it doesn't block */
        Semaphore_post(semMultiBlock);

        return (retval);
    }

    /* pend here until we get a notification back from remote processor */
    status = Semaphore_pend(semRemoteWait, NameServerRemoteNotify_timeout);

    if (status == FALSE) {
        retval = NameServer_E_OSFAILURE;
    }
    else {
        /* getting here means we got the notification back */
        /* enter gate so nobody can be modifying message in shared memory */
        key = GateMP_enter((GateMP_Handle)obj->gate);

        if (obj->cacheEnable) {
            Cache_inv(obj->msg[offset], sizeof(NameServerRemoteNotify_Message),
                      Cache_Type_ALL, TRUE);
        }

        /* if successful request then copy to value */
        if (obj->msg[offset]->requestStatus == TRUE) {
            /* clear out requestStatus */
            obj->msg[offset]->requestStatus = FALSE;

            /* copy to value */
            if (obj->msg[offset]->valueLen == sizeof(UInt32)) {
                memcpy(value, &(obj->msg[offset]->value), sizeof(UInt32));
            }
            else {
                memcpy(value, &(obj->msg[offset]->valueBuf),
                       obj->msg[offset]->valueLen);
            }

            /* set length to amount of data that was copied */
            *valueLen = obj->msg[offset]->valueLen;

            /* set the status */
            retval = NameServer_S_SUCCESS;
        }

        /* clear out the request and response flags */
        obj->msg[offset]->request  = 0;
        obj->msg[offset]->response = 0;

        if (obj->cacheEnable) {
            Cache_wbInv(obj->msg[offset],
                        sizeof(NameServerRemoteNotify_Message),
                        Cache_Type_ALL, TRUE);
        }

        /*  Now we can leave the gate after we send the notification */
        GateMP_leave((GateMP_Handle)obj->gate, key);
    }

    Semaphore_post(semMultiBlock);

    return (retval);
}

/*
 *  ======== NameServerRemoteNotify_sharedMemReq ========
 */
SizeT NameServerRemoteNotify_sharedMemReq(Ptr sharedAddr)
{
    /*
     *  Two Message structs are required.
     *  One for sending request and one for sending response.
     */
    if (ti_sdo_utils_MultiProc_numProcessors > 1) {
        return (2 * sizeof(NameServerRemoteNotify_Message));
    }

    return (0);
}

/*
 *  ======== NameServerRemoteNotify_swiFxn ========
 */
Void NameServerRemoteNotify_swiFxn(UArg arg)
{
    Int count = NameServer_E_FAIL;
    Int offset = 0;
    UInt32 valueLen;
    IArg key;
    NameServer_Handle handle;
    NameServerRemoteNotify_Object *obj;
#ifndef xdc_runtime_Assert_DISABLE_ALL
    Int status;
#endif

    obj = (NameServerRemoteNotify_Object *)arg;

    if (MultiProc_self() > obj->remoteProcId) {
        offset = 1;
    }

    if (obj->cacheEnable) {
        /* invalidate both request and response messages */
        Cache_inv(obj->msg[0],
                  sizeof(NameServerRemoteNotify_Message) << 1,
                  Cache_Type_ALL, TRUE);
    }

    /* in case of request */
    if (obj->msg[1 - offset]->request == TRUE) {
        /* get the NameServer handle */
        handle = NameServer_getHandle(
                     (String)obj->msg[1 - offset]->instanceName);
        valueLen = obj->msg[1 - offset]->valueLen;

        if (handle != NULL) {
            /* Search for the NameServer entry */
            if (valueLen == sizeof(UInt32)) {
                count = NameServer_getLocalUInt32(handle,
                    (String)obj->msg[1 - offset]->name,
                    &obj->msg[1 - offset]->value);
            }
            else {
                count = NameServer_getLocal(handle,
                    (String)obj->msg[1 - offset]->name,
                    &obj->msg[1 - offset]->valueBuf, &valueLen);
            }
        }

        /* enter gate so nobody can be modifying message in shared memory */
        key = GateMP_enter((GateMP_Handle)obj->gate);

        /*
         *  If an entry was found, set requestStatus to TRUE
         *  and valueLen to the size of data that was copied.
         */
        if (count == NameServer_S_SUCCESS) {
            obj->msg[1 - offset]->requestStatus = TRUE;
            obj->msg[1 - offset]->valueLen = valueLen;
        }

        /* Send a response back */
        obj->msg[1 - offset]->response = TRUE;
        obj->msg[1 - offset]->request = FALSE;

        if (obj->cacheEnable) {
            Cache_wbInv(obj->msg[1 - offset],
                        sizeof(NameServerRemoteNotify_Message),
                        Cache_Type_ALL, TRUE);
        }

        /* now we can leave the gate */
        GateMP_leave((GateMP_Handle)obj->gate, key);

        /*
         *  The Notify line must be active at this point for this processor to
         *  have received a request.  Do not wait here since we are in a Swi.
         */
#ifndef xdc_runtime_Assert_DISABLE_ALL
        status =
#endif
        Notify_sendEvent(obj->remoteProcId,
                0, NameServerRemoteNotify_notifyEventId, 0, FALSE);
        /* The NS query could fail, but the reply should never fail */
        Assert_isTrue(status >= 0, Ipc_A_internal);

    }

    /* in case of response */
    if (obj->msg[offset]->response == TRUE) {
        /* post the Semaphore */
        Semaphore_post(
            NameServerRemoteNotify_Instance_State_semRemoteWait(obj));
    }
}

/*
 *  ======== NameServerRemoteNotify_getHandle ========
 */
NameServerRemoteNotify_Handle NameServerRemoteNotify_getHandle(
    UInt16 remoteProcId)
{
    NameServerRemoteNotify_Handle obj;

    for (obj = NameServerRemoteNotify_Object_first(); obj != NULL;
        obj = NameServerRemoteNotify_Object_next(obj)) {
        if (obj->remoteProcId == remoteProcId) {
            break;
        }
    }

    return (obj);
}
