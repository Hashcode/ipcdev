/*
 *  @file       rpmsg-omxdrv.c
 *
 *  @brief      devctl handler for OMX component.
 *
 *
 *  @ver        02.00.00.46_alpha1
 *
 *  ============================================================================
 *
 *  Copyright (c) 2010-2011, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  Contact information for paper mail:
 *  Texas Instruments
 *  Post Office Box 655303
 *  Dallas, Texas 75265
 *  Contact information:
 *  http://www-k.ext.ti.com/sc/technical-support/product-information-centers.htm?
 *  DCMP=TIHomeTracking&HQS=Other+OT+home_d_contact
 *  ============================================================================
 *
 */


/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/List.h>
#include <ti/syslink/utils/String.h>
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/GateSpinlock.h>
#include <_MultiProc.h>

/*QNX specific header include */
#include <errno.h>
#include <unistd.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/netmgr.h>
#include <devctl.h>

/* Module headers */
#include <ti/ipc/rpmsg_omx.h>
#include <ti/ipc/MessageQCopy.h>
#include <_MessageQCopy.h>
#include <_MessageQCopyDefs.h>
#include "OsalSemaphore.h"
#include "std_qnx.h"
#include <pthread.h>

#include <memmgr/tilermem.h>
#include <memmgr/tiler.h>

#include "rpmsg-omxdrv.h"
#include <rpmsg.h>

#define PRIORITY_REALTIME_LOW 29

extern int mem_offset64_peer(pid_t pid, const uintptr_t addr, size_t len,
                             off64_t *offset, size_t *contig_len);

static MsgList_t *nl_cache;
static int num_nl = 0;
static WaitingReaders_t *wr_cache;
static int num_wr = 0;

/*
 * Instead of constantly allocating and freeing the notifier structures
 * we just cache a few of them, and recycle them instead.
 * The cache count is set with CACHE_NUM in rpmsg-omxdrv.h.
 */

static MsgList_t *get_nl()
{
    MsgList_t *item;
    item = nl_cache;
    if (item != NULL) {
        nl_cache = nl_cache->next;
        num_nl--;
    } else {
        item = Memory_alloc(NULL, sizeof(MsgList_t), 0, NULL);
    }
    return(item);
}

static void put_nl(MsgList_t *item)
{
    if (num_nl >= CACHE_NUM) {
        Memory_free(NULL, item, sizeof(*item));
    } else {
        item->next = nl_cache;
        nl_cache = item;
        num_nl++;
    }
    return;
}

static WaitingReaders_t *get_wr()
{
    WaitingReaders_t *item;
    item = wr_cache;
    if (item != NULL) {
        wr_cache = wr_cache->next;
        num_wr--;
    } else {
        item = Memory_alloc(NULL, sizeof(WaitingReaders_t), 0, NULL);
    }
    return(item);
}

static void put_wr(WaitingReaders_t *item)
{
    if (num_wr >= CACHE_NUM) {
        Memory_free(NULL, item, sizeof(*item));
    } else {
        item->next = wr_cache;
        wr_cache = item;
        num_wr++;
    }
    return;
}

typedef enum RPC_OMX_MAP_INFO_TYPE
{
RPC_OMX_MAP_INFO_NONE = 0,
RPC_OMX_MAP_INFO_ONE_BUF = 1,
RPC_OMX_MAP_INFO_TWO_BUF = 2,
RPC_OMX_MAP_INFO_THREE_BUF = 3,
RPC_OMX_MAP_INFO_MAX = 0x7FFFFFFF
} RPC_OMX_MAP_INFO_TYPE;

/* structure to hold rpmsg-omx device information */
typedef struct named_device {
    iofunc_mount_t      mattr;
    iofunc_attr_t       cattr;
    int                 resmgr_id;
    pthread_mutex_t     mutex;
    iofunc_funcs_t      mfuncs;
    resmgr_connect_funcs_t  cfuncs;
    resmgr_io_funcs_t   iofuncs;
    char device_name[_POSIX_PATH_MAX];
} named_device_t;

/* rpmsg-omx device structure */
typedef struct rpmsg_omx_dev {
    dispatch_t       * dpp;
    thread_pool_t    * tpool;
    named_device_t     rpmsg_omx;
} rpmsg_omx_dev_t;

/*!
 *  @brief  Remote connection object
 */
typedef struct rpmsg_omx_conn_object {
    rpmsg_omx_dev_t *   dev;
    MessageQCopy_Handle mq;
    UInt32              addr;
    UInt16              procId;
    ProcMgr_Handle      procH;
} rpmsg_omx_conn_object;

/*!
 *  @brief  omx instance object
 */
typedef struct rpmsg_omx_object_tag {
    MessageQCopy_Handle     mq;
    rpmsg_omx_conn_object * conn;
    UInt32                  addr;
    UInt32                  remoteAddr;
    UInt16                  procId;
    pid_t                   pid;
    Int                     state;
    iofunc_notify_t         notify[3];
} rpmsg_omx_object;

/*!
 *  @brief  Structure of Event callback argument passed to register fucntion.
 */
typedef struct rpmsg_omx_EventCbck_tag {
    List_Elem          element;
    /*!< List element header */
    rpmsg_omx_object * omx;
    /*!< User omx info pointer. Passed back to user callback function */
    UInt32             pid;
    /*!< Process Identifier for user process. */
} rpmsg_omx_EventCbck ;

/*!
 *  @brief  Keeps the information related to Event.
 */
typedef struct rpmsg_omx_EventState_tag {
    List_Handle            bufList;
    /*!< Head of received event list. */
    UInt32                 pid;
    /*!< User process ID. */
    rpmsg_omx_object *     omx;
    /*!< User omx comp. */
    UInt32                 refCount;
    /*!< Reference count, used when multiple Notify_registerEvent are called
         from same process space (multi threads/processes). */
    WaitingReaders_t *     head;
    /*!< Waiting readers head. */
    WaitingReaders_t *     tail;
    /*!< Waiting readers tail. */
} rpmsg_omx_EventState;

/*!
 *  @brief  Per-connection information
 */
typedef struct rpmsg_omx_ocb {
    iofunc_ocb_t        hdr;
    pid_t               pid;
    rpmsg_omx_object *  omx;
} rpmsg_omx_ocb_t;

typedef struct rpmsg_omx_name {
    char name[RPMSG_NAME_SIZE];
}rpmsg_omx_name_t;

#define RPMSG_OMX_MODULE_NAME "rpmsg-omx"

/*!
 *  @brief  rpmsg-omx Module state object
 */
typedef struct rpmsg_omx_ModuleObject_tag {
    Bool                 isSetup;
    /*!< Indicates whether the module has been already setup */
    Bool                 openRefCount;
    /*!< Open reference count. */
    IGateProvider_Handle gateHandle;
    /*!< Handle of gate to be used for local thread safety */
    rpmsg_omx_EventState eventState [MAX_PROCESSES];
    /*!< List for all user processes registered. */
    rpmsg_omx_conn_object * objects [MultiProc_MAXPROCESSORS];
    /*!< List of all remote connections. */
    MessageQCopy_Handle mqHandle;
    /*!< Local mq handle associated with this module */
    UInt32 endpoint;
    /*!< Local endpoint associated with the mq handle */
    OsalSemaphore_Handle sem;
    /*!< Handle to semaphore used for omx instance connection notifications */
    pthread_t nt;
    /*!< notifier thread */
    pthread_mutex_t lock;
    /*!< protection between notifier and event */
    pthread_cond_t  cond;
    /*!< protection between notifier and event */
    MsgList_t *head;
    /*!< list head */
    MsgList_t *tail;
    /*!< list tail */
    int run;
    /*!< notifier thread must keep running */
} rpmsg_omx_ModuleObject;

/*!
 *  @brief  Structure of Event Packet read from notify kernel-side.
 */
typedef struct rpmsg_omx_EventPacket_tag {
    List_Elem          element;
    /*!< List element header */
    UInt32             pid;
    /* Processor identifier */
    rpmsg_omx_object * obj;
    /*!< Pointer to the channel associated with this callback */
    UInt8              data[MessageQCopy_BUFSIZE];
    /*!< Data associated with event. */
    UInt32             len;
    /*!< Length of the data associated with event. */
    UInt32             src;
    /*!< Src endpoint associated with event. */
    struct rpmsg_omx_EventPacket * next;
    struct rpmsg_omx_EventPacket * prev;
} rpmsg_omx_EventPacket ;


/*
 * Instead of constantly allocating and freeing the uBuf structures
 * we just cache a few of them, and recycle them instead.
 * The cache count is set with CACHE_NUM in rpmsg-omxdrv.h.
 */
static rpmsg_omx_EventPacket *uBuf_cache;
static int num_uBuf = 0;

static void flush_uBuf()
{
    rpmsg_omx_EventPacket *uBuf = NULL;

    while(uBuf_cache) {
        num_uBuf--;
        uBuf = uBuf_cache;
        uBuf_cache = (rpmsg_omx_EventPacket *)uBuf_cache->next;
        Memory_free(NULL, uBuf, sizeof(*uBuf));
    }
}

static rpmsg_omx_EventPacket *get_uBuf()
{
    rpmsg_omx_EventPacket *uBuf;
    uBuf = uBuf_cache;
    if (uBuf != NULL) {
        uBuf_cache = (rpmsg_omx_EventPacket *)uBuf_cache->next;
        num_uBuf--;
    } else {
        uBuf = Memory_alloc(NULL, sizeof(rpmsg_omx_EventPacket), 0, NULL);
    }
    return(uBuf);
}

static void put_uBuf(rpmsg_omx_EventPacket * uBuf)
{
    if (num_uBuf >= CACHE_NUM) {
        Memory_free(NULL, uBuf, sizeof(*uBuf));
    } else {
        uBuf->next = (struct rpmsg_omx_EventPacket *)uBuf_cache;
        uBuf_cache = uBuf;
        num_uBuf++;
    }
    return;
}


/** ============================================================================
 *  Globals
 *  ============================================================================
 */
/*!
 *  @var    rpmsg_omx_state
 *
 *  @brief  rpmsg-omx state object variable
 */
static rpmsg_omx_ModuleObject rpmsg_omx_state =
{
    .gateHandle = NULL,
    .isSetup = FALSE,
    .openRefCount = 0,
    .nt = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .head = NULL,
    .tail = NULL,
    .run  = 0
};

extern dispatch_t * syslink_dpp;


static MsgList_t *find_nl(int index)
{
    MsgList_t *item=NULL;
    item = rpmsg_omx_state.head;
    while (item) {
        if (item->index == index)
            return(item);
        item = item->next;
    }
    return(item);
}

/* we have the right locks when calling this function */
/*!
 *  @brief      Function to enqueue a notify list item.
 *
 *  @param      index    Index of the client process associated with the item.
 *
 *  @sa         find_nl
 *              get_nl
 */
static int enqueue_notify_list(int index)
{
    MsgList_t *item;
    item = find_nl(index);
    if (item == NULL) {
        item = get_nl();
        if (item == NULL) {
            return(-1);
        }
        item->next = NULL;
        item->index = index;
        item->num_events=1;
        if (rpmsg_omx_state.head == NULL) {
            rpmsg_omx_state.head = item;
            rpmsg_omx_state.tail = item;
            item->prev = NULL;
        }
        else {
            item->prev = rpmsg_omx_state.tail;
            rpmsg_omx_state.tail->next = item;
            rpmsg_omx_state.tail = item;
        }
    }
    else {
        item->num_events++;
    }
    return(0);
}

/* we have the right locks when calling this function */
/*!
 *  @brief      Function to dequeue a notify list item.
 *
 *  @param      item     The item to remove.
 *
 *  @sa         put_nl
 */
static inline int dequeue_notify_list_item(MsgList_t *item)
{
    int index;
    if (item == NULL) {
        return(-1);
    }
    index = item->index;
    item->num_events--;
    if (item->num_events > 0) {
        return(index);
    }
    if (rpmsg_omx_state.head == item) {
        // removing head
        rpmsg_omx_state.head = item->next;
        if (rpmsg_omx_state.head != NULL) {
            rpmsg_omx_state.head->prev = NULL;
        }
        else {
            // removing head and tail
            rpmsg_omx_state.tail = NULL;
        }
    }
    else {
        item->prev->next = item->next;
        if (item->next != NULL) {
            item->next->prev = item->prev;
        }
        else {
            // removing tail
            rpmsg_omx_state.tail = item->prev;
        }
    }
    put_nl(item);
    return(index);
}

/* we have the right locks when calling this function */
/*!
 *  @brief      Function to add a waiting reader to the list.
 *
 *  @param      index    Index of the client process waiting reader to add.
 *  @param      rcvid    Receive ID of the client process that was passed
 *                       when the client called read().
 *
 *  @sa         None
 */
static int enqueue_waiting_reader(int index, int rcvid)
{
    WaitingReaders_t *item;
    item = get_wr();
    if (item == NULL) {
        return(-1);
    }
    item->rcvid = rcvid;
    item->next = NULL;
    if (rpmsg_omx_state.eventState [index].head == NULL) {
        rpmsg_omx_state.eventState [index].head = item;
        rpmsg_omx_state.eventState [index].tail = item;
    }
    else {
        rpmsg_omx_state.eventState [index].tail->next = item;
        rpmsg_omx_state.eventState [index].tail = item;
    }
    return(EOK);
}

/* we have the right locks when calling this function */
/* caller frees item */
/*!
 *  @brief      Function to remove a waiting reader from the list.
 *
 *  @param      index    Index of the client process waiting reader to dequeue.
 *
 *  @sa         None
 */
static WaitingReaders_t *dequeue_waiting_reader(int index)
{
    WaitingReaders_t *item = NULL;
    if (rpmsg_omx_state.eventState [index].head) {
        item = rpmsg_omx_state.eventState [index].head;
        rpmsg_omx_state.eventState [index].head = rpmsg_omx_state.eventState [index].head->next;
        if (rpmsg_omx_state.eventState [index].head == NULL) {
            rpmsg_omx_state.eventState [index].tail = NULL;
        }
    }
    return(item);
}

/*!
 *  @brief      Function find a specified waiting reader.
 *
 *  @param      index    Index of the client process waiting for the message.
 *  @param      rcvid    Receive ID of the client process that was passed
 *                       when the client called read().
 *
 *  @sa         None
 */

static WaitingReaders_t *find_waiting_reader(int index, int rcvid)
{
    WaitingReaders_t *item = NULL;
    WaitingReaders_t *prev = NULL;
    if (rpmsg_omx_state.eventState [index].head) {
        item = rpmsg_omx_state.eventState [index].head;
        while (item) {
            if (item->rcvid == rcvid) {
                /* remove item from list */
                if (prev)
                    prev->next = item->next;
                if (item == rpmsg_omx_state.eventState [index].head)
                    rpmsg_omx_state.eventState [index].head = item->next;
                break;
            }
            else {
                prev = item;
                item = item->next;
            }
        }
    }
    return item;
}

/*!
 *  @brief      Function used to check if there is a waiting reader with an
 *              event (message) ready to be delivered.
 *
 *  @param      index    Index of the client process waiting for the message.
 *  @param      item     Pointer to the waiting reader.
 *
 *  @sa         dequeue_notify_list_item
 *              dequeue_waiting_reader
 */

static int find_available_reader_and_event(int *index, WaitingReaders_t **item)
{
    MsgList_t *temp;
    if (rpmsg_omx_state.head == NULL) {
        return(0);
    }
    temp = rpmsg_omx_state.head;
    while (temp) {
        if (rpmsg_omx_state.eventState [temp->index].head) {
            // event and reader found
            if (dequeue_notify_list_item(temp) >= 0) {
                *index = temp->index;
                *item = dequeue_waiting_reader(temp->index);
            }
            else {
                /* error occurred, return 0 as item has not been set */
                return(0);
            }
            return(1);
        }
        temp = temp->next;
    }
    return(0);
}

/*!
 *  @brief      Function used to deliver the notification to the client that
 *              it has received a message.
 *
 *  @param      index    Index of the client process receiving hte message.
 *  @param      rcvid    Receive ID of the client process that was passed
 *                       when the client called read().
 *
 *  @sa         put_uBuf
 */

static void deliver_notification(int index, int rcvid)
{
    int err = EOK;
    rpmsg_omx_EventPacket * uBuf     = NULL;
    struct omx_msg_hdr * hdr = NULL;

    uBuf = (rpmsg_omx_EventPacket *) List_get (rpmsg_omx_state.eventState [index].bufList);
    hdr = (struct omx_msg_hdr *)uBuf->data;

    /*  Let the check remain at run-time. */
    if (uBuf != NULL) {
        err = MsgReply(rcvid, hdr->len, hdr->data, hdr->len);
        if (err == -1)
            perror("deliver_notification: MsgReply");
        /* Free the processed event callback packet. */
        put_uBuf(uBuf);
    }
    else {
        MsgReply(rcvid, EOK, NULL, 0);
    }
    return;
}

/*!
 *  @brief      Thread used for notifying waiting readers of messages.
 *
 *  @param      arg    Thread-specific private arg.
 *
 *  @sa         find_available_reader_and_event
 *              deliver_notification
 *              put_wr
 */
static void *notifier_thread(void *arg)
{
    int status;
    int index;
    WaitingReaders_t *item = NULL;
    pthread_mutex_lock(&rpmsg_omx_state.lock);
    while (rpmsg_omx_state.run) {
        status = find_available_reader_and_event(&index, &item);
        if ( (status == 0) || (item == NULL) ) {
            status = pthread_cond_wait(&rpmsg_omx_state.cond, &rpmsg_omx_state.lock);
            if ((status != EOK) && (status != EINTR)) {
                // false wakeup
                break;
            }
            status = find_available_reader_and_event(&index, &item);
            if ( (status == 0) || (item == NULL) ) {
                continue;
            }
        }
        pthread_mutex_unlock(&rpmsg_omx_state.lock);
        // we have unlocked, and now we have an event to deliver
        // we deliver one event at a time, relock, check and continue
        deliver_notification(index, item->rcvid);
        pthread_mutex_lock(&rpmsg_omx_state.lock);
        put_wr(item);
    }
    pthread_mutex_unlock(&rpmsg_omx_state.lock);
    return(NULL);
}


static
Int
_rpmsg_omx_connect(resmgr_context_t *ctp, io_devctl_t *msg, rpmsg_omx_ocb_t *ocb)
{
    Int status = EOK;
    struct omx_conn_req * cargs = (struct omx_conn_req *)(_DEVCTL_DATA (msg->i));
    struct omx_msg_hdr * hdr = NULL;
    rpmsg_omx_object * omx = ocb->omx;
    UInt8 buf[sizeof(struct omx_conn_req) + sizeof(struct omx_msg_hdr)];

    if (omx->state == OMX_CONNECTED) {
        GT_0trace(curTrace, GT_4CLASS, "Already connected.");
        status = (EISCONN);
    }
    else if (ctp->info.msglen - sizeof(msg->i) < sizeof (struct omx_conn_req)) {
        status = (EINVAL);
    }
    else if (String_nlen(cargs->name, 47) == -1) {
        status = (EINVAL);
    }
    else {
        hdr = (struct omx_msg_hdr *)buf;
        hdr->type = OMX_CONN_REQ;
        hdr->len = sizeof(struct omx_conn_req);
        Memory_copy(hdr->data, cargs, sizeof(struct omx_conn_req));

        status = MessageQCopy_send (omx->conn->procId, // remote procid
                                    MultiProc_self(), // local procid
                                    omx->conn->addr, // remote server
                                    omx->addr, // local address
                                    buf, // connect msg
                                    sizeof(buf), // msg size
                                    TRUE); // wait for available bufs
        if (status != MessageQCopy_S_SUCCESS) {
            GT_0trace(curTrace, GT_4CLASS, "Failed to send connect message.");
            status = (EIO);
        }
        else {
            status = OsalSemaphore_pend(rpmsg_omx_state.sem, 5000);
            if (omx->state == OMX_CONNECTED) {
                msg->o.ret_val = EOK;
                status = (_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o)));
            }
            else if (omx->state == OMX_FAIL) {
                GT_0trace(curTrace, GT_4CLASS, "Failed to connect message.");
                status = (ENXIO);
            }
            else if (status < 0) {
                GT_0trace(curTrace, GT_4CLASS, "Semaphore pend failed.");
                status = (EIO);
            }
            else {
                status = (ETIMEDOUT);
            }
        }
    }

    return status;
}


static
Int
_rpmsg_omx_disconnect(resmgr_context_t *ctp, io_devctl_t *msg, rpmsg_omx_ocb_t *ocb)
{
    Int status = EOK;
    struct omx_msg_hdr * hdr = NULL;
    rpmsg_omx_object * omx = ocb->omx;
    UInt8 buf[sizeof(struct omx_disc_req) + sizeof(struct omx_msg_hdr)];
    struct omx_disc_req * dreq = NULL;

    if (omx->state != OMX_CONNECTED) {
        GT_0trace(curTrace, GT_4CLASS, "Already disconnected.");
        status = (ENOTCONN);
    }
    else {
        hdr = (struct omx_msg_hdr *)buf;
        hdr->type = OMX_DISCONNECT;
        hdr->len = sizeof(struct omx_conn_req);
        dreq = (struct omx_disc_req *)hdr->data;
        dreq->addr = omx->remoteAddr;

        status = MessageQCopy_send (omx->conn->procId, // remote procid
                                    MultiProc_self(), // local procid
                                    omx->conn->addr, // remote server
                                    omx->addr, // local address
                                    buf, // connect msg
                                    sizeof(buf), // msg size
                                    TRUE); // wait for available bufs
        if (status != MessageQCopy_S_SUCCESS) {
            GT_0trace(curTrace, GT_4CLASS, "Failed to send disconnect message.");
            status = (EIO);
        }
        else {
            /* There will be no response, so don't wait. */
            omx->state = OMX_UNCONNECTED;
        }
    }

    return status;
}


Int
rpmsg_omx_devctl(resmgr_context_t *ctp, io_devctl_t *msg, IOFUNC_OCB_T *i_ocb)
{
    Int status = 0;
    rpmsg_omx_ocb_t *ocb = (rpmsg_omx_ocb_t *)i_ocb;

    if ((status = iofunc_devctl_default(ctp, msg, &ocb->hdr)) != _RESMGR_DEFAULT)
        return(_RESMGR_ERRNO(status));
    status = 0;

    switch (msg->i.dcmd)
    {
        case OMX_IOCCONNECT:
            status = _rpmsg_omx_connect (ctp, msg, ocb);
            break;
        default:
            status = (ENOSYS);
            break;
    }

    return status;
}


/*!
 *  @brief      Attach a process to rpmsg-omx user support framework.
 *
 *  @param      pid    Process identifier
 *
 *  @sa         _rpmsg_omx_detach
 */
static
Int
_rpmsg_omx_attach (rpmsg_omx_object * omx)
{
    Int32                status   = EOK;
    Bool                 flag     = FALSE;
    Bool                 isInit   = FALSE;
    List_Object *        bufList  = NULL;
    IArg                 key      = 0;
    List_Params          listparams;
    UInt32               i;

    GT_1trace (curTrace, GT_ENTER, "_rpmsg_omx_attach", omx);

    key = IGateProvider_enter (rpmsg_omx_state.gateHandle);
    for (i = 0 ; (i < MAX_PROCESSES) ; i++) {
        if (rpmsg_omx_state.eventState [i].omx == omx) {
            rpmsg_omx_state.eventState [i].refCount++;
            isInit = TRUE;
            status = EOK;
            break;
        }
    }

    if (isInit == FALSE) {
        List_Params_init (&listparams);
        bufList = List_create (&listparams) ;
        /* Search for an available slot for user process. */
        for (i = 0 ; i < MAX_PROCESSES ; i++) {
            if (rpmsg_omx_state.eventState [i].omx == NULL) {
                rpmsg_omx_state.eventState [i].omx = omx;
                rpmsg_omx_state.eventState [i].refCount = 1;
                rpmsg_omx_state.eventState [i].bufList = bufList;
                flag = TRUE;
                break;
            }
        }

        /* No free slots found. Let this check remain at run-time,
         * since it is dependent on user environment.
         */
        if (flag != TRUE) {
            /*! @retval Notify_E_RESOURCE Maximum number of
             supported user clients have already been registered. */
            status = -ENOMEM;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "rpmsgDrv_attach",
                              status,
                              "Maximum number of supported user"
                                  " clients have already been "
                                  "registered.");
            if (bufList != NULL) {
                List_delete (&bufList);
            }
        }
    }
    IGateProvider_leave (rpmsg_omx_state.gateHandle, key);

    GT_1trace (curTrace, GT_LEAVE, "rpmsgDrv_attach", status);

    /*! @retval Notify_S_SUCCESS Operation successfully completed. */
    return status ;
}


 /*!
 *  @brief      This function adds a data to a registered process.
 *
 *  @param      dce       OMX object associated with the client
 *  @param      src       Source address (endpoint) sending the data
 *  @param      pid       Process ID associated with the client
 *  @param      data      Data to be added
 *  @param      len       Length of data to be added
 *
 *  @sa
 */
Int
_rpmsg_omx_addBufByPid (rpmsg_omx_object *omx,
                        UInt32             src,
                        UInt32             pid,
                        void *             data,
                        UInt32             len)
{
    Int32                   status = EOK;
    Bool                    flag   = FALSE;
    rpmsg_omx_EventPacket * uBuf   = NULL;
    IArg                    key;
    UInt32                  i;
    WaitingReaders_t *item;
    MsgList_t *msgItem;

    GT_5trace (curTrace,
               GT_ENTER,
               "_rpmsg_omx_addBufByPid",
               omx,
               src,
               pid,
               data,
               len);

    GT_assert (curTrace, (rpmsg_omx_state.isSetup == TRUE));

    key = IGateProvider_enter (rpmsg_omx_state.gateHandle);
    /* Find the registration for this callback */
    for (i = 0 ; i < MAX_PROCESSES ; i++) {
        if (rpmsg_omx_state.eventState [i].omx == omx) {
            flag = TRUE;
            break;
        }
    }
    IGateProvider_leave (rpmsg_omx_state.gateHandle, key);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (flag != TRUE) {
        /*! @retval ENOMEM Could not find a registered handler
                                      for this process. */
        status = -ENOMEM;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_rpmsgDrv_addBufByPid",
                             status,
                             "Could not find a registered handler "
                             "for this process.!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Allocate memory for the buf */
        pthread_mutex_lock(&rpmsg_omx_state.lock);
        uBuf = get_uBuf();
        pthread_mutex_unlock(&rpmsg_omx_state.lock);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (uBuf == NULL) {
            /*! @retval Notify_E_MEMORY Failed to allocate memory for event
                                packet for received callback. */
            status = -ENOMEM;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_rpmsgDrv_addBufByPid",
                                 status,
                                 "Failed to allocate memory for event"
                                 " packet for received callback.!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            List_elemClear (&(uBuf->element));
            GT_assert (curTrace,
                       (rpmsg_omx_state.eventState [i].bufList != NULL));

            if (data) {
                Memory_copy(uBuf->data, data, len);
            }
            uBuf->len = len;

            List_put (rpmsg_omx_state.eventState [i].bufList,
                      &(uBuf->element));
            pthread_mutex_lock(&rpmsg_omx_state.lock);
            item = dequeue_waiting_reader(i);
            if (item) {
                // there is a waiting reader
                deliver_notification(i, item->rcvid);
                put_wr(item);
                pthread_mutex_unlock(&rpmsg_omx_state.lock);
                status = EOK;
            }
            else {
                if (enqueue_notify_list(i) < 0) {
                    pthread_mutex_unlock(&rpmsg_omx_state.lock);
                    status = -ENOMEM;
                    GT_setFailureReason (curTrace,
                                  GT_4CLASS,
                                  "_rpmsgDrv_addBufByPid",
                                  status,
                                  "Failed to allocate memory for notifier");
                }
                else {
                    msgItem = find_nl(i);
                    /* TODO: omx could be NULL in some cases  */
                    if (omx && msgItem) {
                        if (IOFUNC_NOTIFY_INPUT_CHECK(omx->notify, msgItem->num_events, 0)) {
                            iofunc_notify_trigger(omx->notify, msgItem->num_events, IOFUNC_NOTIFY_INPUT);
                        }
                    }
                    status = EOK;
                    pthread_cond_signal(&rpmsg_omx_state.cond);
                    pthread_mutex_unlock(&rpmsg_omx_state.lock);
                }
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "_rpmsgDrv_addBufByPid", status);

    return status;
}


/*!
 *  @brief      This function implements the callback registered with
 *              MessageQCopy_create for each client.  This function
 *              adds the message from the remote proc to a list
 *              where it is routed to the appropriate waiting reader.
 *
 *  @param      procId    processor Id from which interrupt is received
 *  @param      lineId    Interrupt line ID to be used
 *  @param      eventId   eventId registered
 *  @param      arg       argument to call back
 *  @param      payload   payload received
 *
 *  @sa
 */
Void
_rpmsg_omx_cb (MessageQCopy_Handle handle, void * data, int len, void * priv, UInt32 src, UInt16 srcProc)
{
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int32                   status = 0;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    rpmsg_omx_object * omx = NULL;
    struct omx_msg_hdr * msg_hdr = NULL;
    struct omx_conn_rsp * reply;

    GT_6trace (curTrace,
               GT_ENTER,
               "_rpmsg_omx_cb",
               handle,
               data,
               len,
               priv,
               src,
               srcProc);

    omx = (rpmsg_omx_object *) priv;
    msg_hdr = (struct omx_msg_hdr *)data;

    switch (msg_hdr->type) {
        case OMX_CONN_RSP:
            reply = (struct omx_conn_rsp *)msg_hdr->data;
            omx->remoteAddr = reply->addr;
            if (reply->status != OMX_SUCCESS) {
                omx->state = OMX_FAIL;
            }
            else {
                omx->state = OMX_CONNECTED;
            }
            /* post the semaphore to have the ioctl reply */
            OsalSemaphore_post(rpmsg_omx_state.sem);
            break;
        case OMX_RAW_MSG:
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            status =
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            _rpmsg_omx_addBufByPid (omx,
                                    src,
                                    omx->pid,
                                    data,
                                    len);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_rpmsg_omx_cb",
                                     status,
                                     "Failed to add callback packet for pid");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            break;
        default:
            break;
    }

    GT_0trace (curTrace, GT_LEAVE, "_rpmsg_omx_cb");
}

 /**
  * Handler for ocb_calloc() requests.
  *
  * Special handler for ocb_calloc() requests that we export for control.  An
  * open request from the client will result in a call to our special ocb_calloc
  * handler.  This function attaches the client's pid using _rpmsg_dce_attach
  * and allocates client-specific information.  This function creates an
  * endpoint for the client to communicate with the dCE server on the
  * remote core also.
  *
  * \param ctp       Thread's associated context information.
  * \param device    Device attributes structure.
  *
  * \return Pointer to an iofunc_ocb_t OCB structure.
  */

IOFUNC_OCB_T *
rpmsg_omx_ocb_calloc (resmgr_context_t * ctp, IOFUNC_ATTR_T * device)
{
    rpmsg_omx_ocb_t *ocb = NULL;
    rpmsg_omx_object *obj = NULL;
    struct _msg_info cl_info;
    rpmsg_omx_dev_t * dev = NULL;
    int i = 0;
    Bool found = FALSE;
    char path1[20];
    char path2[20];

    GT_2trace (curTrace, GT_ENTER, "rpmsg_omx_ocb_calloc",
               ctp, device);

    /* Allocate the OCB */
    ocb = (rpmsg_omx_ocb_t *) calloc (1, sizeof (rpmsg_omx_ocb_t));
    if (ocb == NULL){
        errno = ENOMEM;
        return (NULL);
    }

    ocb->pid = ctp->info.pid;

    /* Allocate memory for the rpmsg object. */
    obj = Memory_calloc (NULL, sizeof (rpmsg_omx_object), 0u, NULL);
    if (obj == NULL) {
        errno = ENOMEM;
        free(ocb);
        return (NULL);
    }
    else {
        ocb->omx = obj;
        IOFUNC_NOTIFY_INIT(obj->notify);
        obj->state = OMX_UNCONNECTED;
        /* determine conn and procId for communication based on which device was opened */
        MsgInfo(ctp->rcvid, &cl_info);
        resmgr_pathname(ctp->id, 0, path1, sizeof(path1));
        for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
            if (rpmsg_omx_state.objects[i] != NULL) {
                dev = rpmsg_omx_state.objects[i]->dev;
                resmgr_pathname(dev->rpmsg_omx.resmgr_id, 0, path2, sizeof(path2));
                if (!strcmp(path1, path2)) {
                    found = TRUE;
                    break;
                }
            }
        }
        if (found) {
            obj->conn = rpmsg_omx_state.objects[i];
            obj->procId = obj->conn->procId;
            obj->pid = ctp->info.pid;
            obj->mq = MessageQCopy_create (MessageQCopy_ADDRANY, NULL, _rpmsg_omx_cb, obj, &obj->addr);
            if (obj->mq == NULL) {
                errno = ENOMEM;
                free(obj);
                free(ocb);
                return (NULL);
            }
            else {
                if (_rpmsg_omx_attach (ocb->omx) < 0) {
                    errno = ENOMEM;
                    MessageQCopy_delete (&obj->mq);
                    free(obj);
                    free(ocb);
                    return (NULL);
                }
            }
        }
    }

    GT_1trace (curTrace, GT_LEAVE, "rpmsg_omx_ocb_calloc", ocb);

    return (IOFUNC_OCB_T *)(ocb);
}


/*!
 *  @brief      Detach a process from rpmsg-omx user support framework.
 *
 *  @param      pid    Process identifier
 *
 *  @sa         _rpmsg_omx_attach
 */
static
Int
_rpmsg_omx_detach (rpmsg_omx_object * omx)
{
    Int32                status    = EOK;
    Int32                tmpStatus = EOK;
    Bool                 flag      = FALSE;
    List_Object *        bufList   = NULL;
    UInt32               i;
    IArg                 key;
    MsgList_t          * item;
    WaitingReaders_t   * wr        = NULL;
    struct _msg_info     info;

    GT_1trace (curTrace, GT_ENTER, "rpmsg_omx_detach", omx);

    key = IGateProvider_enter (rpmsg_omx_state.gateHandle);

    for (i = 0 ; i < MAX_PROCESSES ; i++) {
        if (rpmsg_omx_state.eventState [i].omx == omx) {
            if (rpmsg_omx_state.eventState [i].refCount == 1) {
                rpmsg_omx_state.eventState [i].refCount = 0;

                flag = TRUE;
                break;
            }
            else {
                rpmsg_omx_state.eventState [i].refCount--;
                status = EOK;
                break;
            }
        }
    }
    IGateProvider_leave (rpmsg_omx_state.gateHandle, key);

    if (flag == TRUE) {
        key = IGateProvider_enter (rpmsg_omx_state.gateHandle);
        /* Last client being unregistered for this process. */
        rpmsg_omx_state.eventState [i].omx = NULL;

        /* Store in local variable to delete outside lock. */
        bufList = rpmsg_omx_state.eventState [i].bufList;

        rpmsg_omx_state.eventState [i].bufList = NULL;

        IGateProvider_leave (rpmsg_omx_state.gateHandle, key);
    }

    if (flag != TRUE) {
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (i == MAX_PROCESSES) {
            /*! @retval Notify_E_NOTFOUND The specified user process was
                     not found registered with Notify Driver module. */
            status = -ENOMEM;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "rpmsg_omx_detach",
                              status,
                              "The specified user process was not found"
                              " registered with rpmsg Driver module.");
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }
    else {
        if (bufList != NULL) {
            /* Dequeue waiting readers and reply to them */
            pthread_mutex_lock(&rpmsg_omx_state.lock);
            while ((wr = dequeue_waiting_reader(i)) != NULL) {
                /* Check if rcvid is still valid */
                if (MsgInfo(wr->rcvid, &info) != -1) {
                    put_wr(wr);
                    pthread_mutex_unlock(&rpmsg_omx_state.lock);
                    MsgError(wr->rcvid, EINTR);
                    pthread_mutex_lock(&rpmsg_omx_state.lock);
                }
            }
            /* Check for pending ionotify/select calls */
            if (omx) {
                if (IOFUNC_NOTIFY_INPUT_CHECK(omx->notify, 1, 0)) {
                    iofunc_notify_trigger(omx->notify, 1, IOFUNC_NOTIFY_INPUT);
                }
            }

            /* Free event packets for any received but unprocessed events. */
            while ((item = find_nl(i)) != NULL) {
                if (dequeue_notify_list_item(item) >= 0) {
                    rpmsg_omx_EventPacket * uBuf = NULL;

                    uBuf = (rpmsg_omx_EventPacket *) List_get (bufList);

                    /*  Let the check remain at run-time. */
                    if (uBuf != NULL) {
                        put_uBuf(uBuf);
                    }
                }
            }
            pthread_mutex_unlock(&rpmsg_omx_state.lock);

            /* Last client being unregistered with Notify module. */
            List_delete (&bufList);
        }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if ((tmpStatus < 0) && (status >= 0)) {
            status =  tmpStatus;
            GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rpmsg_omx_detach",
                             status,
                             "Failed to delete termination semaphore!");
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_1trace (curTrace, GT_LEAVE, "rpmsg_omx_detach", status);

    /*! @retval Notify_S_SUCCESS Operation successfully completed */
    return status;
}

 /**
  * Handler for ocb_free() requests.
  *
  * Special handler for ocb_free() requests that we export for control.  A
  * close request from the client will result in a call to our special ocb_free
  * handler.  This function detaches the client's pid using _rpmsg_dce_detach
  * and frees any client-specific information that was allocated.
  *
  * \param i_ocb     OCB associated with client's session.
  *
  * \return POSIX errno value.
  *
  * \retval None.
  */

void
rpmsg_omx_ocb_free (IOFUNC_OCB_T * i_ocb)
{
    rpmsg_omx_ocb_t * ocb = (rpmsg_omx_ocb_t *)i_ocb;
    rpmsg_omx_object *obj;

    if (ocb && ocb->omx) {
        obj = ocb->omx;
        if (obj->state == OMX_CONNECTED) {
            /* Need to disconnect this device */
            _rpmsg_omx_disconnect(NULL, NULL, ocb);
        }
        _rpmsg_omx_detach(ocb->omx);
        if (obj->mq) {
            MessageQCopy_delete (&obj->mq);
            obj->mq = NULL;
        }
        free (obj);
        free (ocb);
    }
}

 /**
  * Handler for close_ocb() requests.
  *
  * This function removes the notification entries associated with the current
  * client.
  *
  * \param ctp       Thread's associated context information.
  * \param reserved  This argument must be NULL.
  * \param ocb       OCB associated with client's session.
  *
  * \return POSIX errno value.
  *
  * \retval EOK      Success.
  */

Int
rpmsg_omx_close_ocb (resmgr_context_t *ctp, void *reserved, RESMGR_OCB_T *ocb)
{
    rpmsg_omx_ocb_t * omx_ocb = (rpmsg_omx_ocb_t *)ocb;
    iofunc_notify_remove(ctp, omx_ocb->omx->notify);
    return (iofunc_close_ocb_default(ctp, reserved, ocb));
}

 /**
  * Handler for read() requests.
  *
  * Handles special read() requests that we export for control.  A read
  * request will get a message from the remote processor that is associated
  * with the client that is calling read().
  *
  * \param ctp     Thread's associated context information.
  * \param msg     The actual read() message.
  * \param ocb     OCB associated with client's session.
  *
  * \return POSIX errno value.
  *
  * \retval EOK      Success.
  * \retval EAGAIN   Call is non-blocking and no messages available.
  * \retval ENOMEM   Not enough memory to preform the read.
  */

int rpmsg_omx_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb)
{
    Int status;
    rpmsg_omx_ocb_t * omx_ocb = (rpmsg_omx_ocb_t *)ocb;
    rpmsg_omx_object * omx = omx_ocb->omx;
    Bool                    flag     = FALSE;
    Int                  retVal   = EOK;
    UInt32                  i;
    MsgList_t          * item;
    Int                  nonblock;

    if ((status = iofunc_read_verify(ctp, msg, ocb, &nonblock)) != EOK)
        return (status);

    if (omx->state != OMX_CONNECTED) {
        return (ENOTCONN);
    }

    for (i = 0 ; i < MAX_PROCESSES ; i++) {
        if (rpmsg_omx_state.eventState [i].omx == omx) {
            flag = TRUE;
            break;
        }
    }

    /* Let the check remain at run-time. */
    if (flag == TRUE) {
        /* Let the check remain at run-time for handling any run-time
        * race conditions.
        */
        if (rpmsg_omx_state.eventState [i].bufList != NULL) {
            pthread_mutex_lock(&rpmsg_omx_state.lock);
            item = find_nl(i);
            if (dequeue_notify_list_item(item) < 0) {
                if (nonblock) {
                    pthread_mutex_unlock(&rpmsg_omx_state.lock);
                    return EAGAIN;
                }
                else {
                    retVal = enqueue_waiting_reader(i, ctp->rcvid);
                    if (retVal == EOK) {
                        pthread_cond_signal(&rpmsg_omx_state.cond);
                        pthread_mutex_unlock(&rpmsg_omx_state.lock);
                        return(_RESMGR_NOREPLY);
                    }
                    retVal = ENOMEM;
                    pthread_mutex_unlock(&rpmsg_omx_state.lock);
                }
            }
            else {
                deliver_notification(i, ctp->rcvid);
                pthread_mutex_unlock(&rpmsg_omx_state.lock);
                return(_RESMGR_NOREPLY);
            }
        }
    }

    /*! @retval Number-of-bytes-read Number of bytes read. */
    return retVal;
}

 /**
  * Unblock read calls
  *
  * This function checks if the client is blocked on a read call and if so,
  * unblocks the client.
  *
  * \param ctp     Thread's associated context information.
  * \param msg     The pulse message.
  * \param ocb     OCB associated with client's session.
  *
  * \return POSIX errno value.
  *
  * \retval EINTR    The client has been unblocked.
  * \retval other    The client has not been unblocked or the client was not
  *                  blocked.
  */

int rpmsg_omx_read_unblock(resmgr_context_t *ctp, io_pulse_t *msg, iofunc_ocb_t *ocb)
{
    UInt32                  i;
    Bool                    flag     = FALSE;
    WaitingReaders_t      * wr;
    rpmsg_omx_ocb_t * omx_ocb = (rpmsg_omx_ocb_t *)ocb;
    rpmsg_omx_object * omx = omx_ocb->omx;

    for (i = 0 ; i < MAX_PROCESSES ; i++) {
        if (rpmsg_omx_state.eventState [i].omx == omx) {
            flag = TRUE;
            break;
        }
    }

    /*  Let the check remain at run-time. */
    if (flag == TRUE) {
        /* Let the check remain at run-time for handling any run-time
         * race conditions.
         */
        if (rpmsg_omx_state.eventState [i].bufList != NULL) {
            pthread_mutex_lock(&rpmsg_omx_state.lock);
            wr = find_waiting_reader(i, ctp->rcvid);
            if (wr) {
                put_wr(wr);
                pthread_mutex_unlock(&rpmsg_omx_state.lock);
                return (EINTR);
            }
            pthread_mutex_unlock(&rpmsg_omx_state.lock);
        }
    }

    return _RESMGR_NOREPLY;
}

/**
  * Handler for unblock() requests.
  *
  * Handles unblock request for the client which is requesting to no longer be
  * blocked on the rpmsg-omx driver.
  *
  * \param ctp     Thread's associated context information.
  * \param msg     The pulse message.
  * \param ocb     OCB associated with client's session.
  *
  * \return POSIX errno value.
  *
  * \retval EINTR    The rcvid has been unblocked.
  */

int rpmsg_omx_unblock(resmgr_context_t *ctp, io_pulse_t *msg, RESMGR_OCB_T *ocb)
{
    int status = _RESMGR_NOREPLY;
    struct _msg_info        info;

    /*
     * Try to run the default unblock for this message.
     */
    if ((status = iofunc_unblock_default(ctp,msg,ocb)) != _RESMGR_DEFAULT) {
        return status;
    }

    /*
     * Check if rcvid is still valid and still has an unblock
     * request pending.
     */
    if (MsgInfo(ctp->rcvid, &info) == -1 ||
        !(info.flags & _NTO_MI_UNBLOCK_REQ)) {
        return _RESMGR_NOREPLY;
    }

    if (rpmsg_omx_read_unblock(ctp, msg, ocb) != _RESMGR_NOREPLY) {
           return _RESMGR_ERRNO(EINTR);
    }

    return _RESMGR_ERRNO(EINTR);
}


uint32_t
_rpmsg_omx_pa2da(ProcMgr_Handle handle, uint32_t pa)
{
    Int status = 0;
    uint32_t da;

    if (pa >= TILER_MEM_8BIT && pa < TILER_MEM_END) {
        return pa;
    }
    else {
        status = ProcMgr_translateAddr(handle, (Ptr *)&da,
                                       ProcMgr_AddrType_SlaveVirt,
                                       (Ptr)pa, ProcMgr_AddrType_MasterPhys);
        if (status >= 0)
            return da;
        else
            return 0;
    }
}

int
_rpmsg_omx_map(ProcMgr_Handle handle, char *data, uint32_t bytes, pid_t pid)
{
    int status = EOK;
    struct omx_packet *packet = (struct omx_packet *)data;
    char *map_info = NULL;
    RPC_OMX_MAP_INFO_TYPE type;
    int i = 0;
    int buf_offset = 0;
    uint32_t *buffer = NULL;
    off64_t phys_addr;
    uint32_t ipu_addr;
    uint32_t msg_size;
    size_t phys_len = 0;

    if (bytes <= sizeof(struct omx_packet)) {
        msg_size = 0;
    }
    else {
        msg_size = bytes - sizeof(struct omx_packet);
    }
    if (msg_size < sizeof(RPC_OMX_MAP_INFO_TYPE))
        return (-EINVAL);

    type = *(RPC_OMX_MAP_INFO_TYPE *)(packet->data);

    if (type == RPC_OMX_MAP_INFO_NONE)
        return EOK;
    if (type != RPC_OMX_MAP_INFO_ONE_BUF &&
        type != RPC_OMX_MAP_INFO_TWO_BUF &&
        type != RPC_OMX_MAP_INFO_THREE_BUF) {
        return (-EINVAL);
    }

    map_info = (char *)((uint32_t)packet->data);

    if (msg_size < sizeof(int) + sizeof(RPC_OMX_MAP_INFO_TYPE))
        return (-EINVAL);

    buf_offset = *(int *)((uint32_t)map_info + sizeof(RPC_OMX_MAP_INFO_TYPE));
    if (buf_offset < 0 || (buf_offset + (sizeof(*buffer) * type)) > msg_size)
        return (-EINVAL);

    map_info = (char *)((uint32_t)map_info + buf_offset);

    for (i = 0; i < type; i++) {
        buffer = (uint32_t *)((uint32_t)map_info + (i * sizeof(*buffer)));
        if (*buffer) {
            /* currently only Tiler buffers are supported */
            status = mem_offset64_peer(pid, (uintptr_t)((uint32_t)*buffer), 4, &phys_addr, &phys_len);
            if (status >= 0) {
                if ((ipu_addr = _rpmsg_omx_pa2da(handle, (uint32_t)phys_addr)) != 0)
                    *buffer = ipu_addr;
                else {
                    status = -EINVAL;
                    break;
                }
            }
            else {
                status = -EINVAL;
                break;
            }
        }
    }

    return status;
}

 /**
  * Handler for write() requests.
  *
  * Handles special write() requests that we export for control.  A write()
  * request will send a message to the remote processor which is associated with
  * the client.
  *
  * \param ctp     Thread's associated context information.
  * \param msg     The actual write() message.
  * \param io_ocb  OCB associated with client's session.
  *
  * \return POSIX errno value.
  *
  * \retval EOK      Success.
  * \retval ENOMEM   Not enough memory to preform the write.
  * \retval EIO      MessageQCopy_send failed.
  * \retval EINVAL   msg->i.bytes is negative.
  */

int
rpmsg_omx_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *io_ocb)
{
    int status;
    char buf[MessageQCopy_BUFSIZE];
    int bytes;
    rpmsg_omx_ocb_t * ocb = (rpmsg_omx_ocb_t *)io_ocb;
    rpmsg_omx_object * omx = ocb->omx;
    struct omx_msg_hdr * msg_hdr = NULL;

    if ((status = iofunc_write_verify(ctp, msg, io_ocb, NULL)) != EOK) {
        return (status);
    }

    bytes = ((int64_t) msg->i.nbytes) + sizeof(struct omx_msg_hdr) > MessageQCopy_BUFSIZE ?
            MessageQCopy_BUFSIZE - sizeof(struct omx_msg_hdr) : msg->i.nbytes;
    if (bytes < 0) {
        return EINVAL;
    }
    _IO_SET_WRITE_NBYTES (ctp, bytes);

    msg_hdr = (struct omx_msg_hdr *)buf;

    status = resmgr_msgread(ctp, msg_hdr->data, bytes, sizeof(msg->i));
    if (status != bytes) {
        return (errno);
    }

    /* check that we're in the correct state */
    if (omx->state != OMX_CONNECTED) {
        return (ENOTCONN);
    }

    status = _rpmsg_omx_map(omx->conn->procH, msg_hdr->data, bytes, ctp->info.pid);
    if (status < 0) {
        return -status;
    }

    msg_hdr->type = OMX_RAW_MSG;
    msg_hdr->len = bytes;

    status = MessageQCopy_send(omx->conn->procId, MultiProc_self(),
                               omx->remoteAddr, omx->addr, buf,
                               bytes + sizeof(struct omx_msg_hdr), TRUE);
    if (status < 0) {
        return (EIO);
    }

    return(EOK);
}



 /**
  * Handler for notify() requests.
  *
  * Handles special notify() requests that we export for control.  A notify
  * request results from the client calling select().
  *
  * \param ctp     Thread's associated context information.
  * \param msg     The actual notify() message.
  * \param ocb     OCB associated with client's session.
  *
  * \return POSIX errno value.
  */

Int rpmsg_omx_notify( resmgr_context_t *ctp, io_notify_t *msg, RESMGR_OCB_T *ocb)
{
    rpmsg_omx_ocb_t * omx_ocb = (rpmsg_omx_ocb_t *)ocb;
    rpmsg_omx_object * omx = omx_ocb->omx;
    int trig;
    int i = 0;
    Bool flag = FALSE;
    MsgList_t * item = NULL;
    int status = EOK;

    trig = _NOTIFY_COND_OUTPUT; /* clients can give us data */

    for (i = 0 ; i < MAX_PROCESSES ; i++) {
        if (rpmsg_omx_state.eventState [i].omx == omx) {
            flag = TRUE;
            break;
        }
    }

    pthread_mutex_lock(&rpmsg_omx_state.lock);
    /* Let the check remain at run-time. */
    if (flag == TRUE) {
        /* Let the check remain at run-time for handling any run-time
        * race conditions.
        */
        if (rpmsg_omx_state.eventState [i].bufList != NULL) {
            item = find_nl(i);
            if (item && item->num_events > 0) {
                trig |= _NOTIFY_COND_INPUT;
            }
        }
    }
    status = iofunc_notify(ctp, msg, omx_ocb->omx->notify, trig, NULL, NULL);
    pthread_mutex_unlock(&rpmsg_omx_state.lock);
    return status;
}

 /**
  * Detaches an rpmsg-dce resource manager device name.
  *
  * \param dev     The device to detach.
  *
  * \return POSIX errno value.
  */

static
Void
_deinit_rpmsg_omx_device (rpmsg_omx_dev_t * dev)
{
    resmgr_detach(syslink_dpp, dev->rpmsg_omx.resmgr_id, _RESMGR_DETACH_CLOSE);

    pthread_mutex_destroy(&dev->rpmsg_omx.mutex);

    free (dev);

    return;
}

 /**
  * Initializes and attaches rpmsg-dce resource manager functions to an
  * rpmsg-dce device name.
  *
  * \param num     The number to append to the end of the device name.
  *
  * \return Pointer to the created rpmsg_dce_dev_t device.
  */

static
rpmsg_omx_dev_t *
_init_rpmsg_omx_device (char * name)
{
    iofunc_attr_t  * attr;
    resmgr_attr_t    resmgr_attr;
    rpmsg_omx_dev_t * dev = NULL;

    dev = malloc(sizeof(*dev));
    if (dev == NULL) {
        return NULL;
    }

    memset(&resmgr_attr, 0, sizeof resmgr_attr);
    resmgr_attr.nparts_max = 10;
    resmgr_attr.msg_max_size = 2048;
    memset(&dev->rpmsg_omx.mattr, 0, sizeof(iofunc_mount_t));
    dev->rpmsg_omx.mattr.flags = ST_NOSUID | ST_NOEXEC;
    dev->rpmsg_omx.mattr.conf = IOFUNC_PC_CHOWN_RESTRICTED |
                              IOFUNC_PC_NO_TRUNC |
                              IOFUNC_PC_SYNC_IO;
    dev->rpmsg_omx.mattr.funcs = &dev->rpmsg_omx.mfuncs;
    memset(&dev->rpmsg_omx.mfuncs, 0, sizeof(iofunc_funcs_t));
    dev->rpmsg_omx.mfuncs.nfuncs = _IOFUNC_NFUNCS;
    dev->rpmsg_omx.mfuncs.ocb_calloc = rpmsg_omx_ocb_calloc;
    dev->rpmsg_omx.mfuncs.ocb_free = rpmsg_omx_ocb_free;
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &dev->rpmsg_omx.cfuncs,
                     _RESMGR_IO_NFUNCS, &dev->rpmsg_omx.iofuncs);
    iofunc_attr_init(attr = &dev->rpmsg_omx.cattr, S_IFCHR | 0777, NULL, NULL);
    dev->rpmsg_omx.iofuncs.devctl = rpmsg_omx_devctl;
    dev->rpmsg_omx.iofuncs.notify = rpmsg_omx_notify;
    dev->rpmsg_omx.iofuncs.close_ocb = rpmsg_omx_close_ocb;
    dev->rpmsg_omx.iofuncs.read = rpmsg_omx_read;
    dev->rpmsg_omx.iofuncs.write = rpmsg_omx_write;
    dev->rpmsg_omx.iofuncs.unblock = rpmsg_omx_read_unblock;
    attr->mount = &dev->rpmsg_omx.mattr;
    iofunc_time_update(attr);
    pthread_mutex_init(&dev->rpmsg_omx.mutex, NULL);

    snprintf (dev->rpmsg_omx.device_name, _POSIX_PATH_MAX, "/dev/%s", name);
    if (-1 == (dev->rpmsg_omx.resmgr_id =
        resmgr_attach(syslink_dpp, &resmgr_attr,
                      dev->rpmsg_omx.device_name, _FTYPE_ANY, 0,
                      &dev->rpmsg_omx.cfuncs,
                      &dev->rpmsg_omx.iofuncs, attr))) {
        pthread_mutex_destroy(&dev->rpmsg_omx.mutex);
        free(dev);
        return(NULL);
    }

    return(dev);
}

/**
 * Callback passed to MessageQCopy_registerNotify.
 *
 * This callback is called when a remote processor creates a MessageQCopy
 * handle with the same name as the local MessageQCopy handle and then
 * calls NameMap_register to notify the HOST of the handle.
 *
 * \param handle    The remote handle.
 * \param procId    The remote proc ID of the remote handle.
 * \param endpoint  The endpoint address of the remote handle.
 *
 * \return None.
 */

static
Void
_rpmsg_omx_notify_cb (MessageQCopy_Handle handle, UInt16 procId,
                      UInt32 endpoint, Char * desc, Bool create)
{
    Int status = 0, i = 0;
    Bool found = FALSE;
    rpmsg_omx_conn_object * obj = NULL;

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_omx_state.objects[i] == NULL) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        /* found a space to save this mq handle, allocate memory */
        obj = Memory_calloc (NULL, sizeof (rpmsg_omx_conn_object), 0x0, NULL);
        if (obj) {
            /* store the object in the module info */
            rpmsg_omx_state.objects[i] = obj;

            /* store the mq info in the object */
            obj->mq = handle;
            obj->procId = procId;
            status = ProcMgr_open(&obj->procH, obj->procId);
            if (status < 0) {
                Osal_printf("Failed to open handle to proc %d", procId);
                Memory_free(NULL, obj, sizeof(rpmsg_omx_object));
            }
            else {
                obj->addr = endpoint;

                /* create a /dev/rpmsg-omx instance for users to open */
                obj->dev = _init_rpmsg_omx_device(desc);
                if (obj->dev == NULL) {
                    Osal_printf("Failed to create %s", desc);
                    ProcMgr_close(&obj->procH);
                    Memory_free(NULL, obj, sizeof(rpmsg_omx_object));
                }
            }
        }
    }
}

 /**
  * Callback passed to MessageQCopy_create for the module.
  *
  * This callback is called when a message is received for the rpmsg-dce
  * module.  This callback will never be called, since each client connection
  * gets it's own endpoint for message passing.
  *
  * \param handle    The local MessageQCopy handle.
  * \param data      Data message
  * \param len       Length of data message
  * \param priv      Private information for the endpoint
  * \param src       Remote endpoint sending this message
  * \param srcProc   Remote proc ID sending this message
  *
  * \return None.
  */

static
Void
_rpmsg_omx_module_cb (MessageQCopy_Handle handle, void * data, int len,
                      void * priv, UInt32 src, UInt16 srcProc)
{
    Osal_printf ("_rpmsg_omx_module_cb callback");
}


/*!
 *  @brief  Module setup function.
 *
 *  @sa     rpmsg_omx_destroy
 */
Int
rpmsg_omx_setup (Void)
{
    UInt16 i;
    List_Params  listparams;
    Int status = 0;
    Error_Block eb;
    pthread_attr_t thread_attr;
    struct sched_param sched_param;

    GT_0trace (curTrace, GT_ENTER, "rpmsg_omx_setup");

    Error_init(&eb);

    List_Params_init (&listparams);
    rpmsg_omx_state.gateHandle = (IGateProvider_Handle)
                     GateSpinlock_create ((GateSpinlock_Handle) NULL, &eb);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (rpmsg_omx_state.gateHandle == NULL) {
        status = OMX_NOMEM;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_rpmsg_omx_setup",
                             status,
                             "Failed to create spinlock gate!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        for (i = 0 ; i < MAX_PROCESSES ; i++) {
            rpmsg_omx_state.eventState [i].bufList = NULL;
            rpmsg_omx_state.eventState [i].omx = NULL;
            rpmsg_omx_state.eventState [i].refCount = 0;
            rpmsg_omx_state.eventState [i].head = NULL;
            rpmsg_omx_state.eventState [i].tail = NULL;
        }

        pthread_attr_init(&thread_attr);
        sched_param.sched_priority = PRIORITY_REALTIME_LOW;
        pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&thread_attr, SCHED_RR);
        pthread_attr_setschedparam(&thread_attr, &sched_param);

        rpmsg_omx_state.run = TRUE;
        if (pthread_create(&rpmsg_omx_state.nt, &thread_attr, notifier_thread, NULL) == EOK) {
            pthread_setname_np(rpmsg_omx_state.nt, "rpmsg-omx-notifier");
            /* Initialize the driver mapping array. */
            Memory_set (&rpmsg_omx_state.objects,
                        0,
                        (sizeof (rpmsg_omx_conn_object *)
                         *  MultiProc_MAXPROCESSORS));
            /* create a local handle and register for notifications with MessageQCopy */
            rpmsg_omx_state.mqHandle = MessageQCopy_create (
                                               MessageQCopy_ADDRANY,
                                               RPMSG_OMX_MODULE_NAME,
                                               _rpmsg_omx_module_cb,
                                               NULL,
                                               &rpmsg_omx_state.endpoint);
            if (rpmsg_omx_state.mqHandle == NULL) {
                /*! @retval OMX_FAIL Failed to create MessageQCopy handle! */
                status = -ENOMEM;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "rpmsg_omx_setup",
                                     status,
                                     "Failed to create MessageQCopy handle!");
            }
            else {
                /* TBD: This could be replaced with a messageqcopy_open type call, one for
                 * each core */
                status = MessageQCopy_registerNotify (rpmsg_omx_state.mqHandle,
                                                      _rpmsg_omx_notify_cb);
                if (status < 0) {
                    MessageQCopy_delete (&rpmsg_omx_state.mqHandle);
                    /*! @retval OMX_FAIL Failed to register MQCopy handle! */
                    status = -ENOMEM;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rpmsg_omx_setup",
                                         status,
                                         "Failed to register MQCopy handle!");
                }
            }
            if (status >= 0){
                rpmsg_omx_state.sem = OsalSemaphore_create(OsalSemaphore_Type_Binary);
                if (rpmsg_omx_state.sem == NULL) {
                    //MessageQCopy_unregisterNotify();
                    /*! @retval OMX_FAIL Failed to register MQCopy handle! */
                    status = OMX_NOMEM;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rpmsg_omx_setup",
                                         status,
                                         "Failed to register MQCopy handle!");
                }
            }
            if (status >= 0) {
                rpmsg_omx_state.isSetup = TRUE;
            }
            else {
                MessageQCopy_delete (&rpmsg_omx_state.mqHandle);
                rpmsg_omx_state.run = FALSE;
            }
        }
        else {
            rpmsg_omx_state.run = FALSE;
        }
        pthread_attr_destroy(&thread_attr);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "rpmsg_omx_setup");
    return status;
}


/*!
 *  @brief  Module destroy function.
 *
 *  @sa     rpmsg_omx_setup
 */
Void
rpmsg_omx_destroy (Void)
{
    rpmsg_omx_EventPacket * packet;
    UInt32                  i;
    List_Handle             bufList;
    rpmsg_omx_object      * omx = NULL;
    WaitingReaders_t      * wr = NULL;
    struct _msg_info        info;

    GT_0trace (curTrace, GT_ENTER, "_rpmsg_omx_destroy");

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_omx_state.objects[i]) {
            rpmsg_omx_conn_object * obj = rpmsg_omx_state.objects[i];
            _deinit_rpmsg_omx_device(obj->dev);
            ProcMgr_close(&obj->procH);
            Memory_free(NULL, obj, sizeof(rpmsg_omx_conn_object));
            rpmsg_omx_state.objects[i] = NULL;
        }
    }

    for (i = 0 ; i < MAX_PROCESSES ; i++) {
        omx = NULL;
        if (rpmsg_omx_state.eventState [i].omx != NULL) {
            /* This is recovery.  Need to mark omx structures as invalid */
            omx = rpmsg_omx_state.eventState[i].omx;
            MessageQCopy_delete(&omx->mq);
            omx->mq = NULL;
        }
        bufList = rpmsg_omx_state.eventState [i].bufList;

        rpmsg_omx_state.eventState [i].bufList = NULL;
        rpmsg_omx_state.eventState [i].omx = NULL;
        rpmsg_omx_state.eventState [i].refCount = 0;
        if (bufList != NULL) {
            /* Dequeue waiting readers and reply to them */
            pthread_mutex_lock(&rpmsg_omx_state.lock);
            while ((wr = dequeue_waiting_reader(i)) != NULL) {
                /* Check if rcvid is still valid */
                if (MsgInfo(wr->rcvid, &info) != -1) {
                    put_wr(wr);
                    pthread_mutex_unlock(&rpmsg_omx_state.lock);
                    MsgError(wr->rcvid, EINTR);
                    pthread_mutex_lock(&rpmsg_omx_state.lock);
                }
            }
            /* Check for pending ionotify/select calls */
            if (omx) {
                if (IOFUNC_NOTIFY_INPUT_CHECK(omx->notify, 1, 0)) {
                    iofunc_notify_trigger(omx->notify, 1, IOFUNC_NOTIFY_INPUT);
                }
            }
            pthread_mutex_unlock(&rpmsg_omx_state.lock);

            /* Free event packets for any received but unprocessed events. */
            while (List_empty (bufList) != TRUE){
                packet = (rpmsg_omx_EventPacket *)
                              List_get (bufList);
                if (packet != NULL){
                    Memory_free (NULL, packet, sizeof(*packet));
                }
            }
            List_delete (&(bufList));
        }
    }

    /* Free the cached list */
    flush_uBuf();

    if (rpmsg_omx_state.sem) {
        OsalSemaphore_delete(&rpmsg_omx_state.sem);
    }

    if (rpmsg_omx_state.mqHandle) {
        //MessageQCopy_unregisterNotify();
        MessageQCopy_delete(&rpmsg_omx_state.mqHandle);
    }

    if (rpmsg_omx_state.gateHandle != NULL) {
        GateSpinlock_delete ((GateSpinlock_Handle *)
                                       &(rpmsg_omx_state.gateHandle));
    }

    rpmsg_omx_state.isSetup = FALSE ;
    rpmsg_omx_state.run = FALSE;
    // run through and destroy the thread, and all outstanding
    // omx structures
    pthread_mutex_lock(&rpmsg_omx_state.lock);
    pthread_cond_signal(&rpmsg_omx_state.cond);
    pthread_mutex_unlock(&rpmsg_omx_state.lock);
    pthread_join(rpmsg_omx_state.nt, NULL);
    pthread_mutex_lock(&rpmsg_omx_state.lock);
    while (rpmsg_omx_state.head != NULL) {
        int index;
        WaitingReaders_t *item;
        index = dequeue_notify_list_item(rpmsg_omx_state.head);
        if (index < 0)
            break;
        item = dequeue_waiting_reader(index);
        while (item) {
            put_wr(item);
            item = dequeue_waiting_reader(index);
        }
    }
    rpmsg_omx_state.head = NULL ;
    rpmsg_omx_state.tail = NULL ;
    pthread_mutex_unlock(&rpmsg_omx_state.lock);

    GT_0trace (curTrace, GT_LEAVE, "_rpmsgDrv_destroy");
}


/** ============================================================================
 *  Internal functions
 *  ============================================================================
 */

