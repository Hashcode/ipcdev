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
 *  ======== ping.c ========
 *
 *  Works with the ping_rpmsg HLOS test over the rpmsg-proto service.
 *  The ping_rpmsg test is in the linux/src/tests directory of this tree.
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>

#include <ti/ipc/MultiProc.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/ipc/rpmsg/NameMap.h>
#include <ti/ipc/rpmsg/MessageQCopy.h>

static UInt16 dstProc;
static MessageQCopy_Handle handle = NULL;
static UInt32 myEndpoint = 0;
static UInt32 counter = 0;

/* Send me a zero length data payload to tear down the MesssageQCopy object: */
static Void pingCallbackFxn(MessageQCopy_Handle h, UArg arg, Ptr data,
	UInt16 len, UInt32 src)
{
    const Char *reply = "Pong!";
    UInt  replyLen = strlen(reply) + 1;

    System_printf("%d: Received msg: %s from %d, len:%d\n",
                  counter++, data, src, len);

    /* Send data back to remote endpoint: */
    MessageQCopy_send(dstProc, src, myEndpoint, (Ptr)reply, replyLen);
}

Void pingTaskFxn(UArg arg0, UArg arg1)
{
    System_printf("ping_task at port %d: Entered...\n", arg0);

    /* Create the messageQ for receiving, and register callback: */
    handle = MessageQCopy_create(arg0, pingCallbackFxn, NULL, &myEndpoint);
    if (!handle) {
        System_abort("MessageQCopy_createEx failed\n");
    }

    /* Announce we are here: */
    NameMap_register("rpmsg-proto", "rpmsg-proto", arg0);

    /* Note: we don't get a chance to teardown with MessageQCopy_destroy() */
}

Int main(Int argc, char* argv[])
{
    System_printf("%s starting..\n", MultiProc_getName(MultiProc_self()));

    BIOS_start();

    return (0);
}
