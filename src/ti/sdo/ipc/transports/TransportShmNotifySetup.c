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
 *  ======== TransportShmNotifySetup.c ========
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>

#include <ti/sdo/ipc/transports/TransportShmNotify.h>

#include "package/internal/TransportShmNotifySetup.xdc.h"

#include <ti/sdo/ipc/_MessageQ.h>
#include <ti/sdo/ipc/_Notify.h>
#include <ti/sdo/utils/_MultiProc.h>

/*
 *  ======== TransportShmNotifySetup_attach ========
 */
Int TransportShmNotifySetup_attach(UInt16 remoteProcId, Ptr sharedAddr)
{
    TransportShmNotify_Handle handle;
    TransportShmNotify_Params params;
    Int status = MessageQ_E_FAIL;
    Error_Block eb;

    Error_init(&eb);

    /* init the transport parameters */
    TransportShmNotify_Params_init(&params);
    params.priority = TransportShmNotifySetup_priority;

    /* make sure notify driver has been created */
    if (Notify_intLineRegistered(remoteProcId, 0)) {
        handle = TransportShmNotify_create(remoteProcId, &params, &eb);

        if (handle != NULL) {
            TransportShmNotifySetup_module->handles[remoteProcId] = handle;
            status = MessageQ_S_SUCCESS;
        }
    }

    return (status);
}

/*
 *  ======== TransportShmNotifySetup_detach ========
 */
Int TransportShmNotifySetup_detach(UInt16 remoteProcId)
{
    TransportShmNotify_Handle handle;

    handle = TransportShmNotifySetup_module->handles[remoteProcId];

    /* Trying to detach an un-attached processor should fail */
    if (handle == NULL) {
        return (MessageQ_E_FAIL);
    }

    /* Unregister the instance */
    TransportShmNotifySetup_module->handles[remoteProcId] = NULL;

    TransportShmNotify_delete(&handle);

    return (MessageQ_S_SUCCESS);
}

/*
 *  ======== TransportShmNotifySetup_isRegistered ========
 */
Bool TransportShmNotifySetup_isRegistered(UInt16 remoteProcId)
{
    Bool registered;

    registered = (TransportShmNotifySetup_module->handles[remoteProcId] != NULL);

    return (registered);
}

/*
 *  ======== TransportShmNotifySetup_sharedMemReq ========
 */
SizeT TransportShmNotifySetup_sharedMemReq(Ptr sharedAddr)
{
    return (0);
}
