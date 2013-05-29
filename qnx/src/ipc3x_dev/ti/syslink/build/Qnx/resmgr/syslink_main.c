/*
 *  @file       syslink_main.c
 *
 *  @brief      syslink main
 *
 *
 *  @ver        02.00.00.46_alpha1
 *
 *  ============================================================================
 *
 *  Copyright (c) 2011-2013, Texas Instruments Incorporated
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

#include "proto.h"

#include <pthread.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/procmgr.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <signal.h>
#include <stdbool.h>
#if defined(SYSLINK_PLATFORM_OMAP4430)
#include <login.h>
#endif

#include <IpcKnl.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/List.h>
#include <ti/syslink/utils/MemoryOS.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>
#include <OsalSemaphore.h>
#include <ti/syslink/utils/OsalPrint.h>
#if defined(SYSLINK_PLATFORM_OMAP4430) || defined(SYSLINK_PLATFORM_OMAP5430)
#include <_ipu_pm.h>
#endif
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/ProcMgr.h>
#include <Bitops.h>
#include <RscTable.h>
#if (_NTO_VERSION >= 800)
#include <slog2.h>
#endif

#include <ti-ipc.h>

#define DENY_ALL                    \
            PROCMGR_ADN_ROOT        \
            | PROCMGR_ADN_NONROOT   \
            | PROCMGR_AOP_DENY      \
            | PROCMGR_AOP_LOCK

// Ducati trace to slog2 static variables and defines
#define TRACE_BUFFER_SIZE               4096
// polling interval in microseconds
#define TRACE_POLLING_INTERVAL_US       1000000

#if (_NTO_VERSION >= 800)
static int verbosity = SLOG2_ERROR;
static slog2_buffer_t buffer_handle;
#else
static int verbosity = 2;
#endif
static char trace_buffer[TRACE_BUFFER_SIZE];
static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;
static Bool trace_active;
static pthread_t thread_traces;

// Syslink hibernation global variables
Bool syslink_hib_enable = TRUE;
#if !defined(SYSLINK_PLATFORM_OMAP4430) && !defined(SYSLINK_PLATFORM_OMAP5430)
#define PM_HIB_DEFAULT_TIME 5000
#endif
uint32_t syslink_hib_timeout = PM_HIB_DEFAULT_TIME;
Bool syslink_hib_hibernating = FALSE;
pthread_mutex_t syslink_hib_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t syslink_hib_cond = PTHREAD_COND_INITIALIZER;

extern Int rpmsg_rpc_setup (Void);
extern Void rpmsg_rpc_destroy (Void);
extern Void GateHWSpinlock_LeaveLockForPID(int pid);

typedef struct syslink_firmware_info_t {
    uint16_t proc_id;
    char * proc;
    char * firmware;
} syslink_firmware_info;
static syslink_firmware_info syslink_firmware[MultiProc_MAXPROCESSORS];
static unsigned int syslink_num_cores = 0;

int init_ipc(syslink_dev_t * dev, syslink_firmware_info * firmware, bool recover);
int deinit_ipc(syslink_dev_t * dev, bool recover);

static RscTable_Handle rscHandle[MultiProc_MAXPROCESSORS];

static ProcMgr_Handle procH[MultiProc_MAXPROCESSORS];
static unsigned int procH_fileId[MultiProc_MAXPROCESSORS];
static ProcMgr_State errStates[] = {ProcMgr_State_Mmu_Fault,
                                    ProcMgr_State_Error,
                                    ProcMgr_State_Watchdog,
                                    ProcMgr_State_EndValue};

typedef struct syslink_trace_info_t {
    uintptr_t   va;
    uint32_t    len;
    uint32_t *  widx;
    uint32_t *  ridx;
    Bool        firstRead;
} syslink_trace_info;

static syslink_trace_info proc_traces[MultiProc_MAXPROCESSORS];

int syslink_read(resmgr_context_t *ctp, io_read_t *msg, syslink_ocb_t *ocb)
{
    int         nbytes;
    int         nparts;
    int         status;
    int         nleft;
    uint32_t    len;
    uint16_t    procid = ocb->ocb.attr->procid;

    if ((status = iofunc_read_verify (ctp, msg, &ocb->ocb, NULL)) != EOK)
        return (status);

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return (ENOSYS);

    /* check to see where the trace buffer is */
    if (proc_traces[procid].va == NULL) {
        return (ENOSYS);
    }

    /* need to abort ducati trace thread if it is running as only want one reader */
    pthread_mutex_lock(&trace_mutex);
    if (trace_active == TRUE) {
        trace_active = FALSE;
        pthread_mutex_unlock(&trace_mutex);
        // Wake up if waiting on hibernation
        pthread_mutex_lock(&syslink_hib_mutex);
        syslink_hib_hibernating = FALSE;
        pthread_cond_broadcast(&syslink_hib_cond);
        pthread_mutex_unlock(&syslink_hib_mutex);
        pthread_join(thread_traces, NULL);
    } else {
        pthread_mutex_unlock(&trace_mutex);
    }

    if (ocb->ocb.offset == 0) {
        ocb->widx = *(proc_traces[procid].widx);
        ocb->ridx = *(proc_traces[procid].ridx);
        *(proc_traces[procid].ridx) = ocb->widx;
    }

    /* Check for wrap-around */
    if (ocb->widx < ocb->ridx)
        len = proc_traces[procid].len - ocb->ridx + ocb->widx;
    else
        len = ocb->widx - ocb->ridx;

    /* Determine the amount left to print */
    if (ocb->widx >= ocb->ridx)
        nleft = len - ocb->ocb.offset;
    else if (ocb->ocb.offset < proc_traces[procid].len - ocb->ridx)
        nleft = proc_traces[procid].len - ocb->ridx - ocb->ocb.offset;
    else
        nleft = proc_traces[procid].len - ocb->ridx + ocb->widx - ocb->ocb.offset;

    nbytes = min (msg->i.nbytes, nleft);

    /* Make sure the user has supplied a big enough buffer */
    if (nbytes > 0) {
        /* set up the return data IOV */
        if (ocb->widx < ocb->ridx && ocb->ocb.offset >= proc_traces[procid].len - ocb->ridx)
            SETIOV (ctp->iov, (char *)proc_traces[procid].va + ocb->ocb.offset - (proc_traces[procid].len - ocb->ridx), nbytes);
        else
            SETIOV (ctp->iov, (char *)proc_traces[procid].va + ocb->ridx + ocb->ocb.offset, nbytes);

        /* set up the number of bytes (returned by client's read()) */
        _IO_SET_READ_NBYTES (ctp, nbytes);

        ocb->ocb.offset += nbytes;

        nparts = 1;
    }
    else {
        _IO_SET_READ_NBYTES (ctp, 0);

        /* reset offset */
        ocb->ocb.offset = 0;

        nparts = 0;
    }

    /* mark the access time as invalid (we just accessed it) */

    if (msg->i.nbytes > 0)
        ocb->ocb.attr->attr.flags |= IOFUNC_ATTR_ATIME;

    return (_RESMGR_NPARTS (nparts));
}

extern OsalSemaphore_Handle mqcopy_test_sem;

int syslink_unblock(resmgr_context_t *ctp, io_pulse_t *msg, syslink_ocb_t *ocb)
{
    int status = _RESMGR_NOREPLY;
    struct _msg_info info;

    /*
     * Try to run the default unblock for this message.
     */
    if ((status = iofunc_unblock_default(ctp,msg,&(ocb->ocb))) != _RESMGR_DEFAULT) {
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

    if (mqcopy_test_sem)
        OsalSemaphore_post(mqcopy_test_sem);

    return _RESMGR_NOREPLY;
}

IOFUNC_OCB_T *
syslink_ocb_calloc (resmgr_context_t * ctp, IOFUNC_ATTR_T * device)
{
    syslink_ocb_t *ocb = NULL;

    /* Allocate the OCB */
    ocb = (syslink_ocb_t *) calloc (1, sizeof (syslink_ocb_t));
    if (ocb == NULL){
        errno = ENOMEM;
        return (NULL);
    }

    ocb->pid = ctp->info.pid;

    return (IOFUNC_OCB_T *)(ocb);
}

void
syslink_ocb_free (IOFUNC_OCB_T * i_ocb)
{
    syslink_ocb_t * ocb = (syslink_ocb_t *)i_ocb;

    if (ocb) {
        GateHWSpinlock_LeaveLockForPID(ocb->pid);
        free (ocb);
    }
}

int init_syslink_trace_device(syslink_dev_t *dev)
{
    resmgr_attr_t    resmgr_attr;
    int              i;
    syslink_attr_t * trace_attr;
    char             trace_name[_POSIX_PATH_MAX];
    int              status = 0;
    unsigned int     da = 0, pa = 0;
    unsigned int     len;

    memset(&resmgr_attr, 0, sizeof resmgr_attr);
    resmgr_attr.nparts_max = 10;
    resmgr_attr.msg_max_size = 2048;

    for (i = 0; i < syslink_num_cores; i++) {
        iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &dev->syslink.cfuncs_trace[i],
                         _RESMGR_IO_NFUNCS, &dev->syslink.iofuncs_trace[i]);
        trace_attr = &dev->syslink.cattr_trace[i];
        iofunc_attr_init(&trace_attr->attr,
                         S_IFCHR | 0777, NULL, NULL);
        trace_attr->attr.mount = &dev->syslink.mattr;
        trace_attr->procid = i;
        iofunc_time_update(&trace_attr->attr);
        snprintf (dev->syslink.device_name, _POSIX_PATH_MAX,
                  "/dev/ipc-trace%d", syslink_firmware[i].proc_id);
        dev->syslink.iofuncs_trace[i].read = syslink_read;
        snprintf (trace_name, _POSIX_PATH_MAX, "%d", 0);
        pa = 0;
        status = RscTable_getInfo(syslink_firmware[i].proc_id, TYPE_TRACE, 0, &da, &pa, &len);
        if (status == 0) {
            /* last 8 bytes are for writeIdx/readIdx */
            proc_traces[i].len = len - (sizeof(uint32_t) * 2);
            if (da && !pa) {
                /* need to translate da->pa */
                status = ProcMgr_translateAddr (procH[syslink_firmware[i].proc_id],
                                                (Ptr *) &pa,
                                                ProcMgr_AddrType_MasterPhys,
                                                (Ptr) da,
                                                ProcMgr_AddrType_SlaveVirt);
            }
            else {
                GT_setFailureReason(curTrace, GT_4CLASS, "init_syslink_trace_device",
                                    status, "not performing ProcMgr_translate");
            }
            /* map length aligned to page size */
            proc_traces[i].va =
                    mmap_device_io (((len + 0x1000 - 1) / 0x1000) * 0x1000, pa);
            proc_traces[i].widx = (uint32_t *)(proc_traces[i].va + \
                                               proc_traces[i].len);
            proc_traces[i].ridx = (uint32_t *)((uint32_t)proc_traces[i].widx + \
                                               sizeof(uint32_t));
            if (proc_traces[i].va == MAP_DEVICE_FAILED) {
                GT_setFailureReason(curTrace, GT_4CLASS, "init_syslink_trace_device",
                                    status, "mmap_device_io failed");
                GT_1trace(curTrace, GT_4CLASS, "errno %d", errno);
                proc_traces[i].va = NULL;
            }
            proc_traces[i].firstRead = TRUE;
        }
        else {
            GT_setFailureReason(curTrace, GT_4CLASS, "init_syslink_trace_device",
                                status, "RscTable_getInfo failed");
            proc_traces[i].va = NULL;
        }
        if (-1 == (dev->syslink.resmgr_id_trace[i] =
                       resmgr_attach(dev->dpp, &resmgr_attr,
                                     dev->syslink.device_name, _FTYPE_ANY, 0,
                                     &dev->syslink.cfuncs_trace[i],
                                     &dev->syslink.iofuncs_trace[i],
                                     &trace_attr->attr))) {
            GT_setFailureReason(curTrace, GT_4CLASS, "init_syslink_trace_device",
                                status, "resmgr_attach failed");
            return(-1);
        }
    }

    return (status);
}

int deinit_syslink_trace_device(syslink_dev_t *dev)
{
    int status = EOK;
    int i = 0;

    for (i = 0; i < syslink_num_cores; i++) {
        status = resmgr_detach(dev->dpp, dev->syslink.resmgr_id_trace[i], 0);
        if (status < 0) {
            Osal_printf("syslink: resmgr_detach failed %d", errno);
            status = errno;
        }
        if (proc_traces[i].va && proc_traces[i].va != MAP_DEVICE_FAILED) {
            munmap((void *)proc_traces[i].va,
                   ((proc_traces[i].len + 8 + 0x1000 - 1) / 0x1000) * 0x1000);
        }
        proc_traces[i].va = NULL;
    }

    return (status);
}

/* Initialize the syslink device */
int init_syslink_device(syslink_dev_t *dev)
{
    iofunc_attr_t *  attr;
    resmgr_attr_t    resmgr_attr;
    int              status = 0;

    pthread_mutex_init(&dev->lock, NULL);

    memset(&resmgr_attr, 0, sizeof resmgr_attr);
    resmgr_attr.nparts_max = 10;
    resmgr_attr.msg_max_size = 2048;

    memset(&dev->syslink.mattr, 0, sizeof(iofunc_mount_t));
    dev->syslink.mattr.flags = ST_NOSUID | ST_NOEXEC;
    dev->syslink.mattr.conf = IOFUNC_PC_CHOWN_RESTRICTED |
                              IOFUNC_PC_NO_TRUNC |
                              IOFUNC_PC_SYNC_IO;
    dev->syslink.mattr.funcs = &dev->syslink.mfuncs;

    memset(&dev->syslink.mfuncs, 0, sizeof(iofunc_funcs_t));
    dev->syslink.mfuncs.nfuncs = _IOFUNC_NFUNCS;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &dev->syslink.cfuncs,
                    _RESMGR_IO_NFUNCS, &dev->syslink.iofuncs);

    iofunc_attr_init(attr = &dev->syslink.cattr, S_IFCHR | 0777, NULL, NULL);

    dev->syslink.mfuncs.ocb_calloc = syslink_ocb_calloc;
    dev->syslink.mfuncs.ocb_free = syslink_ocb_free;
    dev->syslink.iofuncs.devctl = syslink_devctl;
    dev->syslink.iofuncs.unblock = syslink_unblock;

    attr->mount = &dev->syslink.mattr;
    iofunc_time_update(attr);

    if (-1 == (dev->syslink.resmgr_id =
        resmgr_attach(dev->dpp, &resmgr_attr,
                      IPC_DEVICE_PATH, _FTYPE_ANY, 0,
                      &dev->syslink.cfuncs,
                      &dev->syslink.iofuncs, attr))) {
        return(-1);
    }

    status = init_syslink_trace_device(dev);
    if (status < 0) {
        return status;
    }

    return(0);
}

/* De-initialize the syslink device */
int deinit_syslink_device(syslink_dev_t *dev)
{
    int status = EOK;

    status = resmgr_detach(dev->dpp, dev->syslink.resmgr_id, 0);
    if (status < 0) {
        Osal_printf("syslink: resmgr_detach failed %d", errno);
        status = errno;
    }

    status = deinit_syslink_trace_device(dev);

    return(status);
}


/* Initialize the devices */
int init_devices(syslink_dev_t *dev)
{
    if (init_syslink_device(dev) < 0) {
        Osal_printf("syslink: syslink device init failed");
        return(-1);
    }

    return(0);
}


/* De-initialize the devices */
int deinit_devices(syslink_dev_t *dev)
{
    int status = EOK;

    if ((status = deinit_syslink_device(dev)) < 0) {
        fprintf( stderr, "syslink: syslink device de-init failed %d\n", status);
        status = errno;
    }

    return(status);
}

static void ipc_recover(Ptr args)
{
    syslink_dev_t * dev = (syslink_dev_t *)args;

    deinit_ipc(dev, TRUE);
    init_ipc(dev, syslink_firmware, TRUE);
    deinit_syslink_trace_device(dev);
    init_syslink_trace_device(dev);
}

Int syslink_error_cb (UInt16 procId, ProcMgr_Handle handle,
                      ProcMgr_State fromState, ProcMgr_State toState,
                      ProcMgr_EventStatus status, Ptr args)
{
    Int ret = 0;
    String errString = NULL;
    syslink_dev_t * dev = (syslink_dev_t *)args;

    if (status == ProcMgr_EventStatus_Event) {
        switch (toState) {
            case ProcMgr_State_Mmu_Fault:
                errString = "MMU Fault";
                break;
            case ProcMgr_State_Error:
                errString = "Exception";
                break;
            case ProcMgr_State_Watchdog:
                errString = "Watchdog";
                break;
            default:
                errString = "Unexpected State";
                ret = -1;
                break;
        }
        GT_2trace (curTrace, GT_4CLASS,
                   "syslink_error_cb: Received Error Callback for %s : %s\n",
                   MultiProc_getName(procId), errString);
        /* Don't allow re-schedule of recovery until complete */
        pthread_mutex_lock(&dev->lock);
        if (ret != -1 && dev->recover == FALSE) {
            /* Schedule recovery. */
            dev->recover = TRUE;
            /* Activate a thread to handle the recovery. */
            GT_0trace (curTrace, GT_4CLASS,
                       "syslink_error_cb: Scheduling recovery...");
            OsalThread_activate(dev->ipc_recovery_work);
        }
        else {
            GT_0trace (curTrace, GT_4CLASS,
                       "syslink_error_cb: Recovery already scheduled.");
        }
        pthread_mutex_unlock(&dev->lock);
    }
    else if (status == ProcMgr_EventStatus_Canceled) {
        GT_1trace (curTrace, GT_3CLASS,
                   "SysLink Error Callback Cancelled for %s",
                   MultiProc_getName(procId));
    }
    else {
        GT_1trace (curTrace, GT_4CLASS,
                   "SysLink Error Callback Unexpected Event for %s",
                   MultiProc_getName(procId));
    }

    return ret;
}

#if defined(SYSLINK_PLATFORM_OMAP4430)
#define SYSLINK_CARVEOUT
#ifdef SYSLINK_CARVEOUT
#define IPU_MEM_SIZE  49 * 1024 * 1024
#define IPU_MEM_PHYS  0x97F00000
#else
#define IPU_MEM_SIZE  104 * 1024 * 1024
#define IPU_MEM_ALIGN 0x1000000
#endif
#else
// only need mem for DEVMEM entries, rest is allocated dynamically
#define IPU_MEM_SIZE  90 * 1024 * 1024
#define IPU_MEM_ALIGN 0x0

#endif

unsigned int syslink_ipu_mem_size = IPU_MEM_SIZE;
#if defined(SYSLINK_PLATFORM_OMAP5430)
unsigned int syslink_dsp_mem_size = IPU_MEM_SIZE;
#endif

/*
 * Initialize the syslink ipc
 *
 * This function sets up the "kernel"-side IPC modules, and does any special
 * initialization required for QNX and the platform being used.  This function
 * also registers for error notifications and initializes the recovery thread.
 */
int init_ipc(syslink_dev_t * dev, syslink_firmware_info * firmware, bool recover)
{
    int status = 0;
#if defined(SYSLINK_PLATFORM_OMAP4430) || defined(SYSLINK_PLATFORM_OMAP5430)
    int32_t ret = 0;
    uint32_t len = 0;
#ifndef SYSLINK_CARVEOUT
    int64_t pa = 0;
    void * da;
#endif
    int64_t paddr = 0;
#endif
    Ipc_Config iCfg;
    OsalThread_Params threadParams;
    ProcMgr_AttachParams attachParams;
    UInt16 procId;
    int i;

#if defined(SYSLINK_PLATFORM_OMAP4430) || defined(SYSLINK_PLATFORM_OMAP5430)
    /* Map a contiguous memory section for ipu - currently hard-coded */
    if (!recover) {
#ifdef SYSLINK_CARVEOUT
        dev->da_virt = mmap64(NULL, IPU_MEM_SIZE,
                              PROT_NOCACHE | PROT_READ | PROT_WRITE,
                              MAP_PHYS,
                              NOFD,
                              IPU_MEM_PHYS);
#else
#if defined(SYSLINK_PLATFORM_OMAP5430)
        dev->da_tesla_virt =
#endif
        dev->da_virt = mmap64(NULL, IPU_MEM_SIZE + IPU_MEM_ALIGN,
                              PROT_NOCACHE | PROT_READ | PROT_WRITE,
                              MAP_ANON | MAP_PHYS | MAP_SHARED,
                              NOFD,
                              0);

#endif

        if (dev->da_virt == MAP_FAILED) {
            status = ENOMEM;
            goto exit;
        }
    }

    if (status >= 0) {
#ifdef SYSLINK_CARVEOUT
        /* Make sure the memory is contiguous */
        ret = mem_offset64(dev->da_virt, NOFD, IPU_MEM_SIZE, &paddr, &len);
        if (ret)
            status = ret;
        else if (len != IPU_MEM_SIZE)
            status = ENOMEM;
#else
        /* Make sure the memory is contiguous */
        ret = mem_offset64(dev->da_virt, NOFD, IPU_MEM_SIZE + IPU_MEM_ALIGN,
                           &paddr, &len);
        if (ret)
            status = ret;
        else if (len != IPU_MEM_SIZE + IPU_MEM_ALIGN)
            status = ENOMEM;
        else {
#if defined(SYSLINK_PLATFORM_OMAP4430)
            pa = (paddr + IPU_MEM_ALIGN - 1) / IPU_MEM_ALIGN * IPU_MEM_ALIGN;
            if ((pa - paddr) < 0x900000)
                pa += 0x900000;
            else
                pa -= 0x700000;
            da = dev->da_virt + (pa - paddr);
#else
            pa = paddr;
            da = dev->da_virt;
#endif
        }
#endif
        if (status != 0)
            goto memoryos_fail;
    }
#endif

#if defined(SYSLINK_PLATFORM_OMAP5430)
    if (status >= 0) {
        iCfg.pAddr_dsp = (uint32_t)pa;
        iCfg.vAddr_dsp = (uint32_t)da;
    }
#endif
    if (status >= 0) {
        if (!recover) {
            /* Set up the MemoryOS module */
            status = MemoryOS_setup();
            if (status < 0)
                goto memoryos_fail;
        }

        /* Setup IPC and platform-specific items */
#if defined(SYSLINK_PLATFORM_OMAP4430) || defined(SYSLINK_PLATFORM_OMAP5430)
#ifdef SYSLINK_CARVEOUT
        iCfg.vAddr = (uint32_t)dev->da_virt;
        iCfg.pAddr = (uint32_t)paddr;
#else
        iCfg.vAddr = (uint32_t)da;
        iCfg.pAddr = (uint32_t)pa;
#endif
#endif
        status = Ipc_setup (&iCfg);
        if (status < 0)
            goto ipcsetup_fail;

        /* NOTE: this is for handling the procmgr event notifications to userspace list */
        if (!recover) {
            /* Setup Fault recovery items. */
            /* Create the thread object used for the interrupt handler. */
            threadParams.priority     = OsalThread_Priority_Medium;
            threadParams.priorityType = OsalThread_PriorityType_Generic;
            threadParams.once         = FALSE;
            dev->ipc_recovery_work = OsalThread_create ((OsalThread_CallbackFxn)
                                                        ipc_recover,
                                                        dev,
                                                        &threadParams);
            if (dev->ipc_recovery_work == NULL)
                goto osalthreadcreate_fail;
        }
        else {
            pthread_mutex_lock(&dev->lock);
            dev->recover = FALSE;
            pthread_mutex_unlock(&dev->lock);
        }

        for (i = 0; i < syslink_num_cores; i++) {
            procId = firmware[i].proc_id = MultiProc_getId(firmware[i].proc);
            if (procId >= MultiProc_MAXPROCESSORS || procH[procId]) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "init_ipc",
                                     status,
                                     "invalid proc!");
                break;
            }

            if (syslink_firmware[i].firmware) {
                rscHandle[procId] = RscTable_alloc(firmware[i].firmware, procId);
                if (rscHandle[procId] == NULL) {
                    status = -1;
                    break;
                }
            }

            status = ProcMgr_open(&procH[procId], procId);
            if (status < 0 || procH[procId] == NULL)
                goto procmgropen_fail;

            /* Load and start the remote processor. */
            ProcMgr_getAttachParams (procH[procId], &attachParams);
            status = ProcMgr_attach (procH[procId], &attachParams);
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "init_ipc",
                                     status,
                                     "ProcMgr_attach failed!");
                goto procmgrattach_fail;
            }

            if (syslink_firmware[i].firmware) {
                status = ProcMgr_load (procH[procId],
                                       (String)firmware[i].firmware, 0, NULL,
                                        NULL, &procH_fileId[procId]);
                if (status < 0) {
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "init_ipc",
                                         status,
                                         "ProcMgr_load failed!");
                    goto procmgrload_fail;
                }
            }

            status = Ipc_attach (procId);
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "init_ipc",
                                     status,
                                     "Ipc_attach failed!");
                goto ipcattach_fail;
            }

            status = ProcMgr_start(procH[procId], NULL);
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "init_ipc",
                                     status,
                                     "ProcMgr_start failed!");
                goto procmgrstart_fail;
            }

            status = ProcMgr_registerNotify(procH[procId], syslink_error_cb, (Ptr)dev,
                                            -1, errStates);
            if (status < 0)
                goto procmgrreg_fail;

            continue;

procmgrreg_fail:
            ProcMgr_stop(procH[procId]);
procmgrstart_fail:
            Ipc_detach(procId);
ipcattach_fail:
            if (syslink_firmware[i].firmware)
                ProcMgr_unload(procH[procId], procH_fileId[procId]);
procmgrload_fail:
            ProcMgr_detach(procH[procId]);
procmgrattach_fail:
            ProcMgr_close(&procH[procId]);
            procH[procId] = NULL;
procmgropen_fail:
            RscTable_free(&rscHandle[procId]);
            break;
        }

        if (status < 0)
            goto tiipcsetup_fail;

        /* Set up rpmsg_mq */
        status = ti_ipc_setup();
        if (status < 0)
            goto tiipcsetup_fail;

        /* Set up rpmsg_rpc */
        status = rpmsg_rpc_setup();
        if (status < 0)
            goto rpcsetup_fail;

        goto exit;
    }

rpcsetup_fail:
    ti_ipc_destroy(recover);
tiipcsetup_fail:
    for (i-=1; i >= 0; i--) {
        procId = firmware[i].proc_id;
        ProcMgr_unregisterNotify(procH[procId], syslink_error_cb,
                                (Ptr)dev, errStates);
        ProcMgr_stop(procH[procId]);
        if (procH_fileId[procId]) {
            ProcMgr_unload(procH[procId], procH_fileId[procId]);
            procH_fileId[procId] = 0;
        }
        ProcMgr_detach(procH[procId]);
        ProcMgr_close(&procH[procId]);
        procH[procId] = NULL;
        RscTable_free(&rscHandle[procId]);
        rscHandle[procId] = NULL;
    }
    OsalThread_delete(&dev->ipc_recovery_work);
osalthreadcreate_fail:
    Ipc_destroy();
ipcsetup_fail:
    MemoryOS_destroy();
memoryos_fail:
#if defined(SYSLINK_PLATFORM_OMAP4430) || defined(SYSLINK_PLATFORM_OMAP5430)
    if (dev->da_virt != MAP_FAILED)
#ifdef SYSLINK_CARVEOUT
        munmap(dev->da_virt, IPU_MEM_SIZE);
#else
        munmap(dev->da_virt, IPU_MEM_SIZE + IPU_MEM_ALIGN);
#endif
#endif
exit:
    return status;
}

int deinit_ipc(syslink_dev_t * dev, bool recover)
{
    int status = EOK;
    uint32_t i = 0, id = 0;

    // Stop the remote cores right away
    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (procH[i]) {
            GT_1trace(curTrace, GT_4CLASS, "stopping %s", MultiProc_getName(i));
            ProcMgr_stop(procH[i]);
        }
    }

    rpmsg_rpc_destroy();

    ti_ipc_destroy(recover);

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (procH[i]) {
            ProcMgr_unregisterNotify (procH[i], syslink_error_cb, (Ptr)dev,
                                      errStates);
            Ipc_detach(i);
            if (procH_fileId[i]) {
                ProcMgr_unload(procH[i], procH_fileId[i]);
                procH_fileId[i] = 0;
            }
            ProcMgr_detach(procH[i]);
            ProcMgr_close(&procH[i]);
            procH[i] = NULL;
            RscTable_free(&rscHandle[i]);
            rscHandle[i] = NULL;
        }
    }

    if (!recover && dev->ipc_recovery_work != NULL) {
        OsalThread_delete (&dev->ipc_recovery_work);
        dev->ipc_recovery_work = NULL;
    }

    if (recover) {
        static FILE *log;
        /* Dump the trace information */
        Osal_printf("syslink: printing remote core trace dump to"
                    " /var/log/ducati-m3-traces.log");
        log = fopen ("/var/log/ducati-m3-traces.log", "a+");
        if (log) {
            for (id = 0; id < syslink_num_cores; id++) {
                if (proc_traces[id].va) {
                    /* print traces */
                    /* wait a little bit for traces to finish dumping */
                    sleep(1);
                    fprintf(log, "****************************************\n");
                    fprintf(log, "***         CORE%d TRACE DUMP         ***\n",
                            id);
                    fprintf(log, "****************************************\n");
                    for (i = (*proc_traces[id].widx + 1);
                         i < (proc_traces[id].len - 8);
                         i++) {
                        fprintf(log, "%c",
                                *(char *)((uint32_t)proc_traces[id].va + i));
                    }
                    for (i = 0; i < *proc_traces[id].widx; i++) {
                        fprintf(log, "%c",
                                *(char *)((uint32_t)proc_traces[id].va + i));
                    }
                }
            }
            fclose(log);
        }
        else {
            GT_setFailureReason(curTrace, GT_4CLASS, "deinit_ipc", errno,
                                "error opening /var/log/ducati-m3-traces.log");
        }
    }

    status = Ipc_destroy();
    if (status < 0) {
        printf("Ipc_destroy() failed 0x%x", status);
    }
    if (!recover) {
        status = MemoryOS_destroy();
        if (status < 0) {
            printf("MemoryOS_destroy() failed 0x%x", status);
        }
#if defined(SYSLINK_PLATFORM_OMAP4430) || defined(SYSLINK_PLATFORM_OMAP5430)
        if (dev->da_virt != MAP_FAILED) {
#ifdef SYSLINK_CARVEOUT
            status = munmap(dev->da_virt, IPU_MEM_SIZE);
#else
            status = munmap(dev->da_virt, IPU_MEM_SIZE + IPU_MEM_ALIGN);
#endif
            if (status < 0) {
               printf("munmap failed %d", errno);
            }
        }
#endif
    }

    return status;
}


/* Read next line of available data for given 'core' and store it in buffer.
 * Returns the number of bytes that were written or -1 on error
 */
static int readNextTrace(int core, char* buffer, int bufSize)
{
    char* readPtr;
    uint32_t readBytes, ridx, widx;
    syslink_trace_info* tinfo = &proc_traces[core];

    /* Make sure it is valid */
    if ( (tinfo == NULL) || (tinfo->va == NULL) ) {
        return -1;
    }

    /* Check to see if something to read */
    if (tinfo->ridx == tinfo->widx) {
        return 0;
    }

    readPtr = (char*) tinfo->va;
    ridx = *tinfo->ridx;
    widx = *tinfo->widx;

    /* If first read, make sure that core is ready by validating ridx, widx */
    if ( (tinfo->firstRead == TRUE) && ((ridx != 0) || (widx >= tinfo->len)) ) {
        // not ready - will try again later
        return 0;
    }

    /* Sanity check ridx/widx to make sure they point inside the buffer */
    if ( (ridx >= tinfo->len) || (widx >= tinfo->len) ) {
        Osal_printf("C%d: widx=%d, ridx=%d, len=%d - out of range",
            core, widx, ridx, tinfo->len);
        return -1;
    }

    readBytes = 0;
    tinfo->firstRead = FALSE;
    /* Read until we hit newline indicating end of trace */
    while ( (readPtr[ridx] != '\n') && (ridx != widx) && (readBytes < bufSize)) {
        buffer[readBytes] = readPtr[ridx];
        readBytes++;
        ridx++;
        // Check for wrap-around
        if (ridx == tinfo->len) {
            ridx = 0;
        }
    }

    /* If did not find newline, abort since either not enough info or no room in buffer */
    if (readPtr[ridx] != '\n') {
        if (readBytes >= bufSize) {
            Osal_printf("C%d: Insufficient size of buffer; read %d, buf %d",
                core, readBytes, bufSize);
            return -1;
        }
        return 0;
    }

    /* Newline may not be valid data if this was not ready to be read */
    if (ridx == widx) {
        return 0;
    }

    /* We read a full line - null terminate and update ridx to mark data read */
    if (readBytes < bufSize) {
        buffer[readBytes] = '\0';
    } else {
        Osal_printf("C%d: No room to write NULL character", core);
        return -1;
    }
    readBytes++;
    ridx++;
    if (ridx == tinfo->len) {
        ridx = 0;
    }
    *tinfo->ridx = ridx;

    return readBytes;
}

/* Thread reading ducati traces and writing them out to slog2 */
static void *ducatiTraceThread(void *parm)
{
    int32_t bytesRead;
    int core;
    int err;
    Bool exit = FALSE;

    pthread_setname_np(0, "ducati-trace");

    pthread_mutex_lock(&trace_mutex);
    while ( (trace_active == TRUE) && (exit == FALSE) ) {
        for (core = 0; core < MultiProc_MAXPROCESSORS; core++) {
            while ((bytesRead = readNextTrace(core, trace_buffer, TRACE_BUFFER_SIZE)) > 0) {
#if (_NTO_VERSION >= 800)
                slog2f(buffer_handle, 0, 0, "C%d:%s", core, trace_buffer);
#else
                slogf(42, _SLOG_NOTICE, "C%d:%s", core, trace_buffer);
#endif
                if (trace_active == FALSE) {
                    break;
                }
            }
            // Abort trace logger on errors as these should not occur
            if (bytesRead < 0) {
                trace_active = FALSE;
            }
            if (trace_active == FALSE) {
                break;
            }
        }
        if (trace_active == FALSE) {
            continue;
        }
        pthread_mutex_unlock(&trace_mutex);

        // No interrupts/events to trigger reading traces, so need to periodically poll
        usleep(TRACE_POLLING_INTERVAL_US);

        // If we are in hibernation, wait on condvar for end of hibernation
        pthread_mutex_lock(&syslink_hib_mutex);
        while ((syslink_hib_enable == TRUE) && (syslink_hib_hibernating == TRUE) ) {
            err = pthread_cond_wait(&syslink_hib_cond, &syslink_hib_mutex);
            if (err != EOK) {
                Osal_printf("pthread_cond_wait failed with err=%d", err);
                exit = TRUE;
                break;
            }
        }
        pthread_mutex_unlock(&syslink_hib_mutex);

        pthread_mutex_lock(&trace_mutex);
    }
    pthread_mutex_unlock(&trace_mutex);
    Osal_printf("ducati trace thread exited");
    return NULL;
}

/* Initialize slog2 for Ducati traces */
static int init_ducati_slog2(void)
{
#if (_NTO_VERSION >= 800)
    slog2_buffer_set_config_t buffer_config;
    const char * buffer_set_name = "ducati";
    const char * buffer_name = "ducati_buffer";

    // Use command line verbosity for default verbosity level
    uint8_t verbosity_level = (uint8_t) verbosity;
    if ( verbosity_level > SLOG2_DEBUG2) {
        verbosity_level = SLOG2_DEBUG2;
    }

    // Initialize the buffer configuration
    buffer_config.buffer_set_name = (char *) buffer_set_name;
    buffer_config.num_buffers = 1;
    buffer_config.verbosity_level = verbosity_level;
    buffer_config.buffer_config[0].buffer_name = (char *) buffer_name;
    buffer_config.buffer_config[0].num_pages = 8;

    // Register the Buffer Set
    if( slog2_register( &buffer_config, &buffer_handle, 0 ) == -1 ) {
        Osal_printf("syslink error registering slogger2 buffer for Ducati!");
        return -1;
    }
#endif

    return 0;
}

/** print usage */
static Void printUsage (Char * app)
{
    printf ("\n%s: [-fHT]\n", app);
    printf ("  -f   specify the binary file to load to the remote cores)\n");
#if defined(SYSLINK_PLATFORM_OMAP5430)
    printf ("  -d   specify the binary file to load to the dsp)\n");
#endif
    printf ("  -H   enable/disable hibernation, 1: ON, 0: OFF, Default: 1)\n");
    printf ("  -T   specify the hibernation timeout in ms, Default: 5000 ms)\n");

    exit (EXIT_SUCCESS);
}

dispatch_t * syslink_dpp = NULL;

int main(int argc, char *argv[])
{
    syslink_dev_t * dev = NULL;
    thread_pool_attr_t tattr;
    int status;
    int error = 0;
    sigset_t set;
    int channelid = 0;
    int c;
    int hib_enable = 1;
    uint32_t hib_timeout = PM_HIB_DEFAULT_TIME;
    char *user_parm = NULL;
    struct stat          sbuf;

    if (-1 != stat(IPC_DEVICE_PATH, &sbuf)) {
        printf ("Syslink Already Running...\n");
        return EXIT_FAILURE;
    }
    printf ("Starting syslink resource manager...\n");

    /* Parse the input args */
    while (1)
    {
        c = getopt (argc, argv, "f:d:H:T:U:v:");
        if (c == -1)
            break;

        switch (c)
        {
        case 'f':
            /* for backward compatibility, "-f" option loaded Ducati/Benelli */
            syslink_firmware[syslink_num_cores].firmware = optarg;
#if defined(SYSLINK_PLATFORM_OMAP4430)
            syslink_firmware[syslink_num_cores].proc = "SYSM3";
#else
#ifndef SYSLINK_SYSBIOS_SMP
            syslink_firmware[syslink_num_cores].proc = "CORE0";
#else
            syslink_firmware[syslink_num_cores].proc = "IPU";
#endif
#endif
            syslink_num_cores++;
#ifndef SYSLINK_SYSBIOS_SMP
            syslink_firmware[syslink_num_cores].firmware = NULL;
#if defined(SYSLINK_PLATFORM_OMAP4430)
            syslink_firmware[syslink_num_cores].proc = "APPM3";
#else
            syslink_firmware[syslink_num_cores].proc = "CORE1";
#endif
            syslink_num_cores++;
#endif
            break;
#if defined(SYSLINK_PLATFORM_OMAP5430)
        case 'd':
            syslink_firmware[syslink_num_cores].firmware = optarg;
            syslink_firmware[syslink_num_cores].proc = "DSP";
            syslink_num_cores++;
            break;
#endif
        case 'H':
            hib_enable = atoi(optarg);
            if (hib_enable != 0 && hib_enable != 1) {
                hib_enable = -1;
            }
            break;
        case 'T':
            hib_timeout = atoi(optarg);
            break;
        case 'U':
            user_parm = optarg;
            break;
        case 'v':
            verbosity++;
            break;
        default:
            fprintf (stderr, "Unrecognized argument\n");
        }
    }

    /* Now parse the operands, which should be in the format:
     * "<multiproc_name> <firmware_file> ..*/
    for (; optind + 1 < argc; optind+=2) {
        if (syslink_num_cores == MultiProc_MAXPROCESSORS) {
            printUsage(argv[0]);
            return (error);
        }
        syslink_firmware[syslink_num_cores].proc = argv [optind];
        syslink_firmware[syslink_num_cores++].firmware = argv [optind+1];
    }


    /* Get the name of the binary from the input args */
    if (!syslink_num_cores) {
        fprintf(stderr, "-f or -d or <core_id> option must be specified");
        printUsage(argv[0]);
        return (error);
    }

    /* Validate hib_enable args */
    if (hib_enable == -1) {
        fprintf (stderr, "invalid hibernation enable value\n");
        printUsage(argv[0]);
        return (error);
    }

    syslink_hib_enable = (Bool)hib_enable;
    syslink_hib_timeout = hib_timeout;

    /* Init logging for syslink */
    if (Osal_initlogging(verbosity) != 0) {
        return -1;
    }

    /* Obtain I/O privity */
    error = ThreadCtl_r (_NTO_TCTL_IO, 0);
    if (error == -1) {
        Osal_printf("Unable to obtain I/O privity");
        return (error);
    }

    /* allocate the device structure */
    if (NULL == (dev = calloc(1, sizeof(syslink_dev_t)))) {
        Osal_printf("syslink: calloc() failed");
        return (-1);
    }

    /* create the channel */
    if ((channelid = ChannelCreate_r (_NTO_CHF_UNBLOCK |
                                    _NTO_CHF_DISCONNECT |
                                    _NTO_CHF_COID_DISCONNECT |
                                    _NTO_CHF_REPLY_LEN |
                                    _NTO_CHF_SENDER_LEN)) < 0) {
        Osal_printf("Unable to create channel %d", channelid);
        return (channelid);
    }

    /* create the dispatch structure */
    if (NULL == (dev->dpp = syslink_dpp = dispatch_create_channel (channelid, 0))) {
        Osal_printf("syslink:  dispatch_create() failed");
        return(-1);
    }

    /* Initialize the thread pool */
    memset (&tattr, 0x00, sizeof (thread_pool_attr_t));
    tattr.handle = dev->dpp;
    tattr.context_alloc = dispatch_context_alloc;
    tattr.context_free = dispatch_context_free;
    tattr.block_func = dispatch_block;
    tattr.unblock_func = dispatch_unblock;
    tattr.handler_func = dispatch_handler;
    tattr.lo_water = 2;
    tattr.hi_water = 4;
    tattr.increment = 1;
    tattr.maximum = 10;

    /* Create the thread pool */
    if ((dev->tpool = thread_pool_create(&tattr, 0)) == NULL) {
        Osal_printf("syslink: thread pool create failed");
        return(-1);
    }

    /* init syslink */
    status = init_ipc(dev, syslink_firmware, FALSE);
    if (status < 0) {
        Osal_printf("syslink: IPC init failed");
        return(-1);
    }

    /* init the syslink device */
    status = init_devices(dev);
    if (status < 0) {
        Osal_printf("syslink: device init failed");
        return(-1);
    }

#if (_NTO_VERSION >= 800)
    /* Relinquish privileges */
    status = procmgr_ability(  0,
                 DENY_ALL | PROCMGR_AID_SPAWN,
                 DENY_ALL | PROCMGR_AID_FORK,
                 PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_MEM_PEER,
                 PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_MEM_PHYS,
                 PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_INTERRUPT,
                 PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_PATHSPACE,
                 PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_RSRCDBMGR,
                 PROCMGR_ADN_ROOT | PROCMGR_AOP_ALLOW | PROCMGR_AOP_LOCK | PROCMGR_AOP_SUBRANGE | PROCMGR_AID_SETUID,
                 (uint64_t)1, (uint64_t)~0,
                 PROCMGR_ADN_ROOT | PROCMGR_AOP_ALLOW | PROCMGR_AOP_LOCK | PROCMGR_AOP_SUBRANGE | PROCMGR_AID_SETGID,
                 (uint64_t)1, (uint64_t)~0,
                 PROCMGR_ADN_ROOT | PROCMGR_AOP_DENY | PROCMGR_AOP_LOCK | PROCMGR_AID_EOL);

    if (status != EOK) {
        Osal_printf("procmgr_ability failed! errno=%d", status);
        return EXIT_FAILURE;
    }

    /* Reduce priority to either what defined from command line or at least nobody */
    if (user_parm != NULL) {
        if (set_ids_from_arg(user_parm) < 0) {
            Osal_printf("unable to set uid/gid - %s", strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        if (setuid(99) != 0) {
            Osal_printf("unable to set uid - %s", strerror(errno));
            return EXIT_FAILURE;
        }
    }
#endif

    /* make this a daemon process */
    if (-1 == procmgr_daemon(EXIT_SUCCESS,
        PROCMGR_DAEMON_NOCLOSE | PROCMGR_DAEMON_NODEVNULL)) {
        Osal_printf("syslink:  procmgr_daemon() failed");
        return(-1);
    }

    /* start the thread pool */
    thread_pool_start(dev->tpool);

    /* Init slog2 and thread for Ducati traces */
    if (init_ducati_slog2() != 0) {
        return -1;
    }
    trace_active = TRUE;
    status = pthread_create (&thread_traces, NULL, ducatiTraceThread, NULL);
    if (status != EOK) {
        Osal_printf("pthread_create for trace thread failed err=%d", status);
        trace_active = FALSE;
    }

    /* Mask out unnecessary signals */
    sigfillset (&set);
    sigdelset (&set, SIGINT);
    sigdelset (&set, SIGTERM);
    pthread_sigmask (SIG_BLOCK, &set, NULL);

    /* Wait for one of these signals */
    sigemptyset (&set);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);

    Osal_printf("Syslink resource manager started");

    /* Wait for a signal */
    while (1)
    {
        switch (SignalWaitinfo (&set, NULL))
        {
            case SIGTERM:
            case SIGQUIT:
            case SIGINT:
                error = EOK;
                goto done;

            default:
                break;
        }
    }

    error = EOK;

done:
    GT_0trace(curTrace, GT_4CLASS, "Syslink resource manager exiting \n");
    /* Stop ducatiTraceThread if running */
    pthread_mutex_lock(&trace_mutex);
    if (trace_active) {
        trace_active = FALSE;
        pthread_mutex_unlock(&trace_mutex);
        // Wake up if waiting on hibernation
        pthread_mutex_lock(&syslink_hib_mutex);
        syslink_hib_hibernating = FALSE;
        pthread_cond_broadcast(&syslink_hib_cond);
        pthread_mutex_unlock(&syslink_hib_mutex);
        error = pthread_join(thread_traces, NULL);
        if (error < 0) {
            Osal_printf("syslink: pthread_join failed with err=%d", error);
        }
    } else {
        pthread_mutex_unlock(&trace_mutex);
    }

    error = thread_pool_destroy(dev->tpool);
    if (error < 0)
        Osal_printf("syslink: thread_pool_destroy returned an error");
    deinit_ipc(dev, FALSE);
    deinit_devices(dev);
    free(dev);
    return (EOK);
}
