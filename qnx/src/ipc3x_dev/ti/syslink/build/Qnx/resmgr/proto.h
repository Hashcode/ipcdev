/*
 *  @file       Proto.h
 *
 *  @brief      common include for resource manager.
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


#ifndef _SYSLINK_PROTO_H_INCLUDED
#define _SYSLINK_PROTO_H_INCLUDED

struct _iofunc_attr;
#define RESMGR_HANDLE_T struct _iofunc_attr
struct syslink_ocb;
#define IOFUNC_OCB_T struct syslink_ocb
#define RESMGR_OCB_T struct syslink_ocb
#define THREAD_POOL_PARAM_T dispatch_context_t
struct syslink_attr;
#define IOFUNC_ATTR_T struct syslink_attr

/* QNX specific header files */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <hw/inout.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <devctl.h>
#include <atomic.h>
#include <sys/slogcodes.h>
#include <ti/syslink/Std.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

#include "ti/syslink/utils/List.h"
#include <OsalThread.h>

#define SYSLINK_DEVICE_PATH        "/dev/syslink"

#define NUM_REMOTE_PROCS 3

/*
 *  Define our device attributes structure.
*/

typedef struct syslink_attr {
    iofunc_attr_t   attr;
    uint16_t        procid;
} syslink_attr_t;

typedef struct named_device {
    iofunc_mount_t      mattr;
    iofunc_attr_t       cattr;
    syslink_attr_t      cattr_trace[NUM_REMOTE_PROCS];
    int                 resmgr_id;
    int                 resmgr_id_trace[NUM_REMOTE_PROCS];
    iofunc_funcs_t      mfuncs;
    resmgr_connect_funcs_t  cfuncs;
    resmgr_connect_funcs_t  cfuncs_trace[NUM_REMOTE_PROCS];
    resmgr_io_funcs_t   iofuncs;
    resmgr_io_funcs_t   iofuncs_trace[NUM_REMOTE_PROCS];
    char device_name[_POSIX_PATH_MAX];
} named_device_t;

typedef struct syslink_dev {
    dispatch_t       * dpp;
    thread_pool_t    * tpool;
    named_device_t     syslink;
    void             * da_virt;
    void             * da_tesla_virt;
    pthread_mutex_t    lock;
    Bool               recover;
    OsalThread_Handle  ipc_recovery_work;
} syslink_dev_t;

typedef struct syslink_ocb {
    iofunc_ocb_t       ocb;
    pid_t              pid;
    uint32_t           ridx;
    uint32_t           widx;
} syslink_ocb_t;

int  syslink_devctl(resmgr_context_t *ctp, io_devctl_t *msg, syslink_ocb_t *ocb);

#endif
