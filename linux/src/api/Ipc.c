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
/*!
 *  @file       Ipc.c
 *
 *  @brief      Starts and stops user side Ipc
 *              All setup/destroy APIs on user side will be call from this
 *              module.
 *
 *  @ver        0002
 *
 */

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* Common IPC headers: */
#include <ti/ipc/Std.h>
#include <ti/ipc/Ipc.h>
#include <ti/ipc/NameServer.h>

/* User side headers */
#include <ladclient.h>

/* IPC startup/shutdown stuff: */
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>
#include <_MessageQ.h>
#include <_NameServer.h>

static LAD_ClientHandle ladHandle;

static void cleanup(int arg);

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
/* Function to start Ipc */
Int Ipc_start (Void)
{
    MessageQ_Config   msgqCfg;
    MultiProc_Config  mpCfg;
    Int32             status;
    LAD_Status        ladStatus;
    UInt16            rprocId;
    Int32             attachedAny = 0;

    /* Catch ctrl-C, and cleanup: */
    (void) signal(SIGINT, cleanup);

    ladStatus = LAD_connect(&ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("Ipc_start: LAD_connect() failed: %d\n", ladStatus);
        status = Ipc_E_FAIL;
        goto exit;
    }

    /* 
     * Get MultiProc configuration from LAD and initialize local MultiProc
     * config structure.
     */
    MultiProc_getConfig(&mpCfg);
    _MultiProc_initCfg(&mpCfg);

    status = NameServer_setup();
    if (status >= 0) {
        MessageQ_getConfig(&msgqCfg);
        MessageQ_setup(&msgqCfg);

        /* Now attach to all remote processors, assuming they are up. */
        for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
            if (0 == rprocId) {
                /* Skip host, which should always be 0th entry. */
                continue;
            }
            status = MessageQ_attach(rprocId, NULL);
            if (status == MessageQ_E_RESOURCE) {
                continue;
            }
            if (status < 0) {
                printf("Ipc_start: MessageQ_attach(%d) failed: %d\n",
                       rprocId, status);
                status = Ipc_E_FAIL;

                break;
            }
            else {
                attachedAny = 1;
            }
        }
        if (attachedAny) {
            status = Ipc_S_SUCCESS;
        }
    }
    else {
        printf("Ipc_start: NameServer_setup() failed: %d\n", status);
        status = Ipc_E_FAIL;
    }

exit:
    return (status);
}


/* Function to stop Ipc */
Int Ipc_stop (Void)
{
    Int32             status = Ipc_S_SUCCESS;
    LAD_Status        ladStatus;
    UInt16            rprocId;

    /* Now detach from all remote processors, assuming they are up. */
    for (rprocId = 0;
         (rprocId < MultiProc_getNumProcessors()) && (status >= 0);
         rprocId++) {
        if (0 == rprocId) {
          /* Skip host, which should always be 0th entry. */
          continue;
        }
        status = MessageQ_detach(rprocId);
        if (status < 0) {
            printf("Ipc_stop: MessageQ_detach(%d) failed: %d\n",
                rprocId, status);
            status = Ipc_E_FAIL;
            goto exit;
       }
    }

    status = MessageQ_destroy();
    if (status < 0) {
        printf("Ipc_stop: MessageQ_destroy() failed: %d\n", status);
        status = Ipc_E_FAIL;
        goto exit;
    }

    status = NameServer_destroy();
    if (status < 0) {
        printf("Ipc_stop: NameServer_destroy() failed: %d\n", status);
        status = Ipc_E_FAIL;
        goto exit;
    }

    ladStatus = LAD_disconnect(ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("LAD_disconnect() failed: %d\n", ladStatus);
        status = Ipc_E_FAIL;
        goto exit;
    }

exit:
    return (status);
}

static void cleanup(int arg)
{
    printf("Ipc: Caught SIGINT, calling Ipc_stop...\n");
    Ipc_stop();
    exit(0);
}
