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
 *  @file   MessageQ.c
 *
 *  @brief  MessageQ module "client" implementation
 *
 *  This implementation is geared for use in a "client/server" model, whereby
 *  system-wide data is maintained in a "server" component and process-
 *  specific data is handled here.  At the moment, this implementation
 *  connects and communicates with LAD for the server connection.
 */


/* Standard IPC header */
#include <ti/ipc/Std.h>

/* Linux specific header files, replacing OSAL: */
#include <pthread.h>

/* Module level headers */
#include <ti/ipc/NameServer.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>
#include <ti/ipc/MessageQ.h>
#include <_MessageQ.h>

/* Socket Headers */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* Socket Protocol Family */
#include <net/rpmsg.h>

/* Socket utils: */
#include <SocketFxns.h>

#include <ladclient.h>
#include <_lad.h>

/* =============================================================================
 * Macros/Constants
 * =============================================================================
 */

/*!
 *  @brief  Name of the reserved NameServer used for MessageQ.
 */
#define MessageQ_NAMESERVER  "MessageQ"

/*!
 *  @brief  Value of an invalid socket ID:
 */
#define Transport_INVALIDSOCKET  (0xFFFFFFFF)

/* More magic rpmsg port numbers: */
#define MESSAGEQ_RPMSG_PORT       61
#define MESSAGEQ_RPMSG_MAXSIZE   512

/* Trace flag settings: */
#define TRACESHIFT    12
#define TRACEMASK     0x1000

/* Define BENCHMARK to quiet key MessageQ APIs: */
//#define BENCHMARK

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/* structure for MessageQ module state */
typedef struct MessageQ_ModuleObject {
    Int                 refCount;
    /*!< Reference count */
    NameServer_Handle   nameServer;
    /*!< Handle to the local NameServer used for storing GP objects */
    pthread_mutex_t     gate;
    /*!< Handle of gate to be used for local thread safety */
    MessageQ_Params     defaultInstParams;
    /*!< Default instance creation parameters */
    int                 sock[MultiProc_MAXPROCESSORS];
    /*!< Sockets to for sending to each remote processor */
    int                 seqNum;
    /*!< Process-specific sequence number */
} MessageQ_ModuleObject;

/*!
 *  @brief  Structure for the Handle for the MessageQ.
 */
typedef struct MessageQ_Object_tag {
    MessageQ_Params         params;
    /*! Instance specific creation parameters */
    MessageQ_QueueId        queue;
    /* Unique id */
    int                     fd[MultiProc_MAXPROCESSORS];
    /* File Descriptor to block on messages from remote processors. */
    int                     unblockFd;
    /* Write this fd to unblock the select() call in MessageQ _get() */
    void                    *serverHandle;
} MessageQ_Object;

static Bool verbose = FALSE;


/* =============================================================================
 *  Globals
 * =============================================================================
 */
static MessageQ_ModuleObject MessageQ_state =
{
    .refCount               = 0,
    .nameServer             = NULL,
};

/*!
 *  @var    MessageQ_module
 *
 *  @brief  Pointer to the MessageQ module state.
 */
MessageQ_ModuleObject * MessageQ_module = &MessageQ_state;


/* =============================================================================
 * Forward declarations of internal functions
 * =============================================================================
 */

/* This is a helper function to initialize a message. */
static Int transportCreateEndpoint(int * fd, UInt16 rprocId, UInt16 queueIndex);
static Int transportCloseEndpoint(int fd);
static Int transportGet(int sock, MessageQ_Msg * retMsg);
static Int transportPut(MessageQ_Msg msg, UInt16 dstId, UInt16 dstProcId);

/* =============================================================================
 * APIS
 * =============================================================================
 */
/* Function to get default configuration for the MessageQ module.
 *
 */
Void MessageQ_getConfig (MessageQ_Config * cfg)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    assert (cfg != NULL);

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "MessageQ_getConfig: can't find connection to daemon for pid %d\n",
           getpid())

        return;
    }

    cmd.cmd = LAD_MESSAGEQ_GETCONFIG;
    cmd.clientId = handle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "MessageQ_getConfig: sending LAD command failed, status=%d\n", status)
        return;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("MessageQ_getConfig: no LAD response, status=%d\n", status)
        return;
    }
    status = rsp.messageQGetConfig.status;

    PRINTVERBOSE2(
      "MessageQ_getConfig: got LAD response for client %d, status=%d\n",
      handle, status)

    memcpy(cfg, &rsp.messageQGetConfig.cfg, sizeof(*cfg));

    return;
}

/* Function to setup the MessageQ module. */
Int MessageQ_setup(const MessageQ_Config * cfg)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;
    Int i;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "MessageQ_setup: can't find connection to daemon for pid %d\n",
           getpid())

        return MessageQ_E_RESOURCE;
    }

    cmd.cmd = LAD_MESSAGEQ_SETUP;
    cmd.clientId = handle;
    memcpy(&cmd.args.messageQSetup.cfg, cfg, sizeof(*cfg));

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "MessageQ_setup: sending LAD command failed, status=%d\n", status)
        return MessageQ_E_FAIL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("MessageQ_setup: no LAD response, status=%d\n", status)
        return(status);
    }
    status = rsp.setup.status;

    PRINTVERBOSE2(
      "MessageQ_setup: got LAD response for client %d, status=%d\n",
      handle, status)

    MessageQ_module->nameServer = rsp.setup.nameServerHandle;
    MessageQ_module->seqNum = 0;

    /* Create a default local gate. */
    pthread_mutex_init (&(MessageQ_module->gate), NULL);

    /* Clear sockets array. */
    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        MessageQ_module->sock[i] = Transport_INVALIDSOCKET;
    }

    return status;
}

/*
 * Function to destroy the MessageQ module.
 * Destroys socket/protocol maps;  sockets themselves should have been
 * destroyed in MessageQ_delete() and MessageQ_detach() calls.
 */
Int MessageQ_destroy (void)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "MessageQ_destroy: can't find connection to daemon for pid %d\n",
           getpid())

        return MessageQ_E_RESOURCE;
    }

    cmd.cmd = LAD_MESSAGEQ_DESTROY;
    cmd.clientId = handle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "MessageQ_destroy: sending LAD command failed, status=%d\n", status)
        return MessageQ_E_FAIL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("MessageQ_destroy: no LAD response, status=%d\n", status)
        return(status);
    }
    status = rsp.status;

    PRINTVERBOSE2(
      "MessageQ_destroy: got LAD response for client %d, status=%d\n",
      handle, status)

    return status;
}

/* Function to initialize the parameters for the MessageQ instance. */
Void MessageQ_Params_init (MessageQ_Params * params)
{
    memcpy (params, &(MessageQ_module->defaultInstParams),
            sizeof (MessageQ_Params));

    return;
}

/*
 *   Function to create a MessageQ object for receiving.
 *
 *   Create a socket and bind the source address (local ProcId/MessageQ ID) in
 *   order to get messages dispatched to this messageQ.
 */
MessageQ_Handle MessageQ_create (String name, const MessageQ_Params * params)
{
    Int                   status;
    MessageQ_Object *     obj    = NULL;
    UInt16                queueIndex = 0u;
    UInt16                procId;
    UInt16                rprocId;
    LAD_ClientHandle      handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "MessageQ_create: can't find connection to daemon for pid %d\n",
           getpid())

        return NULL;
    }

    cmd.cmd = LAD_MESSAGEQ_CREATE;
    cmd.clientId = handle;
    strncpy(cmd.args.messageQCreate.name, name,
            LAD_MESSAGEQCREATEMAXNAMELEN - 1);
    cmd.args.messageQCreate.name[LAD_MESSAGEQCREATEMAXNAMELEN - 1] = '\0';
    if (params) {
        memcpy(&cmd.args.messageQCreate.params, params, sizeof(*params));
    }

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "MessageQ_create: sending LAD command failed, status=%d\n", status)
        return NULL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("MessageQ_create: no LAD response, status=%d\n", status)
        return NULL;
    }
    status = rsp.messageQCreate.status;

    PRINTVERBOSE2(
      "MessageQ_create: got LAD response for client %d, status=%d\n",
      handle, status)

    if (status == -1) {
       PRINTVERBOSE1(
          "MessageQ_create: MessageQ server operation failed, status=%d\n",
          status)
       return NULL;
    }

    /* Create the generic obj */
    obj = (MessageQ_Object *)calloc(1, sizeof (MessageQ_Object));

    if (params != NULL) {
       /* Populate the params member */
        memcpy((Ptr) &obj->params, (Ptr)params, sizeof (MessageQ_Params));
    }

    procId = MultiProc_self();
    queueIndex = (MessageQ_QueueIndex)rsp.messageQCreate.queueId;
    obj->queue = rsp.messageQCreate.queueId;
    obj->serverHandle = rsp.messageQCreate.serverHandle;

    /*
     * Create a set of communication endpoints (one per each remote proc),
     * and return the socket as target for MessageQ_put() calls, and as
     * a file descriptor to close during MessageQ_delete().
     */
    for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
        obj->fd[rprocId] = Transport_INVALIDSOCKET;
        if (procId == rprocId) {
            /* Skip creating an endpoint for ourself. */
            continue;
        }

        PRINTVERBOSE3("MessageQ_create: creating endpoint for: %s, rprocId: %d, queueIndex: %d\n", name, rprocId, queueIndex)

        status = transportCreateEndpoint(&obj->fd[rprocId], rprocId,
                                           queueIndex);
        if (status < 0) {
           obj->fd[rprocId] = Transport_INVALIDSOCKET;
        }
    }

    /*
     * Now, to support MessageQ_unblock() functionality, create an event object.
     * Writing to this event will unblock the select() call in MessageQ_get().
     */
    obj->unblockFd = eventfd(0, 0);
    if (obj->unblockFd == -1)  {
        printf ("MessageQ_create: eventfd creation failed: %d, %s\n",
                   errno, strerror(errno));
        MessageQ_delete((MessageQ_Handle *)&obj);
    }
    else {
        int endpointFound = 0;

        for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
            if (obj->fd[rprocId] != Transport_INVALIDSOCKET) {
                endpointFound = 1;
            }
        }
        if (!endpointFound) {
            printf("MessageQ_create: no transport endpoints found, deleting\n");
            MessageQ_delete((MessageQ_Handle *)&obj);
        }
    }

    return ((MessageQ_Handle) obj);
}

/*
 * Function to delete a MessageQ object for a specific slave processor.
 *
 * Deletes the socket associated with this MessageQ object.
 */
Int MessageQ_delete (MessageQ_Handle * handlePtr)
{
    Int               status    = MessageQ_S_SUCCESS;
    MessageQ_Object * obj       = NULL;
    UInt16            rprocId;
    LAD_ClientHandle      handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "MessageQ_delete: can't find connection to daemon for pid %d\n",
           getpid())

        return MessageQ_E_FAIL;
    }

    obj = (MessageQ_Object *) (*handlePtr);

    cmd.cmd = LAD_MESSAGEQ_DELETE;
    cmd.clientId = handle;
    cmd.args.messageQDelete.serverHandle = obj->serverHandle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "MessageQ_delete: sending LAD command failed, status=%d\n", status)
        return MessageQ_E_FAIL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("MessageQ_delete: no LAD response, status=%d\n", status)
        return MessageQ_E_FAIL;
    }
    status = rsp.messageQDelete.status;

    PRINTVERBOSE2(
      "MessageQ_delete: got LAD response for client %d, status=%d\n",
      handle, status)


    /* Close the event used for MessageQ_unblock(): */
    close(obj->unblockFd);

    /* Close the communication endpoint: */
    for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
        if (obj->fd[rprocId] != Transport_INVALIDSOCKET) {
            status = transportCloseEndpoint(obj->fd[rprocId]);
        }
    }

    /* Now free the obj */
    free (obj);
    *handlePtr = NULL;

    return (status);
}

/*
 *  Opens an instance of MessageQ for sending.
 *
 *  We need not create a socket here; the sockets for all remote processors
 *  were created during MessageQ_attach(), and will be
 *  retrieved during MessageQ_put().
 */
Int MessageQ_open (String name, MessageQ_QueueId * queueId)
{
    Int status = MessageQ_S_SUCCESS;

    status = NameServer_getUInt32 (MessageQ_module->nameServer,
                                     name, queueId, NULL);

    if (status == NameServer_E_NOTFOUND) {
        /* Set return queue ID to invalid. */
        *queueId = MessageQ_INVALIDMESSAGEQ;
        status = MessageQ_E_NOTFOUND;
    }
    else if (status >= 0) {
        /* Override with a MessageQ status code. */
        status = MessageQ_S_SUCCESS;
    }
    else {
        /* Set return queue ID to invalid. */
        *queueId = MessageQ_INVALIDMESSAGEQ;
        /* Override with a MessageQ status code. */
        if (status == NameServer_E_TIMEOUT) {
            status = MessageQ_E_TIMEOUT;
        }
        else {
            status = MessageQ_E_FAIL;
        }
    }

    return (status);
}

/* Closes previously opened instance of MessageQ module. */
Int MessageQ_close (MessageQ_QueueId * queueId)
{
    Int32 status = MessageQ_S_SUCCESS;

    /* Nothing more to be done for closing the MessageQ. */
    *queueId = MessageQ_INVALIDMESSAGEQ;

    return (status);
}

/*
 * Place a message onto a message queue.
 *
 * Calls TransportShm_put(), which handles the sending of the message using the
 * appropriate kernel interface (socket, device ioctl) call for the remote
 * procId encoded in the queueId argument.
 *
 */
Int MessageQ_put (MessageQ_QueueId queueId, MessageQ_Msg msg)
{
    Int      status;
    UInt16   dstProcId  = (UInt16)(queueId >> 16);
    UInt16   queueIndex = (MessageQ_QueueIndex)(queueId & 0x0000ffff);

    msg->dstId     = queueIndex;
    msg->dstProc   = dstProcId;

    status = transportPut(msg, queueIndex, dstProcId);

    return (status);
}

/*
 * Gets a message for a message queue and blocks if the queue is empty.
 * If a message is present, it returns it.  Otherwise it blocks
 * waiting for a message to arrive.
 * When a message is returned, it is owned by the caller.
 *
 * We block using select() on the receiving socket's file descriptor, then
 * get the waiting message via the socket API recvfrom().
 * We use the socket stored in the messageQ object via a previous call to
 * MessageQ_create().
 *
 */
Int MessageQ_get (MessageQ_Handle handle, MessageQ_Msg * msg ,UInt timeout)
{
    Int     status = MessageQ_S_SUCCESS;
    Int     tmpStatus;
    MessageQ_Object * obj = (MessageQ_Object *) handle;
    int     retval;
    int     nfds;
    fd_set  rfds;
    struct  timeval tv;
    void    *timevalPtr;
    UInt16  rprocId;
    int     maxfd = 0;

    /* Wait (with timeout) and retreive message from socket: */
    FD_ZERO(&rfds);
    for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
        if (rprocId == MultiProc_self() ||
            obj->fd[rprocId] == Transport_INVALIDSOCKET) {
            continue;
        }
        maxfd = MAX(maxfd, obj->fd[rprocId]);
        FD_SET(obj->fd[rprocId], &rfds);
    }

    /* Wait also on the event fd, which may be written by MessageQ_unblock(): */
    FD_SET(obj->unblockFd, &rfds);

    if (timeout == MessageQ_FOREVER) {
        timevalPtr = NULL;
    }
    else {
        /* Timeout given in msec: convert:  */
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        timevalPtr = &tv;
    }
    /* Add one to last fd created: */
    nfds = MAX(maxfd, obj->unblockFd) + 1;

    retval = select(nfds, &rfds, NULL, NULL, timevalPtr);
    if (retval)  {
        if (FD_ISSET(obj->unblockFd, &rfds))  {
            /*
             * Our event was signalled by MessageQ_unblock().
             *
             * This is typically done during a shutdown sequence, where
             * the intention of the client would be to ignore (i.e. not fetch)
             * any pending messages in the transport's queue.
             * Thus, we shall not check for nor return any messages.
             */
            *msg = NULL;
            status = MessageQ_E_UNBLOCKED;
        }
        else {
            for (rprocId = 0; rprocId < MultiProc_getNumProcessors();
                 rprocId++) {
                if (rprocId == MultiProc_self() ||
                    obj->fd[rprocId] == Transport_INVALIDSOCKET) {
                    continue;
                }
                if (FD_ISSET(obj->fd[rprocId], &rfds)) {
                    /* Our transport's fd was signalled: Get the message: */
                    tmpStatus = transportGet(obj->fd[rprocId], msg);
                    if (tmpStatus < 0) {
                        printf ("MessageQ_get: tranposrtshm_get failed.");
                        status = MessageQ_E_FAIL;
                    }
                }
            }
        }
    }
    else if (retval == 0) {
        *msg = NULL;
        status = MessageQ_E_TIMEOUT;
    }

    return (status);
}

/*
 * Return a count of the number of messages in the queue
 *
 * TBD: Implement as a socket ioctl, using getsockopt().  Return -1 for now.
 */
Int MessageQ_count (MessageQ_Handle handle)
{
    Int               count = -1;
#if 0
    MessageQ_Object * obj   = (MessageQ_Object *) handle;
    socklen_t         optlen;

    /*
     * TBD: Need to find a way to implement (if anyone uses it!), and
     * push down into transport..
     */

    /*
     * 2nd arg to getsockopt should be transport independent, but using
     *  SSKPROTO_SHMFIFO for now:
     */
    getsockopt(obj->fd, SSKPROTO_SHMFIFO, SSKGETOPT_GETMSGQCOUNT,
                 &count, &optlen);
#endif

    return (count);
}

/* Initializes a message not obtained from MessageQ_alloc. */
Void MessageQ_staticMsgInit (MessageQ_Msg msg, UInt32 size)
{
    /* Fill in the fields of the message */
    MessageQ_msgInit (msg);
    msg->heapId  = MessageQ_STATICMSG;
    msg->msgSize = size;
}

/*
 * Allocate a message and initialize the needed fields (note some
 * of the fields in the header are set via other APIs or in the
 * MessageQ_put function,
 */
MessageQ_Msg MessageQ_alloc (UInt16 heapId, UInt32 size)
{
    MessageQ_Msg msg       = NULL;

    /*
     * heapId not used for local alloc (as this is over a copy transport), but
     * we need to send to other side as heapId is used in BIOS transport:
     */
    msg = (MessageQ_Msg)calloc (1, size);
    MessageQ_msgInit (msg);
    msg->msgSize = size;
    msg->heapId  = heapId;

    return msg;
}

/* Frees the message back to the heap that was used to allocate it. */
Int MessageQ_free (MessageQ_Msg msg)
{
    UInt32         status = MessageQ_S_SUCCESS;

    /* Check to ensure this was not allocated by user: */
    if (msg->heapId == MessageQ_STATICMSG)  {
        status =  MessageQ_E_CANNOTFREESTATICMSG;
    }
    else {
        free (msg);
    }

    return status;
}

/* Register a heap with MessageQ. */
Int MessageQ_registerHeap (Ptr heap, UInt16 heapId)
{
    Int  status = MessageQ_S_SUCCESS;

    /* Do nothing, as this uses a copy transport: */

    return status;
}

/* Unregister a heap with MessageQ. */
Int MessageQ_unregisterHeap (UInt16 heapId)
{
    Int  status = MessageQ_S_SUCCESS;

    /* Do nothing, as this uses a copy transport: */

    return status;
}

/* Unblocks a MessageQ */
Void MessageQ_unblock (MessageQ_Handle handle)
{
    MessageQ_Object * obj   = (MessageQ_Object *) handle;
    uint64_t     buf = 1;
    int          numBytes;

    /* Write 8 bytes to awaken any threads blocked on this messageQ: */
    numBytes = write(obj->unblockFd, &buf, sizeof(buf));
}

/* Embeds a source message queue into a message. */
Void MessageQ_setReplyQueue (MessageQ_Handle handle, MessageQ_Msg msg)
{
    MessageQ_Object * obj   = (MessageQ_Object *) handle;

    msg->replyId   = (UInt16)(obj->queue);
    msg->replyProc = (UInt16)(obj->queue >> 16);
}

/* Returns the QueueId associated with the handle. */
MessageQ_QueueId MessageQ_getQueueId (MessageQ_Handle handle)
{
    MessageQ_Object * obj = (MessageQ_Object *) handle;
    UInt32            queueId;

    queueId = (obj->queue);

    return queueId;
}

/* Sets the tracing of a message */
Void MessageQ_setMsgTrace (MessageQ_Msg msg, Bool traceFlag)
{
    msg->flags = (msg->flags & ~TRACEMASK) |   (traceFlag << TRACESHIFT);
}

/*
 *  Returns the amount of shared memory used by one transport instance.
 *
 *  The MessageQ module itself does not use any shared memory but the
 *  underlying transport may use some shared memory.
 */
SizeT MessageQ_sharedMemReq (Ptr sharedAddr)
{
    SizeT memReq = 0u;

    /* Do nothing, as this is a copy transport. */

    return (memReq);
}

/*
 *  Create a socket for this remote proc, and attempt to connect.
 *
 *  Only creates a socket if one does not already exist for this procId.
 *
 *  Note: remoteProcId may be MultiProc_Self() for loopback case.
 */
Int MessageQ_attach (UInt16 remoteProcId, Ptr sharedAddr)
{
    Int     status = MessageQ_S_SUCCESS;
    int     sock;

    PRINTVERBOSE1("MessageQ_attach: remoteProcId: %d\n", remoteProcId)

    if (remoteProcId >= MultiProc_MAXPROCESSORS) {
        status = MessageQ_E_INVALIDPROCID;
        goto exit;
    }

    pthread_mutex_lock (&(MessageQ_module->gate));

    /* Only create a socket if one doesn't exist: */
    if (MessageQ_module->sock[remoteProcId] == Transport_INVALIDSOCKET)  {
        /* Create the socket for sending messages to the remote proc: */
        sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
        if (sock < 0) {
            status = MessageQ_E_FAIL;
            printf ("MessageQ_attach: socket failed: %d, %s\n",
                       errno, strerror(errno));
        }
        else  {
            PRINTVERBOSE1("MessageQ_attach: created send socket: %d\n", sock)
            MessageQ_module->sock[remoteProcId] = sock;
            /* Attempt to connect: */
            status = ConnectSocket(sock, remoteProcId, MESSAGEQ_RPMSG_PORT);
            if (status < 0) {
                status = MessageQ_E_RESOURCE;
                /* don't hard-printf since this is no longer fatal */
                PRINTVERBOSE1("MessageQ_attach: ConnectSocket(remoteProcId:%d) failed\n",
                       remoteProcId);
            }
        }
    }
    else {
        status = MessageQ_E_ALREADYEXISTS;
    }

    pthread_mutex_unlock (&(MessageQ_module->gate));

    if (status == MessageQ_E_RESOURCE) {
        MessageQ_detach(remoteProcId);
    }

exit:
    return (status);
}

/*
 *  Close the socket for this remote proc.
 *
 */
Int MessageQ_detach (UInt16 remoteProcId)
{
    Int status = MessageQ_S_SUCCESS;
    int sock;

    if (remoteProcId >= MultiProc_MAXPROCESSORS) {
        status = MessageQ_E_INVALIDPROCID;
        goto exit;
    }

    pthread_mutex_lock (&(MessageQ_module->gate));

    sock = MessageQ_module->sock[remoteProcId];
    if (sock != Transport_INVALIDSOCKET) {
        if (close(sock)) {
            status = MessageQ_E_OSFAILURE;
            printf("MessageQ_detach: close failed: %d, %s\n",
                   errno, strerror(errno));
        }
        else {
            PRINTVERBOSE1("MessageQ_detach: closed socket: %d\n", sock)
            MessageQ_module->sock[remoteProcId] = Transport_INVALIDSOCKET;
        }
    }

    pthread_mutex_unlock (&(MessageQ_module->gate));

exit:
    return (status);
}

/*
 * This is a helper function to initialize a message.
 */
Void MessageQ_msgInit (MessageQ_Msg msg)
{
#if 0
    Int                 status    = MessageQ_S_SUCCESS;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "MessageQ_msgInit: can't find connection to daemon for pid %d\n",
           getpid())

        return;
    }

    cmd.cmd = LAD_MESSAGEQ_MSGINIT;
    cmd.clientId = handle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "MessageQ_msgInit: sending LAD command failed, status=%d\n", status)
        return;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("MessageQ_msgInit: no LAD response, status=%d\n", status)
        return;
    }
    status = rsp.msgInit.status;

    PRINTVERBOSE2(
      "MessageQ_msgInit: got LAD response for client %d, status=%d\n",
      handle, status)

    memcpy(msg, &rsp.msgInit.msg, sizeof(*msg));
#else
    msg->reserved0 = 0;  /* We set this to distinguish from NameServerMsg */
    msg->replyId   = (UInt16)MessageQ_INVALIDMESSAGEQ;
    msg->msgId     = MessageQ_INVALIDMSGID;
    msg->dstId     = (UInt16)MessageQ_INVALIDMESSAGEQ;
    msg->flags     = MessageQ_HEADERVERSION | MessageQ_NORMALPRI;
    msg->srcProc   = MultiProc_self();

    pthread_mutex_lock(&(MessageQ_module->gate));
    msg->seqNum  = MessageQ_module->seqNum++;
    pthread_mutex_unlock(&(MessageQ_module->gate));
#endif
}

/*
 * =============================================================================
 * Transport: Fxns kept here until need for a transport layer is realized.
 * =============================================================================
 */
/*
 * ======== transportCreateEndpoint ========
 *
 * Create a communication endpoint to receive messages.
 */
static Int transportCreateEndpoint(int * fd, UInt16 rprocId, UInt16 queueIndex)
{
    Int          status    = MessageQ_S_SUCCESS;
    int         err;

    /*  Create the socket to receive messages for this messageQ. */
    *fd = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
    if (*fd < 0) {
        status = MessageQ_E_FAIL;
        printf ("transportCreateEndpoint: socket call failed: %d, %s\n",
                  errno, strerror(errno));
        goto exit;
    }

    PRINTVERBOSE1("transportCreateEndpoint: created socket: fd: %d\n", *fd)

    err = SocketBindAddr(*fd, rprocId, (UInt32)queueIndex);
    if (err < 0) {
        status = MessageQ_E_FAIL;
        /* don't hard-printf since this is no longer fatal */
        PRINTVERBOSE2("transportCreateEndpoint: bind failed: %d, %s\n",
                      errno, strerror(errno));
    }

exit:
    return (status);
}

/*
 * ======== transportCloseEndpoint ========
 *
 *  Close the communication endpoint.
 */
static Int transportCloseEndpoint(int fd)
{
    Int status = MessageQ_S_SUCCESS;

    PRINTVERBOSE1("transportCloseEndpoint: closing socket: %d\n", fd)

    /* Stop communication to this socket:  */
    close(fd);

    return (status);
}

/*
 * ======== transportGet ========
 *  Retrieve a message waiting in the socket's queue.
*/
static Int transportGet(int sock, MessageQ_Msg * retMsg)
{
    Int           status    = MessageQ_S_SUCCESS;
    MessageQ_Msg  msg;
    struct sockaddr_rpmsg fromAddr;  // [Socket address of sender]
    unsigned int  len;
    int           byteCount;

    /*
     * We have no way of peeking to see what message size we'll get, so we
     * allocate a message of max size to receive contents from the rpmsg socket
     * (currently, a copy transport)
     */
    msg = MessageQ_alloc (0, MESSAGEQ_RPMSG_MAXSIZE);
    if (!msg)  {
        status = MessageQ_E_MEMORY;
        goto exit;
    }

    memset(&fromAddr, 0, sizeof(fromAddr));
    len = sizeof(fromAddr);

    byteCount = recvfrom(sock, msg, MESSAGEQ_RPMSG_MAXSIZE, 0,
                      (struct sockaddr *)&fromAddr, &len);
    if (len != sizeof(fromAddr)) {
        printf("recvfrom: got bad addr len (%d)\n", len);
        status = MessageQ_E_FAIL;
        goto exit;
    }
    if (byteCount < 0) {
        printf("recvfrom failed: %s (%d)\n", strerror(errno), errno);
        status = MessageQ_E_FAIL;
        goto exit;
    }
    else {
         /* Update the allocated message size (even though this may waste space
          * when the actual message is smaller than the maximum rpmsg size,
          * the message will be freed soon anyway, and it avoids an extra copy).
          */
         msg->msgSize = byteCount;

         /*
          * If the message received was statically allocated, reset the
          * heapId, so the app can free it.
          */
         if (msg->heapId == MessageQ_STATICMSG)  {
             msg->heapId = 0;  /* for a copy transport, heap id is 0. */
         }
    }

    PRINTVERBOSE1("transportGet: recvfrom socket: fd: %d\n", sock)
    PRINTVERBOSE3("\tReceived a msg: byteCount: %d, rpmsg addr: %d, rpmsg proc: %d\n", byteCount, fromAddr.addr, fromAddr.vproc_id)
    PRINTVERBOSE2("\tMessage Id: %d, Message size: %d\n", msg->msgId, msg->msgSize)

    *retMsg = msg;

exit:
    return (status);
}

/*
 * ======== transportPut ========
 *
 * Calls the socket API sendto() on the socket associated with
 * with this destination procID.
 * Currently, both local and remote messages are sent via the Socket ABI, so
 * no local object lists are maintained here.
*/
static Int transportPut(MessageQ_Msg msg, UInt16 dstId, UInt16 dstProcId)
{
    Int     status    = MessageQ_S_SUCCESS;
    int     sock;
    int     err;

    /*
     * Retrieve the socket for the AF_SYSLINK protocol associated with this
     * transport.
     */
    sock = MessageQ_module->sock[dstProcId];

    PRINTVERBOSE2("Sending msgId: %d via sock: %d\n", msg->msgId, sock)

    err = send(sock, msg, msg->msgSize, 0);
    if (err < 0) {
        printf ("transportPut: send failed: %d, %s\n",
                  errno, strerror(errno));
        status = MessageQ_E_FAIL;
    }

    /*
     * Free the message, as this is a copy transport, we maintain MessageQ
     * semantics.
     */
    MessageQ_free (msg);

    return (status);
}
