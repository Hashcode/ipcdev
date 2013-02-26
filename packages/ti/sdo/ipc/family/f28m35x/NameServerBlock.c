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
 *  ======== NameServerBlock.c ========
 */

#include <xdc/std.h>
#include <string.h>
#include <stdlib.h>

#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/knl/ISync.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/sdo/ipc/family/f28m35x/IpcMgr.h>

#include "package/internal/NameServerBlock.xdc.h"

#include <ti/sdo/ipc/_Notify.h>
#include <ti/sdo/utils/_MultiProc.h>
#include <ti/sdo/utils/_NameServer.h>

/* Need to use reserved notify events */
#undef NameServerBlock_notifyEventId
#define NameServerBlock_notifyEventId \
        ti_sdo_ipc_family_f28m35x_NameServerBlock_notifyEventId + \
                    (UInt32)((UInt32)Notify_SYSTEMKEY << 16)



/*
 *************************************************************************
 *                       Instance functions
 *************************************************************************
 */
Int NameServerBlock_Instance_init(NameServerBlock_Object *obj,
        UInt16 remoteProcId,
        const NameServerBlock_Params *params,
        Error_Block *eb)
{
    Int               status;
    Semaphore_Params  semParams;
    Semaphore_Handle  semRemoteWait;
    Semaphore_Handle  semMultiBlock;
    Swi_Handle        swiHandle;
    Swi_Params        swiParams;

    /* Assert that a NameServerBlock_Params has been supplied */
    Assert_isTrue(params != NULL, IpcMgr_A_nullArgument);

    /* Assert that remoteProcId is valid */
    Assert_isTrue(remoteProcId != MultiProc_self() &&
                  remoteProcId != MultiProc_INVALIDID,
                  IpcMgr_A_invParam);

    semRemoteWait = NameServerBlock_Instance_State_semRemoteWait(obj);
    semMultiBlock = NameServerBlock_Instance_State_semMultiBlock(obj);
    swiHandle = NameServerBlock_Instance_State_swiObj(obj);

    obj->readRequest = (NameServerBlock_Message *)(params->readAddr);
    obj->readResponse = (NameServerBlock_Message *)(
                        (UInt32)params->readAddr +
                        sizeof(NameServerBlock_Message));
    obj->writeRequest = (NameServerBlock_Message *)(params->writeAddr);
    obj->writeResponse = (NameServerBlock_Message *)(
                         (UInt32)params->writeAddr +
                         sizeof(NameServerBlock_Message));
    obj->remoteProcId = remoteProcId;

    /* construct the Semaphore */
    Semaphore_Params_init(&semParams);
    Semaphore_construct(Semaphore_struct(semRemoteWait), 0, &semParams);

    /* construct the semaphore */
    Semaphore_construct(Semaphore_struct(semMultiBlock), 1, &semParams);

    /* swi created with lowest priority and fxn = swiFxn */
    Swi_Params_init(&swiParams);
    swiParams.arg0 = (UArg)obj;
    swiParams.priority = 0;
    Swi_construct(Swi_struct(swiHandle),
                 (ti_sysbios_knl_Swi_FuncPtr)NameServerBlock_swiFxn,
                 &swiParams, eb);

    /* initialize own side of message struct only */
    obj->writeRequest->request = 0;
    obj->writeRequest->value = 0;
    obj->writeRequest->valueLen = 0;

    memset(obj->writeRequest->instanceName, 0,
               sizeof(obj->writeRequest->instanceName));
    memset(obj->writeRequest->name, 0,
               sizeof(obj->writeRequest->name));

    obj->writeResponse->response = 0;
    obj->writeResponse->requestStatus = 0;

    /* register the call back function and event Id with notify */
    status = Notify_registerEventSingle(
                remoteProcId,
                0,
                NameServerBlock_notifyEventId,
                (Notify_FnNotifyCbck)NameServerBlock_cbFxn,
                (UArg)swiHandle);

    /* if not successful return */
    if (status < 0) {
        Error_raise(eb, IpcMgr_E_internal, 0, 0);
        return (1);
    }

    /* register the remote driver with NameServer */
    ti_sdo_utils_NameServer_registerRemoteDriver(
            NameServerBlock_Handle_upCast(obj), remoteProcId);

    return (0);
}

/*
 *  ======== NameServerBlock_Instance_finalize ========
 */
Void NameServerBlock_Instance_finalize(NameServerBlock_Object *obj,
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
                       NameServerBlock_notifyEventId);
    }

    semRemoteWait = NameServerBlock_Instance_State_semRemoteWait(obj);
    if (semRemoteWait != NULL) {
        Semaphore_destruct(Semaphore_struct(semRemoteWait));
    }

    semMultiBlock = NameServerBlock_Instance_State_semMultiBlock(obj);
    if (semMultiBlock != NULL) {
        Semaphore_destruct(Semaphore_struct(semMultiBlock));
    }

    swiHandle = NameServerBlock_Instance_State_swiObj(obj);
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
 *  ======== NameServerBlock_attach ========
 */
Int NameServerBlock_attach(UInt16 remoteProcId, Ptr sharedAddr)
{
    return (NameServerBlock_E_FAIL);
}

/*
 *  ======== NameServerBlock_cbFxn ========
 */
Void NameServerBlock_cbFxn(UInt16 procId,
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
 *  ======== NameServerBlock_detach ========
 */
Int NameServerBlock_detach(UInt16 remoteProcId)
{
    return (NameServerBlock_E_FAIL);
}

/*
 *  ======== NameServerBlock_get ========
 */
Int NameServerBlock_get(NameServerBlock_Object *obj,
                        String instanceName,
                        String name,
                        Ptr value,
                        UInt32 *valueLen,
                        ISync_Handle syncHandle,
                        Error_Block *eb)
{
    Int len;
    Int retval = NameServer_E_NOTFOUND;
    Int status;
    Semaphore_Handle semRemoteWait;
    Semaphore_Handle semMultiBlock;

    Assert_isTrue(*valueLen <= (NameServerBlock_bufLen * sizeof(Bits32)),
                  NameServerBlock_A_invalidValueLen);

    semRemoteWait = NameServerBlock_Instance_State_semRemoteWait(obj);
    semMultiBlock = NameServerBlock_Instance_State_semMultiBlock(obj);

    /* prevent multiple threads from entering */
    Semaphore_pend(semMultiBlock, BIOS_WAIT_FOREVER);

    /* this is a request message */
    obj->writeRequest->request = 1;
    obj->writeRequest->response = 0;
    obj->writeRequest->valueLen = *valueLen;

#ifdef xdc_target__isaCompatible_28
    /*
     *  On C28, sizeof(Bits32) is 2. When requesting a value from the M3,
     *  this value needs to be doubled because sizeof(Bits32) is 4 on M3.
     */
    obj->writeRequest->valueLen = obj->writeRequest->valueLen << 1;

#else
    /*
     *  On M3, sizeof(Bits32) is 4. When requesting a value from the C28,
     *  this value needs to be halved because sizeof(Bits32) is 2 on C28.
     */
    obj->writeRequest->valueLen = obj->writeRequest->valueLen >> 1;

#endif

    /* copy the name of instance into shared memory */
    len = strlen(instanceName);
    NameServerBlock_strncpy((Char *)obj->writeRequest->instanceName,
                            instanceName,
                            len + 1);

    /* copy the name of nameserver entry into shared memory */
    len = strlen(name);
    NameServerBlock_strncpy((Char *)obj->writeRequest->name,
                            name,
                            len + 1);

    /* send the notification to remote processor */
    status = Notify_sendEvent(
                       obj->remoteProcId,
                       0,
                       NameServerBlock_notifyEventId,
                       0,
                       FALSE);

    if (status < 0) {
        /* undo previous options */
        obj->writeRequest->request = 0;
        obj->writeRequest->valueLen = 0;

        /* post the semaphore to make sure it doesn't block */
        Semaphore_post(semMultiBlock);

        return (retval);
    }

    /* pend here until we get a notification back from remote processor */
    status = Semaphore_pend(semRemoteWait, BIOS_WAIT_FOREVER);

    if (status == FALSE) {
        retval = NameServer_E_OSFAILURE;
    }
    else {
        /* if successful request then copy to value */
        if (obj->readResponse->requestStatus == TRUE) {
            /* copy to value */
            if (obj->readResponse->valueLen <= sizeof(UInt32)) {
                memcpy(value, &(obj->readResponse->value), sizeof(UInt32));
            }
            else {
                memcpy(value, &(obj->readResponse->valueBuf),
                       obj->readResponse->valueLen);
            }

            /* set length to amount of data that was copied */
            *valueLen = obj->readResponse->valueLen;

            /* set the status */
            retval = NameServer_S_SUCCESS;
        }

        /* clear out the request */
        obj->writeRequest->request = 0;
    }

    Semaphore_post(semMultiBlock);

    return (retval);
}

/*
 *  ======== NameServerBlock_sharedMemReq ========
 */
SizeT NameServerBlock_sharedMemReq(Ptr sharedAddr)
{
    /*
     *  Four Message structs are required.
     *  One for sending request and one for sending response.
     *  One for receiving request and one for receiving response.
     */
    if (ti_sdo_utils_MultiProc_numProcessors > 1) {
        return (4 * sizeof(NameServerBlock_Message));
    }

    return (0);
}

#ifdef xdc_target__isaCompatible_28
/*
 *  ======== NameServerBlock_strncpy ========
 *  Copies the source string into the destination string as a
 *  packed string.  The length includes the null terminating
 *  character.
 */
Char* NameServerBlock_strncpy(Char *dest, Char *src, SizeT len)
{
    Int i;
    UInt half = len >> 1;

    /* copy and pack the string */
    for (i = 0; i < half; i++) {
        dest[i] = src[i << 1] | (src[(i << 1) + 1] << 8);
    }

    /* The terminating character */
    if (len & 1) {
        /*
         *  (len = odd) ==> name has even number of characters.
         *  The next byte is (len >> 1), so zero that out.
         */
        dest[half] = '\0';
    }
    else {
        /*
         *  (len = even) ==> name has odd number of characters.
         *  Zero out the upper half of the last byte.
         */
        dest[half - 1] |= ('\0' << 8);
    }

    return (dest);
}

#else
/*
 *  ======== NameServerBlock_strncpy ========
 */
Char* NameServerBlock_strncpy(Char *dest, Char *src, SizeT len)
{
    return (strncpy(dest, src, len));
}

#endif

/*
 *  ======== NameServerBlock_swiFxn ========
 */
Void NameServerBlock_swiFxn(UArg arg)
{
    UInt32 valueLen;
    UInt hwiKey;
    NameServer_Handle handle;
    NameServerBlock_Object *obj;
#ifdef xdc_target__isaCompatible_28
    static Char instanceName[64];
    static Char name[64];
    String ptrIName;
    String ptrName;
    Int i;
#else
    String instanceName;
    String name;
#endif
    Int status;

    obj = (NameServerBlock_Object *)arg;

    /* In case of request */
    if (obj->readRequest->request == TRUE) {
#ifdef xdc_target__isaCompatible_28
        ptrIName = (String)obj->readRequest->instanceName;
        ptrName = (String)obj->readRequest->name;

        /* Unpack the string for the C28 */
        for (i = 0; i < 64; i++) {
            if (i & 1) {
                /* odd values are taken from the upper 8 bits shifted */
                instanceName[i] = ptrIName[i >> 1] >> 8;
                name[i] = ptrName[i >> 1] >> 8;
            }
            else {
                /* even values are taken from the lower 8 bits */
                instanceName[i] = ptrIName[i >> 1] & 0xFF;
                name[i] = ptrName[i >> 1] & 0xFF;
            }
        }
#else
        /* Strings are in the correct format already */
        instanceName = (String)obj->readRequest->instanceName;
        name = (String)obj->readRequest->name;
#endif
        /* get the NameServer handle */
        handle = NameServer_getHandle((String)instanceName);
        valueLen = obj->readRequest->valueLen;

        if (handle != NULL) {
            /* Search for the NameServer entry */
            if (valueLen <= sizeof(UInt32)) {
                status = NameServer_getLocalUInt32(handle,
                    (String)name,
                    &obj->writeResponse->value);
            }
            else {
                status = NameServer_getLocal(handle,
                    (String)name,
                    &obj->writeResponse->valueBuf, &valueLen);
            }
        }
        else {
            status = NameServer_E_FAIL;
        }

        /*
         *  If an entry was found, set requestStatus to TRUE
         *  and valueLen to the size of data that was copied.
         */
        if (status == NameServer_S_SUCCESS) {
            obj->writeResponse->requestStatus = TRUE;
#ifdef xdc_target__isaCompatible_28
            obj->writeResponse->valueLen = (valueLen << 1);
#else
            obj->writeResponse->valueLen = (valueLen >> 1);
#endif
        }
        else {
            obj->writeResponse->requestStatus = FALSE;
            obj->writeResponse->valueLen = 0;
        }

        /* Send a response back */
        obj->writeResponse->response = TRUE;

        /*
         *  The Notify line must be active at this point for this processor to
         *  have received a request
         */
        status = Notify_sendEvent(obj->remoteProcId,
                0, NameServerBlock_notifyEventId, 0, FALSE);

        /* The NS query could fail, but the reply should never fail */
        Assert_isTrue(status >= 0, IpcMgr_A_internal);
    }

    /* in case of response */
    if (obj->readResponse->response == TRUE &&
        obj->writeRequest->response == FALSE) {

        /* disable interrupts */
        hwiKey = Hwi_disable();

        /* acknowledge receive of response atomically with posting semaphore */
        obj->writeRequest->response = TRUE;

        /* post the Semaphore */
        Semaphore_post(
            NameServerBlock_Instance_State_semRemoteWait(obj));

        /* restore interrupts */
        Hwi_restore(hwiKey);
    }
}
