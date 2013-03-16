/*
 * Copyright (c) 2011-2013, Texas Instruments Incorporated
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
/** ============================================================================
 *  @file       VirtQueue.c
 *
 *  @brief      Virtio Queue implementation for BIOS
 *
 *  Differences between BIOS version and Linux kernel (include/linux/virtio.h):
 *  - Renamed module from virtio.h to VirtQueue_Object.h to match the API prefixes;
 *  - BIOS (XDC) types and CamelCasing used;
 *  - virtio_device concept removed (i.e, assumes no containing device);
 *  - simplified scatterlist from Linux version;
 *  - VirtQueue_Objects are created statically here, so just added a VirtQueue_Object_init()
 *    fxn to take the place of the Virtio vring_new_virtqueue() API;
 *  - The notify function is implicit in the implementation, and not provided
 *    by the client, as it is in Linux virtio.
 *
 *  All VirtQueue operations can be called in any context.
 *
 *  The virtio header should be included in an application as follows:
 *  @code
 *  #include <ti/ipc/family/omap54xx/VirtQueue.h>
 *  @endcode
 *
 */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Cache.h>

#include <ti/ipc/MultiProc.h>

#include <ti/ipc/rpmsg/virtio_ring.h>
#include <ti/pm/IpcPower.h>
#include <string.h>

#include <ti/ipc/rpmsg/_VirtQueue.h>

#include "InterruptProxy.h"
#include "VirtQueue.h"


/* Used for defining the size of the virtqueue registry */
#define NUM_QUEUES              4

/* Predefined device addresses */
#ifndef DSPC674
#define IPC_MEM_VRING0          0xA0000000
#define IPC_MEM_VRING1          0xA0004000
#else
#define IPC_MEM_VRING0          0x9FB00000
#define IPC_MEM_VRING1          0x9FB04000
#endif
#define IPC_MEM_VRING2          0xA0008000
#define IPC_MEM_VRING3          0xA000c000

/*
 * Sizes of the virtqueues (expressed in number of buffers supported,
 * and must be power of two)
 */
#define VQ0_SIZE                256
#define VQ1_SIZE                256
#define VQ2_SIZE                256
#define VQ3_SIZE                256

/*
 * enum - Predefined Mailbox Messages
 *
 * @RP_MSG_MBOX_READY: informs the M3's that we're up and running. will be
 * followed by another mailbox message that carries the A9's virtual address
 * of the shared buffer. This would allow the A9's drivers to send virtual
 * addresses of the buffers.
 *
 * @RP_MSG_MBOX_STATE_CHANGE: informs the receiver that there is an inbound
 * message waiting in its own receive-side vring. please note that currently
 * this message is optional: alternatively, one can explicitly send the index
 * of the triggered virtqueue itself. the preferred approach will be decided
 * as we progress and experiment with those design ideas.
 *
 * @RP_MSG_MBOX_CRASH: this message indicates that the BIOS side is unhappy
 *
 * @RP_MBOX_ECHO_REQUEST: this message requests the remote processor to reply
 * with RP_MBOX_ECHO_REPLY
 *
 * @RP_MBOX_ECHO_REPLY: this is a reply that is sent when RP_MBOX_ECHO_REQUEST
 * is received.
 *
 * @RP_MBOX_ABORT_REQUEST:  tells the M3 to crash on demand
 *
 * @RP_MBOX_BOOTINIT_DONE: this message indicates the BIOS side has reached a
 * certain state during the boot process. This message is used to inform the
 * host that the basic BIOS initialization is done, and lets the host use this
 * notification to perform certain actions.
 */
enum {
    RP_MSG_MBOX_READY           = (Int)0xFFFFFF00,
    RP_MSG_MBOX_STATE_CHANGE    = (Int)0xFFFFFF01,
    RP_MSG_MBOX_CRASH           = (Int)0xFFFFFF02,
    RP_MBOX_ECHO_REQUEST        = (Int)0xFFFFFF03,
    RP_MBOX_ECHO_REPLY          = (Int)0xFFFFFF04,
    RP_MBOX_ABORT_REQUEST       = (Int)0xFFFFFF05,
    RP_MSG_FLUSH_CACHE          = (Int)0xFFFFFF06,
    RP_MSG_BOOTINIT_DONE        = (Int)0xFFFFFF07,
    RP_MSG_HIBERNATION          = (Int)0xFFFFFF10,
    RP_MSG_HIBERNATION_FORCE    = (Int)0xFFFFFF11,
    RP_MSG_HIBERNATION_ACK      = (Int)0xFFFFFF12,
    RP_MSG_HIBERNATION_CANCEL   = (Int)0xFFFFFF13
};

#define DIV_ROUND_UP(n,d)   (((n) + (d) - 1) / (d))
#define RP_MSG_NUM_BUFS     (VQ0_SIZE) /* must be power of two */
#define RP_MSG_BUF_SIZE     (512)
#define RP_MSG_BUFS_SPACE   (RP_MSG_NUM_BUFS * RP_MSG_BUF_SIZE * 2)

#define PAGE_SIZE           (4096)
/*
 * The alignment to use between consumer and producer parts of vring.
 * Note: this is part of the "wire" protocol. If you change this, you need
 * to update your BIOS image as well
 */
#define RP_MSG_VRING_ALIGN  (4096)

/* With 256 buffers, our vring will occupy 3 pages */
#define RP_MSG_RING_SIZE    ((DIV_ROUND_UP(vring_size(RP_MSG_NUM_BUFS, \
                            RP_MSG_VRING_ALIGN), PAGE_SIZE)) * PAGE_SIZE)

/* The total IPC space needed to communicate with a remote processor */
#define RPMSG_IPC_MEM   (RP_MSG_BUFS_SPACE + 2 * RP_MSG_RING_SIZE)

#define ID_SYSM3_TO_A9      ID_SELF_TO_A9
#define ID_A9_TO_SYSM3      ID_A9_TO_SELF
#define ID_DSP_TO_A9        ID_SELF_TO_A9
#define ID_A9_TO_DSP        ID_A9_TO_SELF
#define ID_APPM3_TO_A9      2
#define ID_A9_TO_APPM3      3

typedef struct VirtQueue_Object {
    /* Id for this VirtQueue_Object */
    UInt16                  id;

    /* The function to call when buffers are consumed (can be NULL) */
    VirtQueue_callback      callback;

    /* Shared state */
    struct vring            vring;

    /* Number of free buffers */
    UInt16                  num_free;

    /* Last available index; updated by VirtQueue_getAvailBuf */
    UInt16                  last_avail_idx;

    /* Last available index; updated by VirtQueue_addUsedBuf */
    UInt16                  last_used_idx;

    /* Will eventually be used to kick remote processor */
    UInt16                  procId;
} VirtQueue_Object;

static struct VirtQueue_Object *queueRegistry[NUM_QUEUES] = {NULL};

static UInt16 hostProcId;
#ifndef SMP
static UInt16 dspProcId;
static UInt16 sysm3ProcId;
static UInt16 appm3ProcId;
#endif

#if defined(M3_ONLY) && !defined(SMP)
extern Void OffloadM3_init();
extern Int OffloadM3_processSysM3Tasks(UArg msg);
#endif

static inline Void * mapPAtoVA(UInt pa)
{
    return (Void *)((pa & 0x000fffffU) | IPC_MEM_VRING0);
}

static inline UInt mapVAtoPA(Void * va)
{
    return ((UInt)va & 0x000fffffU) | 0x9cf00000U;
}

/*!
 * ======== VirtQueue_kick ========
 */
Void VirtQueue_kick(VirtQueue_Handle vq)
{
    /* For now, simply interrupt remote processor */
    if (vq->vring.avail->flags & VRING_AVAIL_F_NO_INTERRUPT) {
        Log_print0(Diags_USER1,
                "VirtQueue_kick: no kick because of VRING_AVAIL_F_NO_INTERRUPT\n");
        return;
    }

    Log_print2(Diags_USER1,
            "VirtQueue_kick: Sending interrupt to proc %d with payload 0x%x\n",
            (IArg)vq->procId, (IArg)vq->id);
    InterruptProxy_intSend(vq->procId, vq->id);
}

/*!
 * ======== VirtQueue_addUsedBuf ========
 */
Int VirtQueue_addUsedBuf(VirtQueue_Handle vq, Int16 head, Int len)
{
    struct vring_used_elem *used;

    if ((head > vq->vring.num) || (head < 0)) {
        Error_raise(NULL, Error_E_generic, 0, 0);
    }

    /*
    * The virtqueue contains a ring of used buffers.  Get a pointer to the
    * next entry in that used ring.
    */
    used = &vq->vring.used->ring[vq->vring.used->idx % vq->vring.num];
    used->id = head;
    used->len = len;

    vq->vring.used->idx++;

    return (0);
}

/*!
 * ======== VirtQueue_addAvailBuf ========
 */
Int VirtQueue_addAvailBuf(VirtQueue_Object *vq, Void *buf)
{
    UInt16 avail;

    if (vq->num_free == 0) {
        /* There's no more space */
        Error_raise(NULL, Error_E_generic, 0, 0);
    }

    vq->num_free--;

    avail =  vq->vring.avail->idx++ % vq->vring.num;

    vq->vring.desc[avail].addr = mapVAtoPA(buf);
    vq->vring.desc[avail].len = RP_MSG_BUF_SIZE;

    return (vq->num_free);
}

/*!
 * ======== VirtQueue_getUsedBuf ========
 */
Void *VirtQueue_getUsedBuf(VirtQueue_Object *vq)
{
    UInt16 head;
    Void *buf;

    /* There's nothing available? */
    if (vq->last_used_idx == vq->vring.used->idx) {
        return (NULL);
    }

    head = vq->vring.used->ring[vq->last_used_idx % vq->vring.num].id;
    vq->last_used_idx++;

    buf = mapPAtoVA(vq->vring.desc[head].addr);

    return (buf);
}

/*!
 * ======== VirtQueue_getAvailBuf ========
 */
Int16 VirtQueue_getAvailBuf(VirtQueue_Handle vq, Void **buf, Int *len)
{
    UInt16 head;

    Log_print6(Diags_USER1, "getAvailBuf vq: 0x%x %d %d %d 0x%x 0x%x\n",
        (IArg)vq, vq->last_avail_idx, vq->vring.avail->idx, vq->vring.num,
        (IArg)&vq->vring.avail, (IArg)vq->vring.avail);

    /* There's nothing available? */
    if (vq->last_avail_idx == vq->vring.avail->idx) {
        /* We need to know about added buffers */
        vq->vring.used->flags &= ~VRING_USED_F_NO_NOTIFY;

        return (-1);
    }
    /*
     * Grab the next descriptor number they're advertising, and increment
     * the index we've seen.
     */
    head = vq->vring.avail->ring[vq->last_avail_idx++ % vq->vring.num];

    *buf = mapPAtoVA(vq->vring.desc[head].addr);
    *len = vq->vring.desc[head].len;

    return (head);
}

/*!
 * ======== VirtQueue_disableCallback ========
 */
Void VirtQueue_disableCallback(VirtQueue_Object *vq)
{
    //TODO
    Log_print0(Diags_USER1, "VirtQueue_disableCallback called.");
}

/*!
 * ======== VirtQueue_enableCallback ========
 */
Bool VirtQueue_enableCallback(VirtQueue_Object *vq)
{
    Log_print0(Diags_USER1, "VirtQueue_enableCallback called.");

    //TODO
    return (FALSE);
}

/*!
 * ======== VirtQueue_isr ========
 * Note 'arg' is ignored: it is the Hwi argument, not the mailbox argument.
 */
Void VirtQueue_isr(UArg msg)
{
    VirtQueue_Object *vq;

    Log_print1(Diags_USER1, "VirtQueue_isr received msg = 0x%x\n", msg);

#ifndef SMP
    if (MultiProc_self() == sysm3ProcId || MultiProc_self() == dspProcId) {
#endif
        switch(msg) {
            case (UInt)RP_MSG_MBOX_READY:
                return;

            case (UInt)RP_MBOX_ECHO_REQUEST:
                InterruptProxy_intSend(hostProcId, (UInt)(RP_MBOX_ECHO_REPLY));
                return;

            case (UInt)RP_MBOX_ABORT_REQUEST:
                {
                    /* Suppress Coverity Error: FORWARD_NULL: */
                    // coverity[assign_zero]
                    Fxn f = (Fxn)0x0;
                    Log_print0(Diags_USER1, "Crash on demand ...\n");
                    // coverity[var_deref_op]
                    f();
                }
                return;

            case (UInt)RP_MSG_FLUSH_CACHE:
                Cache_wbAll();
                return;

#ifndef DSPC674
            case (UInt)RP_MSG_HIBERNATION:
                if (IpcPower_canHibernate() == FALSE) {
                    InterruptProxy_intSend(hostProcId,
                                        (UInt)RP_MSG_HIBERNATION_CANCEL);
                    return;
                }

            /* Fall through */
            case (UInt)RP_MSG_HIBERNATION_FORCE:
#ifndef SMP
                /* Core0 should notify Core1 */
                if (MultiProc_self() == sysm3ProcId) {
                    InterruptProxy_intSend(appm3ProcId,
                                           (UInt)(RP_MSG_HIBERNATION));
                }
#endif
                /* Ack request */
                InterruptProxy_intSend(hostProcId,
                                    (UInt)RP_MSG_HIBERNATION_ACK);
                IpcPower_suspend();
                return;
#endif
            default:
#if defined(M3_ONLY) && !defined(SMP)
                /* Check and process any Inter-M3 Offload messages */
                if (OffloadM3_processSysM3Tasks(msg))
                    return;
#endif

                /*
                 *  If the message isn't one of the above, it's either part of the
                 *  2-message synchronization sequence or it a virtqueue message
                 */
                break;
        }
#ifndef SMP
    }
#ifndef DSPC674
    else if (msg & 0xFFFF0000) {
        if (msg == (UInt)RP_MSG_HIBERNATION) {
            IpcPower_suspend();
        }
        return;
    }

    if (MultiProc_self() == sysm3ProcId && (msg == ID_A9_TO_APPM3 || msg == ID_APPM3_TO_A9)) {
        InterruptProxy_intSend(appm3ProcId, (UInt)msg);
    }
    else {
#endif
#endif
        /* Don't let unknown messages to pass as a virtqueue index */
        if (msg >= NUM_QUEUES) {
            /* Adding print here deliberately, we should never see this */
            System_printf("VirtQueue_isr: Invalid mailbox message 0x%x "
                          "received\n", msg);
            return;
        }

        vq = queueRegistry[msg];
        if (vq) {
            vq->callback(vq);
        }
#ifndef SMP
#ifndef DSPC674
    }
#endif
#endif
}


/*!
 * ======== VirtQueue_create ========
 */
VirtQueue_Handle VirtQueue_create(UInt16 remoteProcId, VirtQueue_Params *params,
                                  Error_Block *eb)
{
    VirtQueue_Object *vq;
    Void *vringAddr;

    vq = Memory_alloc(NULL, sizeof(VirtQueue_Object), 0, eb);
    if (NULL == vq) {
        return (NULL);
    }

    vq->callback = params->callback;
    vq->id = params->vqId;
    vq->procId = remoteProcId;
    vq->last_avail_idx = 0;

#ifndef SMP
    if (MultiProc_self() == appm3ProcId) {
        /* vqindices that belong to AppM3 should be big so they don't
         * collide with SysM3's virtqueues */
         vq->id += 2;
    }
#endif

    switch (vq->id) {
        /* IPC transport vrings */
        case ID_SELF_TO_A9:
            /* IPU/DSP -> A9 */
            vringAddr = (struct vring *) IPC_MEM_VRING0;
            break;
        case ID_A9_TO_SELF:
            /* A9 -> IPU/DSP */
            vringAddr = (struct vring *) IPC_MEM_VRING1;
            break;
#ifndef SMP
        case ID_APPM3_TO_A9:
            /* APPM3 -> A9 */
            vringAddr = (struct vring *) IPC_MEM_VRING2;
            break;
        case ID_A9_TO_APPM3:
            /* A9 -> APPM3 */
            vringAddr = (struct vring *) IPC_MEM_VRING3;
            break;
#endif
        default:
            return (NULL);
    }

    Log_print3(Diags_USER1,
            "vring: %d 0x%x (0x%x)\n", vq->id, (IArg)vringAddr,
            RP_MSG_RING_SIZE);

    /* See coverity related comment in vring_init() */
    // coverity[overrun-call]
    vring_init(&(vq->vring), RP_MSG_NUM_BUFS, vringAddr, RP_MSG_VRING_ALIGN);

    /*
     *  Don't trigger a mailbox message every time MPU makes another buffer
     *  available
     */
    if (vq->procId == hostProcId) {
        vq->vring.used->flags |= VRING_USED_F_NO_NOTIFY;
    }

    queueRegistry[vq->id] = vq;

    return (vq);
}

/*!
 * ======== VirtQueue_startup ========
 */
Void VirtQueue_startup()
{
    hostProcId      = MultiProc_getId("HOST");
#ifndef SMP
    dspProcId       = MultiProc_getId("DSP");
    sysm3ProcId     = MultiProc_getId("CORE0");
    appm3ProcId     = MultiProc_getId("CORE1");
#endif

#ifndef DSPC674
    /* Initilize the IpcPower module */
    IpcPower_init();
#endif

#if defined(M3_ONLY) && !defined(SMP)
    if (MultiProc_self() == sysm3ProcId) {
        OffloadM3_init();
    }
#endif

    InterruptProxy_intRegister(VirtQueue_isr);
}

/*!
 * ======== VirtQueue_postCrashToMailbox ========
 */
Void VirtQueue_postCrashToMailbox(Void)
{
    InterruptProxy_intSend(0, (UInt)RP_MSG_MBOX_CRASH);
}

#define CACHE_WB_TICK_PERIOD    5

/*!
 * ======== VirtQueue_cacheWb ========
 *
 * Used for flushing SysMin trace buffer.
 */
Void VirtQueue_cacheWb()
{
    static UInt32 oldticks = 0;
    UInt32 newticks;

    newticks = Clock_getTicks();
    if (newticks - oldticks < (UInt32)CACHE_WB_TICK_PERIOD) {
        /* Don't keep flushing cache */
        return;
    }

    oldticks = newticks;

    /* Flush the cache */
    Cache_wbAll();
}
