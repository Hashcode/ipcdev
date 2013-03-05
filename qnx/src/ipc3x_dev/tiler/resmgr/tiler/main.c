/*
 * Copyright (c) 2010, Texas Instruments Incorporated
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
 * */

#include "proto.h"

#include <stdarg.h>
#include <signal.h>
#if defined(TILER_PLATFORM_OMAP4)
#include <login.h>
#endif

#define DENY_ALL                    \
            PROCMGR_ADN_ROOT        \
            |PROCMGR_ADN_NONROOT    \
            |PROCMGR_AOP_DENY       \
            |PROCMGR_AOP_LOCK

static resmgr_connect_funcs_t    connect_funcs;
static resmgr_io_funcs_t         io_funcs;
static iofunc_mount_t            mattr;
static iofunc_funcs_t            ocb_funcs;
static iofunc_attr_t             attr;
static volatile unsigned done = 0;

static int lowmem_limit = 128 * 1024 * 1024;
static int lowmem_current = INT_MAX;
#if _NTO_VERSION >= 660
static int lowmem_id = -1;
static int lowmem_pulse_prio = -1;
#endif

int tiler_devctl(resmgr_context_t *ctp, io_devctl_t *msg, tiler_ocb_t *ocb);
int tiler_read(resmgr_context_t *ctp, io_read_t *msg, tiler_ocb_t *ocb);

#if _NTO_VERSION >= 660
static int
tiler_lowmem ( message_context_t *ctp, int code, unsigned flags, void *handle )
{
    lowmem_current = procmgr_value_current(lowmem_id);
#if 0
    slogf(99, 1, "current lowmem is %dM, limit is %dM", lowmem_current/1024/1024, lowmem_limit/1024/1024 );
#endif
    if ( tiler_islowmem() ) {
        tiler_purge();
    }
    return EOK;
}
#endif

int tiler_islowmem(void)
{
    return lowmem_current < lowmem_limit;
}

int tiler_lowmem_init(dispatch_t *dpp)
{
#if _NTO_VERSION >= 660
    int code, coid;
    struct sigevent ev_lowmem;

    if((code = pulse_attach(dpp, MSG_FLAG_ALLOC_PULSE, 0, &tiler_lowmem, NULL)) == -1) {
        perror("tiler: Unable to setup lowmem event");
        return -1;
    }

    if ((coid = message_connect(dpp, MSG_FLAG_SIDE_CHANNEL)) == -1) {
        perror("tiler: Unable to connect to dpp channel");
        return -1;
    }

    SIGEV_PULSE_INIT(&ev_lowmem, coid, lowmem_pulse_prio, code, 0);

    lowmem_id = procmgr_value_notify_add(PROCMGR_VALUE_FREE_MEM|PROCMGR_VALUE_TRIGGER_DOWN|PROCMGR_VALUE_TRIGGER_UP, 0, lowmem_limit, &ev_lowmem);
    if ( lowmem_id == -1 ) {
        perror("tiler: Unable to register to low memory event");
        return -1;
    }
#endif
    return 0;
}

int
main(int argc, char *const argv[])
{
    /* declare variables we'll be using */
    resmgr_attr_t        resmgr_attr;
    dispatch_t           *dpp;
    int                  id;
    int                  ret = 0;
    struct stat          sbuf;
    thread_pool_attr_t   tattr;
    thread_pool_t        *tpool;
    sigset_t             set;
    int32_t              size = 0;
    int                  c;
    char                 *user_parm = NULL;

    while (1)
    {
        c = getopt (argc, argv, "S:L:U:");
        if (c == -1)
            break;

        switch (c)
        {
        case 'S':
            size = strtol (optarg, NULL, 0);
            break;
        case 'L':
            lowmem_limit = atoi(optarg) * 1024 * 1024;
            break;
        case 'U':
            user_parm = optarg;
            break;
        default:
            fprintf (stderr, "Unrecognized argument\n");
        }
    }
    /* Obtain I/O privity */
    ret = ThreadCtl_r (_NTO_TCTL_IO, 0);
    if (ret)
    {
        fprintf(stderr, "Unable to obtain I/O privity");
        return EXIT_FAILURE;
    }

    /* Only let one tiler run. */
    if (-1 != stat("/dev/tiler", &sbuf)) {
        return EXIT_FAILURE;
    }

    ret = tiler_init(size);
    if (ret) {
        fprintf(stderr, "tiler_init failed with status [%d]", ret);
        return EXIT_FAILURE;
    }

    /* initialize dispatch interface */
    if((dpp = dispatch_create()) == NULL) {
        fprintf(stderr,
                "%s: Unable to allocate dispatch handle.\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* Initialize the thread pool */
    memset (&tattr, 0x00, sizeof (thread_pool_attr_t));
    tattr.handle = dpp;
    tattr.context_alloc = dispatch_context_alloc;
    tattr.context_free = dispatch_context_free;
    tattr.block_func = dispatch_block;
    tattr.unblock_func = dispatch_unblock;
    tattr.handler_func = dispatch_handler;
    tattr.lo_water = 2;
    tattr.hi_water = 8;
    tattr.increment = 1;
    tattr.maximum = 50;

    /* initialize resource manager attributes */
    memset(&resmgr_attr, 0, sizeof resmgr_attr);
    resmgr_attr.nparts_max = 10;
    resmgr_attr.msg_max_size = 16384;
    memset(&mattr, 0, sizeof(iofunc_mount_t));
    mattr.flags = 0;
    mattr.conf = IOFUNC_PC_CHOWN_RESTRICTED | IOFUNC_PC_NO_TRUNC | IOFUNC_PC_SYNC_IO;
    mattr.dev = 0;
    mattr.blocksize=0;
    mattr.funcs = &ocb_funcs;
    memset(&ocb_funcs, 0, sizeof(iofunc_funcs_t));
    ocb_funcs.nfuncs = _IOFUNC_NFUNCS;
    ocb_funcs.ocb_calloc = ocb_calloc;
    ocb_funcs.ocb_free = ocb_free;
    memset(&io_funcs, 0, sizeof(resmgr_io_funcs_t));
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.devctl = tiler_devctl;
    //io_funcs.mmap = tiler_mmap;
    io_funcs.read = tiler_read;

    iofunc_attr_init(&attr, S_IFNAM | 0777 , 0, 0);
    attr.mount = &mattr;

    if ( ( tpool = thread_pool_create(&tattr,0) ) == NULL )
        return EXIT_FAILURE;

    if (-1 != stat("/dev/tiler", &sbuf)) {
        return EXIT_FAILURE;
    }

    /* attach our device name */
    id = resmgr_attach(
            dpp,            /* dispatch handle        */
            &resmgr_attr,   /* resource manager attrs */
            "/dev/tiler",   /* device name            */
            _FTYPE_ANY,     /* open type              */
            0,              /* flags                  */
            &connect_funcs, /* connect routines       */
            &io_funcs,      /* I/O routines           */
            &attr);         /* handle                 */
    if(id == -1) {
        fprintf(stderr, "%s: Unable to attach name.\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Setup low memory purger */
    tiler_lowmem_init(dpp);

    /* background the process */
    procmgr_daemon(0, PROCMGR_DAEMON_NOCLOSE|PROCMGR_DAEMON_NODEVNULL);
    thread_pool_start( tpool );

    /* Mask out unecessary signals */
    sigfillset (&set);
    sigdelset (&set, SIGINT);
    sigdelset (&set, SIGTERM);
    pthread_sigmask (SIG_BLOCK, &set, NULL);

    /* Wait for one of these signals */
    sigemptyset (&set);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGQUIT);
    sigaddset (&set, SIGTERM);

#if (_NTO_VERSION >= 800)
    /* Relinquish privileges */
    ret = procmgr_ability(  0,
                            DENY_ALL | PROCMGR_AID_SPAWN,
                            DENY_ALL | PROCMGR_AID_FORK,
                            DENY_ALL | PROCMGR_AID_PROT_EXEC,
                            PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_MEM_PEER,
                            PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_MEM_PHYS,
                            PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_MEM_SPECIAL,
                            PROCMGR_AID_EOL);

    if(ret != EOK) {
        fprintf(stderr, "procmgr_ability failed! errno=%d\n", ret);
        return EXIT_FAILURE;
    }

    /* Drop root privileges */
    if (user_parm != NULL) {
        if (set_ids_from_arg(user_parm) < 0) {
            fprintf(stderr, "unable to set uid/gid - %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        // become nobody if nothing specified from command line
        if (setuid(99) != 0) {
                fprintf(stderr, "unable to set uid - %s\n", strerror(errno));
        }
    }
#endif

    /* Wait for a signal */
    while (1)
    {
        switch (SignalWaitinfo (&set, NULL))
        {
            case SIGTERM:
            case SIGQUIT:
            case SIGINT:
                goto done;
            default:
                break;
        }
    }

done:
    /* Received SIGTERM: clean up */
    resmgr_detach(dpp, id, _RESMGR_DETACH_ALL);

    tiler_exit();

    return 0;
}
