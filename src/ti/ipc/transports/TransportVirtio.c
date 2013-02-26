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
 *  ======== TransportVirtio.c ========
 *
 *  Notes:
 *  - The logic in the functions for sending (_put()) and receiving _swiFxn()
 *    depend on the role (host or slave) the processor is playing in the
 *    asymmetric virtio I/O.
 *  - The host always adds *available* buffers to send/receive, while the slave
 *    always adds *used* buffers to send/receive.
 *  - The logic is summarized below:
 *
 *    Host:
 *    - Prime vq_host with avail bufs, and kick vq_host so slave can send.
 *    - To send a buffer to the slave processor:
 *          allocate a tx buffer, or get_used_buf(vq_slave);
 *               >> copy data into buf <<
 *          add_avail_buf(vq_slave);
 *          kick(vq_slave);
 *    - To receive buffer from slave processor:
 *          get_used_buf(vq_host);
 *              >> empty data from buf <<
 *          add_avail_buf(vq_host);
 *          kick(vq_host);
 *
 *    Slave:
 *    - To receive buffer from the host:
 *          get_avail_buf(vq_slave);
 *              >> empty data from buf <<
 *          add_used_buf(vq_slave);
 *          kick(vq_slave);
 *    - To send buffer to the host:
 *          get_avail_buf(vq_host);
 *              >> copy data into buf <<
 *          add_used_buf(vq_host);
 *          kick(vq_host);
 *
 */

#include <string.h>

#include <xdc/std.h>

#include <xdc/runtime/System.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Main.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/gates/GateSwi.h>

#include <ti/sdo/utils/_MultiProc.h>
#include <ti/sdo/ipc/_MessageQ.h>

#include <ti/ipc/rpmsg/virtio_ring.h>
#include <ti/ipc/rpmsg/Rpmsg.h>

/* TBD: VirtQueue.h needs to live in a common directory, not family specific.*/
#if defined(OMAPL138)
#include <ti/ipc/family/omapl138/VirtQueue.h>
#elif defined(TCI6614)
#include <ti/ipc/family/tci6614/VirtQueue.h>
#elif defined(TCI6638)
#include <ti/ipc/family/tci6638/VirtQueue.h>
#else
#error unknown processor!
#endif

#include <ti/ipc/namesrv/_NameServerRemoteRpmsg.h>

#include "_TransportVirtio.h"
#include "package/internal/TransportVirtio.xdc.h"

/* TBD: until NameMap built over a new rpmsg API: */
static VirtQueue_Handle vq_host;

/* Maximum RPMSG payload: */
#define MAX_PAYLOAD (VirtQueue_RP_MSG_BUF_SIZE - sizeof(Rpmsg_Header))

/* Addresses below this are assumed to be bound to MessageQ objects: */
#define RPMSG_RESERVED_ADDRESSES     (1024)

/* Name of the rpmsg socket on host: */
#define RPMSG_SOCKET_NAME  "rpmsg-proto"

#define FXNN "callback_usedBufReady"
static Void callback_usedBufReady(VirtQueue_Handle vq)
{
    Log_print2(Diags_INFO, FXNN": vq %d kicked; VirtQueue_isHost: 0x%x",
            VirtQueue_getId(vq), VirtQueue_isHost(vq));
    if (VirtQueue_isHost(vq))  {
        /* Post a SWI to process all incoming messages */
        Swi_post(VirtQueue_getSwiHandle(vq));
    }
    else {
        /* Note: We post nothing for vq_slave. */
       Log_print0(Diags_INFO, FXNN": Not posting SWI");
    }
}
#undef FXNN


#define FXNN "callback_availBufReady"
static Void callback_availBufReady(VirtQueue_Handle vq)
{
    Log_print2(Diags_INFO, FXNN": vq %d kicked; VirtQueue_isSlave: 0x%x",
            VirtQueue_getId(vq), VirtQueue_isSlave(vq));
    if (VirtQueue_isSlave(vq))  {
        /* Post a SWI to process all incoming messages */
        Swi_post(VirtQueue_getSwiHandle(vq));
    }
    else {
       /* Note: We post nothing for vq_host, as we assume the
        * host has already made all buffers available for slave to send.
        */
       Log_print0(Diags_INFO, FXNN": Not posting SWI");
    }
}
#undef FXNN

/* Allocate a buffer for sending: */
#define FXNN "getTxBuf"
static Void *getTxBuf(TransportVirtio_Object *obj, VirtQueue_Handle vq)
{
        Void     *buf;

        /*
         * either pick the next unused tx buffer
         * (half of our buffers are used for sending messages)
         */
        if (obj->last_sbuf < VirtQueue_RP_MSG_NUM_BUFS)  {
           Log_print1(Diags_INFO, FXNN": last_sbuf: %d", obj->last_sbuf);
           buf = (Char *)obj->sbufs + VirtQueue_RP_MSG_BUF_SIZE * obj->last_sbuf++;
        }
        else {
           /* or recycle a used one */
           buf = VirtQueue_getUsedBuf(vq);
        }
        return (buf);
}
#undef FXNN


/*  --------------  TEMP NameService over VirtQueue ----------------------- */

/* -------------- TEMP NameService over rpmsg ----------------------- */
#define FXNN "nameService_register"
static void nameService_register(UInt16 dstProc, char * name, UInt32 port, enum Rpmsg_nsFlags flags)
{
    struct Rpmsg_NsMsg nsMsg;
    UInt16 len = sizeof(nsMsg);
    UInt32 dstEndpt = RPMSG_NAMESERVICE_PORT;
    UInt32 srcEndpt = port;
    Ptr data = &nsMsg;

    strncpy(nsMsg.name, name, RPMSG_NAME_SIZE);
    nsMsg.name[RPMSG_NAME_SIZE - 1] = '\0'; /* ensure NULL termination */

    nsMsg.addr = port;
    nsMsg.flags = flags;

    Log_print3(Diags_INFO, FXNN": %sing service %s on port %d",
           (IArg)(flags == RPMSG_NS_CREATE? "creat":"destroy"),
           (IArg)name, (IArg)port);
    sendRpmsg(dstProc, dstEndpt, srcEndpt, data, len);
}
#undef FXNN

void sendRpmsg(UInt16 dstProc, UInt32 dstEndpt, UInt32 srcEndpt,
              Ptr data, UInt16 len)
{
    Int16             token = 0;
    Rpmsg_Msg             msg;
    IArg              key;

    if (dstProc != MultiProc_self()) {
        /* Send to remote processor: */
        /* Protect vring structs. */
        key = GateSwi_enter(TransportVirtio_module->gateSwiHandle);
        token = VirtQueue_getAvailBuf(vq_host, (Void **)&msg);
        GateSwi_leave(TransportVirtio_module->gateSwiHandle, key);

        if (token >= 0) {
            /* Copy the payload and set message header: */
            memcpy(msg->payload, data, len);
            msg->dataLen = len;
            msg->dstAddr = dstEndpt;
            msg->srcAddr = srcEndpt;
            msg->flags = 0;
            msg->reserved = 0;

            key = GateSwi_enter(TransportVirtio_module->gateSwiHandle);
            VirtQueue_addUsedBuf(vq_host, token);
            VirtQueue_kick(vq_host);
            GateSwi_leave(TransportVirtio_module->gateSwiHandle, key);
        }
        else {
            System_abort("sendRpmsg: getAvailBuf failed!");
        }
    }
}



/*
 *************************************************************************
 *                       Instance functions
 *************************************************************************
 */

/*
 *  ======== TransportVirtio_Instance_init ========
 *
 */
#define FXNN "TransportVirtio_Instance_init"
Int TransportVirtio_Instance_init(TransportVirtio_Object *obj,
        UInt16 remoteProcId, const TransportVirtio_Params *params,
        Error_Block *eb)
{
    Bool        flag;
    Swi_Handle  swiHandle;
    Swi_Params  swiParams;
    GateSwi_Params gatePrms;
    VirtQueue_Params vqParams;

    /* set object fields */
    obj->priority     = params->priority;
    obj->remoteProcId = remoteProcId;

    /* From the remoteProcId, we must determine if this Virtio Transport is
     * acting as host or a slave.
     */
#if 0
    /* Linux side currently assumes HOST is lowest ID core in the system:
     */
    if ((MultiProc_self() == MultiProc_getId("HOST")) ||
         ((remoteProcId != MultiProc_getId("HOST")) &&
          (MultiProc_self() < remoteProcId)))
    {
      /* This processor is Host in pair if it's own ID is the "HOST" id according
        * to the MultiProc names or if the remote processor is not the host and
        * the remote processor has an ID greater than its own ID */
      obj->isHost = TRUE;
    }
#else
    /* Hardcoded below until constraint mentioned above is lifted: */
    obj->isHost = (MultiProc_self() == MultiProc_getId("HOST"));
#endif

    Log_print2(Diags_INFO, FXNN": remoteProc: %d, isHost: %d",
                  obj->remoteProcId, obj->isHost);

    swiHandle = TransportVirtio_Instance_State_swiObj(obj);

    /* construct the Swi to process incoming messages: */
    Swi_Params_init(&swiParams);
    swiParams.arg0 = (UArg)obj;
    Swi_construct(Swi_struct(swiHandle),
                 (Swi_FuncPtr)TransportVirtio_swiFxn,
                 &swiParams, eb);

    /* Construct a GateSwi to protect our vrings: */
    GateSwi_Params_init(&gatePrms);
    TransportVirtio_module->gateSwiHandle = GateSwi_create(&gatePrms, NULL);

    /*
     * Create a pair VirtQueues (one for sending, one for receiving).
     * Note: First one gets an even, second gets odd vq ID.
     */
    VirtQueue_Params_init(&vqParams);
    vqParams.host = obj->isHost;
    vqParams.swiHandle = swiHandle;
    vqParams.intVectorId = params->intVectorId;
    if (obj->isHost)  {
      vqParams.callback = (Fxn) callback_usedBufReady;
    }
    else {
      vqParams.callback = (Fxn) callback_availBufReady;
    }

    vq_host = obj->vq_host = (Ptr)VirtQueue_create(remoteProcId, &vqParams, eb);
    obj->vq_slave  = (Ptr)VirtQueue_create(remoteProcId, &vqParams, eb);

    if (obj->isHost)  {
#if 0  /* This code is broken for multicore, and case where obj->isHost */
       /* Initialize fields used by getTxBuf(): */
            obj->sbufs = (Char *)buf_addr + VirtQueue_RP_MSG_NUM_BUFS * VirtQueue_RP_MSG_BUF_SIZE;
        obj->last_sbuf = 0;

       /* Host needs to prime his vq with some buffers for receiving: */
       for (i = 0; i < VirtQueue_RP_MSG_NUM_BUFS; i++) {
            VirtQueue_addAvailBuf(obj->vq_host,
                                  ((Char *)buf_addr + i * VirtQueue_RP_MSG_BUF_SIZE));
       }
       VirtQueue_kick(obj->vq_host);
#else
       Assert_isTrue(FALSE, NULL);
#endif
    }

    /* Plug Vring Interrupts, and wait for host ready to recv kick: */
    VirtQueue_startup(remoteProcId, obj->isHost);

    /* Announce our "MessageQ" service to other side: */
    nameService_register(remoteProcId, RPMSG_SOCKET_NAME, RPMSG_MESSAGEQ_PORT,
                         RPMSG_NS_CREATE);

    /* Register the transport with MessageQ */
    flag = ti_sdo_ipc_MessageQ_registerTransport(
        TransportVirtio_Handle_upCast(obj), remoteProcId, params->priority);

    if (flag == FALSE) {
        return (2);
    }

    return (0);
}
#undef FXNN

/*
 *  ======== TransportVirtio_Instance_finalize ========
 */
#define FXNN "TransportVirtio_Instance_finalize"
Void TransportVirtio_Instance_finalize(TransportVirtio_Object *obj, Int status)
{
    Swi_Handle  swiHandle;

    Log_print0(Diags_ENTRY, "--> "FXNN);


    /* Announce our "MessageQ" service is going away: */
    nameService_register(obj->remoteProcId, RPMSG_SOCKET_NAME,
                         RPMSG_MESSAGEQ_PORT, RPMSG_NS_DESTROY);

    /* Destruct the swi */
    swiHandle = TransportVirtio_Instance_State_swiObj(obj);
    if (swiHandle != NULL) {
      Swi_destruct(Swi_struct(swiHandle));
    }

    GateSwi_delete(&(TransportVirtio_module->gateSwiHandle));

    /* Delete the VirtQueue instance */
    if (obj->isHost) {
       VirtQueue_delete(obj->vq_slave);
    }
    else{
       VirtQueue_delete(obj->vq_host);
    }

    switch(status) {
        case 0: /* MessageQ_registerTransport succeeded */
            ti_sdo_ipc_MessageQ_unregisterTransport(obj->remoteProcId,
                obj->priority);

            /* fall thru OK */
        case 1: /* NOT USED: Notify_registerEventSingle failed */
        case 2: /* MessageQ_registerTransport failed */
            break;
    }
#undef FXNN
}

/*
 *  ======== TransportVirtio_put ========
 *
 *  Notes: In keeping with the semantics of IMessageQTransport_put(), we
 *  simply return FALSE if the remote proc has made no buffers available in the
 *  vring.
 *  Otherwise, we could block here, waiting for the remote proc to add a buffer.
 *  This implies that the remote proc must always have buffers available in the
 *  vring in order for this side to send without failing!
 *
 *  Also, this is a copy-transport, to match the Linux side rpmsg.
 */
#define FXNN "TransportVirtio_put"
Bool TransportVirtio_put(TransportVirtio_Object *obj, Ptr msg)
{
    Int          status = MessageQ_S_SUCCESS;
    UInt         msgSize;
    Int16        token = (-1);
    IArg         key;
    Rpmsg_Msg    rpMsg = NULL;

    Log_print1(Diags_ENTRY, "--> "FXNN": Entered: isHost: %d",
                 obj->isHost);

    /* Send to remote processor: */
    key = GateSwi_enter(TransportVirtio_module->gateSwiHandle);
    if (obj->isHost)  {
       rpMsg = getTxBuf(obj, obj->vq_slave);
    }
    else {
       token = VirtQueue_getAvailBuf(obj->vq_host, (Void **)&rpMsg);
    }
    GateSwi_leave(TransportVirtio_module->gateSwiHandle, key);

    if ((obj->isHost && rpMsg) || token >= 0) {
        /* Assert msg->msgSize <= vring's max fixed buffer size */
        msgSize = MessageQ_getMsgSize(msg);

        Assert_isTrue(msgSize <= MAX_PAYLOAD, NULL);

        /* Copy the payload and set message header: */
        memcpy(rpMsg->payload, (Ptr)msg, msgSize);
        rpMsg->dataLen  = msgSize;
        rpMsg->dstAddr  = (((MessageQ_Msg)msg)->dstId & 0x0000FFFF);
        rpMsg->srcAddr  = RPMSG_MESSAGEQ_PORT;
        rpMsg->flags    = 0;
        rpMsg->reserved = 0;

        /* free the app's message */
        if (((MessageQ_Msg)msg)->heapId != ti_sdo_ipc_MessageQ_STATICMSG) {
            MessageQ_free(msg);
        }

        Log_print4(Diags_INFO, FXNN": sending rpMsg: 0x%x from: %d, "
                   "to: %d, dataLen: %d",
                  (IArg)rpMsg, (IArg)rpMsg->srcAddr, (IArg)rpMsg->dstAddr,
                  (IArg)rpMsg->dataLen);

        key = GateSwi_enter(TransportVirtio_module->gateSwiHandle);
        if (obj->isHost)  {
            VirtQueue_addAvailBuf(obj->vq_slave, rpMsg);
            VirtQueue_kick(obj->vq_slave);
        }
        else {
            VirtQueue_addUsedBuf(obj->vq_host, token);
            VirtQueue_kick(obj->vq_host);
        }
        GateSwi_leave(TransportVirtio_module->gateSwiHandle, key);
    }
    else {
        status = MessageQ_E_FAIL;
        Log_print1(Diags_STATUS, FXNN": %s failed!",
                      (IArg)(obj->isHost? "getTxBuf" : "getAvailBuf"));
    }

    return (status == MessageQ_S_SUCCESS? TRUE: FALSE);
}
#undef FXNN

/*
 *  ======== TransportVirtio_control ========
 */
Bool TransportVirtio_control(TransportVirtio_Object *obj, UInt cmd,
    UArg cmdArg)
{
    return (FALSE);
}

/*
 *  ======== TransportVirtio_getStatus ========
 */
Int TransportVirtio_getStatus(TransportVirtio_Object *obj)
{
    return (0);
}

/*
 *************************************************************************
 *                       Module functions
 *************************************************************************
 */

/*
 *  ======== TransportVirtio_swiFxn ========
 *
 */
#define FXNN "TransportVirtio_swiFxn"
Void TransportVirtio_swiFxn(UArg arg0, UArg arg1)
{
    Int16             token;
    Bool              bufAdded = FALSE;
    UInt32            queueId;
    MessageQ_Msg      msg;
    MessageQ_Msg      buf = NULL;
    Rpmsg_Msg         rpMsg;
    UInt              msgSize;
    TransportVirtio_Object      *obj;
    Bool              buf_avail = FALSE;
    Rpmsg_NsMsg       * nsMsg; /* Name Service Message */
    NameServerRemote_Msg * nsrMsg;  /* Name Server Message */
    Int               nsPort = NAME_SERVER_PORT_INVALID;

    Log_print0(Diags_ENTRY, "--> "FXNN);

    obj = (TransportVirtio_Object *)arg0;

    /* Process all available buffers: */
    if (obj->isHost)  {
        rpMsg = VirtQueue_getUsedBuf(obj->vq_host);
        buf_avail = (rpMsg != NULL);
    }
    else {
        token = VirtQueue_getAvailBuf(obj->vq_slave, (Void **)&rpMsg);
        buf_avail = (token >= 0);
    }

    while (buf_avail) {
        Log_print4(Diags_INFO, FXNN": \n\tReceived rpMsg: 0x%x from: %d, "
                   "to: %d, dataLen: %d",
                  (IArg)rpMsg, (IArg)rpMsg->srcAddr, (IArg)rpMsg->dstAddr,
                  (IArg)rpMsg->dataLen);

        /* See if this is an rpmsg ns announcment... : */
        if (rpMsg->dstAddr != RPMSG_MESSAGEQ_PORT) {
            if (rpMsg->dstAddr == RPMSG_NAMESERVICE_PORT) {
                nsMsg = (Rpmsg_NsMsg *)rpMsg->payload;
                Log_print3(Diags_USER1, FXNN": ns announcement "
                  "from %d: %s, flag: %s\n",
                  nsMsg->addr, (IArg)nsMsg->name,
                  (IArg)(nsMsg->flags == RPMSG_NS_CREATE? "create":"destroy"));
                /* ... and if it is from our rpmsg-proto socket, save
                 * the rpmsg src address as the NameServer reply address:
                 */
                if (!strcmp(nsMsg->name, RPMSG_SOCKET_NAME) &&
                    rpMsg->srcAddr == NAME_SERVER_RPMSG_ADDR) {
                    if (nsMsg->flags == RPMSG_NS_CREATE) {
                        nsPort = NAME_SERVER_RPMSG_ADDR;
                    }
                    else if (nsMsg->flags  == RPMSG_NS_DESTROY) {
                        nsPort = NAME_SERVER_PORT_INVALID;
                    }
                    NameServerRemote_SetNameServerPort(nsPort);
                }
            }
            goto skip;
        }
        else if (rpMsg->srcAddr >= RPMSG_RESERVED_ADDRESSES) {
            /*
             * This could either be a NameServer request or a MessageQ.
             * Check the NameServerRemote_Msg reserved field to distinguish.
             */
            nsrMsg = (NameServerRemote_Msg *)rpMsg->payload;
            if (nsrMsg->reserved == NAMESERVER_MSG_TOKEN) {
                /* Process the NameServer request/reply message: */
                NameServerRemote_processMessage(nsrMsg);
                goto skip;
            }
        }

        /* Convert Rpmsg payload into a MessageQ_Msg: */
        msg = (MessageQ_Msg)rpMsg->payload;

        Log_print4(Diags_INFO, FXNN": \n\tmsg->heapId: %d, "
                   "msg->msgSize: %d, msg->dstId: %d, msg->msgId: %d\n",
                   msg->heapId, msg->msgSize, msg->dstId, msg->msgId);

        /* Alloc a message from msg->heapId to copy the msg */
        msgSize = MessageQ_getMsgSize(msg);
        buf = MessageQ_alloc(msg->heapId, msgSize);

        /* Make sure buf is not NULL */
        Assert_isTrue(buf != NULL, NULL);

        /* copy the message to the buffer allocated. */
        memcpy((Ptr)buf, (Ptr)msg, msgSize);

        /*
         * If the message received was statically allocated, reset the
         * heapId, so the app can free it.
         */
         if (msg->heapId == ti_sdo_ipc_MessageQ_STATICMSG)  {
             msg->heapId = 0;  /* for a copy transport, heap id is 0. */
         }

        /* get the queue id */
        queueId = MessageQ_getDstQueue(msg);

        /* Pass to destination queue: */
        MessageQ_put(queueId, buf);

skip:
        if (obj->isHost)  {
            VirtQueue_addAvailBuf(obj->vq_host, rpMsg);
        }
        else {
            VirtQueue_addUsedBuf(obj->vq_slave, token);
        }
        bufAdded = TRUE;

        /* See if there is another one: */
        if (obj->isHost)  {
            rpMsg = VirtQueue_getUsedBuf(obj->vq_host);
            buf_avail = (rpMsg != NULL);
        }
        else {
            token = VirtQueue_getAvailBuf(obj->vq_slave, (Void **)&rpMsg);
            buf_avail = (token >= 0);
        }
    }

    if (bufAdded)  {
       /* Tell host/slave we've processed the buffers: */
       VirtQueue_kick(obj->isHost? obj->vq_host: obj->vq_slave);
    }
    Log_print0(Diags_EXIT, "<-- "FXNN);
}

/*
 *  ======== TransportVirtio_setErrFxn ========
 */
Void TransportVirtio_setErrFxn(TransportVirtio_ErrFxn errFxn)
{
    /* Ignore the errFxn */
}
