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
 *  #include <ti/ipc/rpmsg/VirtQueue.h>
 *  @endcode
 *
 */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/SysMin.h>

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/hal/Cache.h>
#include <ti/sysbios/knl/Swi.h>

#include <ti/sdo/ipc/notifyDrivers/IInterrupt.h>
#include <ti/sdo/ipc/family/da830/InterruptDsp.h>
#include <ti/ipc/remoteproc/Resource.h>

#include <ti/ipc/MultiProc.h>

#include "package/internal/VirtQueue.xdc.h"

#include <string.h>

#include <ti/ipc/rpmsg/virtio_ring.h>

/* Used for defining the size of the virtqueue registry */
#define NUM_QUEUES                      5

/* Predefined device addresses */
#define IPU_MEM_VRING0          0xc3000000
#define IPU_MEM_VRING1          0xc3004000

#define CONSOLE_VRING0_PA       0xc3008000
#define CONSOLE_VRING1_PA       0xc300c000

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
 */
enum {
    RP_MSG_MBOX_READY           = (Int)0xFFFFFF00,
    RP_MSG_MBOX_STATE_CHANGE    = (Int)0xFFFFFF01,
    RP_MSG_MBOX_CRASH           = (Int)0xFFFFFF02,
    RP_MBOX_ECHO_REQUEST        = (Int)0xFFFFFF03,
    RP_MBOX_ECHO_REPLY          = (Int)0xFFFFFF04,
    RP_MBOX_ABORT_REQUEST       = (Int)0xFFFFFF05,
    RP_MSG_FLUSH_CACHE          = (Int)0xFFFFFF06,
    RP_MSG_HIBERNATION          = (Int)0xFFFFFF07
};

#define DIV_ROUND_UP(n,d)   (((n) + (d) - 1) / (d))
#define RP_MSG_BUFS_SPACE   (VirtQueue_RP_MSG_NUM_BUFS * VirtQueue_RP_MSG_BUF_SIZE * 2)

/* With 256 buffers, our vring will occupy 3 pages */
#define RP_MSG_RING_SIZE    ((DIV_ROUND_UP(vring_size(VirtQueue_RP_MSG_NUM_BUFS, \
                            VirtQueue_RP_MSG_VRING_ALIGN), VirtQueue_PAGE_SIZE)) * VirtQueue_PAGE_SIZE)

/* The total IPC space needed to communicate with a remote processor */
#define RPMSG_IPC_MEM   (RP_MSG_BUFS_SPACE + 2 * RP_MSG_RING_SIZE)

#define ID_DSP_TO_A9      0
#define ID_A9_TO_DSP      1

#define CONSOLE_DSP_TO_A9 2
#define CONSOLE_A9_TO_DSP 3

/* TODO: do we need these to be configurable? */
#define DSPEVENTID              5
#define DSPINT                  5
#define DSP2ARM_CHIPINT0        28

static VirtQueue_Object *queueRegistry[NUM_QUEUES] = {NULL};

static inline Void * mapPAtoVA(UInt pa)
{
    return (Void *)((pa & 0x000fffffU) | 0xc3000000U);
}

static inline UInt mapVAtoPA(Void * va)
{
    return ((UInt)va & 0x000fffffU) | 0xc9000000U;
}

/*
 * ======== VirtQueue_kick ========
 */
Void VirtQueue_kick(VirtQueue_Handle vq)
{
    struct vring *vring = vq->vringPtr;
    IInterrupt_IntInfo intInfo;

    intInfo.remoteIntId = DSP2ARM_CHIPINT0;

    /* For now, simply interrupt remote processor */
    if (vring->avail->flags & VRING_AVAIL_F_NO_INTERRUPT) {
        Log_print0(Diags_USER1,
                "VirtQueue_kick: no kick because of VRING_AVAIL_F_NO_INTERRUPT\n");
        return;
    }

    Log_print2(Diags_USER1,
            "VirtQueue_kick: Sending interrupt to proc %d with payload 0x%x\n",
            (IArg)vq->procId, (IArg)vq->id);
    InterruptDsp_intSend(vq->procId, &intInfo, vq->id);
}

/*
 * ======== VirtQueue_addUsedBuf ========
 */
Int VirtQueue_addUsedBuf(VirtQueue_Handle vq, Int16 head)
{
    struct vring_used_elem *used;
    struct vring *vring = vq->vringPtr;

    if ((head > vring->num) || (head < 0)) {
        Error_raise(NULL, Error_E_generic, 0, 0);
    }

    /*
    * The virtqueue contains a ring of used buffers.  Get a pointer to the
    * next entry in that used ring.
    */
    used = &vring->used->ring[vring->used->idx % vring->num];
    used->id = head;
    used->len = VirtQueue_RP_MSG_BUF_SIZE;

    vring->used->idx++;

    return (0);
}

/*
 * ======== VirtQueue_addAvailBuf ========
 */
Int VirtQueue_addAvailBuf(VirtQueue_Object *vq, Void *buf)
{
    UInt16 avail;
    struct vring *vring = vq->vringPtr;

    if (vq->num_free == 0) {
        /* There's no more space */
        Error_raise(NULL, Error_E_generic, 0, 0);
    }

    vq->num_free--;

    avail =  vring->avail->idx++ % vring->num;

    vring->desc[avail].addr = mapVAtoPA(buf);
    vring->desc[avail].len = VirtQueue_RP_MSG_BUF_SIZE;

    return (vq->num_free);
}

/*
 * ======== VirtQueue_getUsedBuf ========
 */
Void *VirtQueue_getUsedBuf(VirtQueue_Object *vq)
{
    UInt16 head;
    Void *buf;
    struct vring *vring = vq->vringPtr;

    /* There's nothing available? */
    if (vq->last_used_idx == vring->used->idx) {
        return (NULL);
    }

    head = vring->used->ring[vq->last_used_idx % vring->num].id;
    vq->last_used_idx++;
    vq->num_free++;

    buf = mapPAtoVA(vring->desc[head].addr);

    return (buf);
}

/*
 * ======== VirtQueue_getAvailBuf ========
 */
Int16 VirtQueue_getAvailBuf(VirtQueue_Handle vq, Void **buf)
{
    UInt16 head;
    struct vring *vring = vq->vringPtr;

    Log_print6(Diags_USER1, "getAvailBuf vq: 0x%x %d %d %d 0x%x 0x%x\n",
    (IArg)vq,
        vq->last_avail_idx, vring->avail->idx, vring->num,
        (IArg)&vring->avail, (IArg)vring->avail);

    /* There's nothing available? */
    if (vq->last_avail_idx == vring->avail->idx) {
        /* We need to know about added buffers */
        vring->used->flags &= ~VRING_USED_F_NO_NOTIFY;
        /* check again after setting flag */
        if (vq->last_avail_idx == vring->avail->idx)
            return -1;
    }

    /* No need to be kicked about added buffers anymore */
    vring->used->flags |= VRING_USED_F_NO_NOTIFY;

    /*
     * Grab the next descriptor number they're advertising, and increment
     * the index we've seen.
     */
    head = vring->avail->ring[vq->last_avail_idx++ % vring->num];

    *buf = mapPAtoVA(vring->desc[head].addr);

    return (head);
}

/*
 * ======== VirtQueue_isr ========
 * Note 'arg' is ignored: it is the Hwi argument, not the mailbox argument.
 */
Void VirtQueue_isr(UArg msg)
{
    VirtQueue_Object *vq;
    IInterrupt_IntInfo intInfo;

    Log_print1(Diags_USER1, "VirtQueue_isr received msg = 0x%x\n", msg);

    intInfo.localIntId = DSPEVENTID;
    intInfo.remoteIntId = DSP2ARM_CHIPINT0;

    Assert_isTrue((InterruptDsp_isIntSet(msg, &intInfo) == TRUE), NULL);
    InterruptDsp_intClear(msg, &intInfo);

    vq = queueRegistry[0];
    if (vq) {
        vq->callback(vq);
    }
    vq = queueRegistry[1];
    if (vq) {
       vq->callback(vq);
    }
}


/*
 * ======== VirtQueue_create ========
 */
Void VirtQueue_Instance_init(VirtQueue_Object *vq, UInt16 remoteProcId,
                             const VirtQueue_Params *params)
{
    void *vring_phys = NULL;
    Error_Block eb;

    VirtQueue_module->traceBufPtr = Resource_getTraceBufPtr();

    Error_init(&eb);

    vq->vringPtr = Memory_calloc(NULL, sizeof(struct vring), 0, &eb);
    Assert_isTrue((vq->vringPtr != NULL), NULL);

    vq->callback = params->callback;
    vq->id = VirtQueue_module->numQueues++;
    vq->procId = remoteProcId;
    vq->last_avail_idx = 0;
    vq->last_used_idx = 0;
    vq->num_free = VirtQueue_RP_MSG_NUM_BUFS;
    vq->swiHandle = params->swiHandle;

    switch (vq->id) {
    /* sysm3 rpmsg vrings */
        case ID_DSP_TO_A9:
            vring_phys = (struct vring *) IPU_MEM_VRING0;
            break;
        case ID_A9_TO_DSP:
            vring_phys = (struct vring *) IPU_MEM_VRING1;
            break;

    /* sysm3 console vrings */
        case CONSOLE_DSP_TO_A9:
            vring_phys = (struct vring *) CONSOLE_VRING0_PA;
            break;
        case CONSOLE_A9_TO_DSP:
            vring_phys = (struct vring *) CONSOLE_VRING1_PA;
            break;
    }

    Log_print3(Diags_USER1,
            "vring: %d 0x%x (0x%x)\n", vq->id, (IArg)vring_phys,
            RP_MSG_RING_SIZE);

    vring_init(vq->vringPtr, VirtQueue_RP_MSG_NUM_BUFS, vring_phys, VirtQueue_RP_MSG_VRING_ALIGN);

    queueRegistry[vq->id] = vq;
}

/*
 * ======== VirtQueue_startup ========
 */
Void VirtQueue_startup(UInt16 remoteProcId, Bool isHost)
{
    IInterrupt_IntInfo intInfo;

    /*
     * Wait for first kick from host, which happens to coincide with the
     * priming of host's receive buffers, indicating host is ready to send.
     * Since interrupt is cleared, we throw away this first kick, which is
     * OK since we don't process this in the ISR anyway.
     */
    intInfo.intVectorId = DSPINT;
    intInfo.localIntId = DSPEVENTID;
    intInfo.remoteIntId = DSP2ARM_CHIPINT0;  /* ??? don't care??? */
    while (InterruptDsp_isIntSet(remoteProcId, &intInfo) == FALSE);
    InterruptDsp_intClear(remoteProcId, &intInfo);

    /*
     *  DSP can be used to prototype communications with CORE0 instead of
     *  HOST
     */
    if (MultiProc_self() == VirtQueue_dspProcId) {
        InterruptDsp_intRegister(remoteProcId, &intInfo, (Fxn)VirtQueue_isr, NULL);
    }
    Log_print0(Diags_USER1, "Passed VirtQueue_startup\n");
}

/* By convention, Host VirtQueues host are the even number in the pair */
Bool VirtQueue_isSlave(VirtQueue_Handle vq)
{
  return (vq->id & 0x1);
}

Bool VirtQueue_isHost(VirtQueue_Handle vq)
{
  return (~(vq->id & 0x1));
}

UInt16 VirtQueue_getId(VirtQueue_Handle vq)
{
  return (vq->id);
}

Swi_Handle VirtQueue_getSwiHandle(VirtQueue_Handle vq)
{
  return (vq->swiHandle);
}

/*
 * ======== postCrashToMailbox ========
 */
Void postCrashToMailbox(Error_Block * eb)
{
    IInterrupt_IntInfo intInfo;

    intInfo.remoteIntId = DSP2ARM_CHIPINT0;

    Error_print(eb);
    InterruptDsp_intSend(0, &intInfo, (UInt)RP_MSG_MBOX_CRASH);
}


#define CACHE_WB_TICK_PERIOD    5

/*
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

    /* Flush the cache of the SysMin buffer only: */
    Assert_isTrue((VirtQueue_module->traceBufPtr != NULL), NULL);
    Cache_wb(VirtQueue_module->traceBufPtr, SysMin_bufSize, Cache_Type_ALL,
             FALSE);
}
