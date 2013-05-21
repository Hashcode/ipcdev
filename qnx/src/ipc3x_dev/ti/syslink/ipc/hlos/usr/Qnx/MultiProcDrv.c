/*
 *  @file   MultiProcDrv.c
 *
 *  @brief      OS-specific implementation of MultiProc driver for Qnx
 *
 *
 *  ============================================================================
 *
 *  Copyright (c) 2013, Texas Instruments Incorporated
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
 */


/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/Trace.h>
#include <ti/ipc/MultiProc.h>
#include <ti/syslink/inc/MultiProcDrvDefs.h>
#include <ti/syslink/build/Qnx/resmgr/dcmd_syslink.h>

/* QNX specific header files */
#include <sys/types.h>
#include <unistd.h>

/** ============================================================================
 *  Globals
 *  ============================================================================
 */
/*!
 *  @brief  Driver handle for MultiProc in this process.
 */
extern Int32 IpcDrv_handle;

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
/*!
 *  @brief  Function to invoke the APIs through ioctl.
 *
 *  @param  cmd     Command for driver ioctl
 *  @param  args    Arguments for the ioctl command
 *
 *  @sa
 */
Int
MultiProcDrv_ioctl (UInt32 cmd, Ptr args)
{
    Int status      = MultiProc_S_SUCCESS;
    int osStatus    = -1;

    GT_2trace (curTrace, GT_ENTER, "MultiProcDrv_ioctl", cmd, args);

    switch (cmd) {
          case CMD_MULTIPROC_GETCONFIG:
          {
              MultiProcDrv_CmdArgs *cargs = (MultiProcDrv_CmdArgs *)args;
              iov_t mpgetconfig_iov[2];

              SETIOV(&mpgetconfig_iov[0], cargs, sizeof(MultiProcDrv_CmdArgs));
              SETIOV(&mpgetconfig_iov[1], cargs->args.getConfig.config,
                  sizeof(MultiProc_Config));
              osStatus = devctlv(IpcDrv_handle, DCMD_MULTIPROC_GETCONFIG, 1, 2,
                  mpgetconfig_iov, mpgetconfig_iov, NULL);
          }
          break;

          default:
          {
              /* This does not impact return status of this function, so retVal
               * comment is not used.
               */
              status = MultiProc_E_INVALIDARG;
              GT_setFailureReason (curTrace,
                                   GT_4CLASS,
                                   "MultiProcDrv_ioctl",
                                   status,
                                   "Unsupported ioctl command specified");
          }
          break;
      }


    if (osStatus != 0) {
        /*! @retval MultiProc_E_OSFAILURE Driver ioctl failed */
        status = MultiProc_E_OSFAILURE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "MultiProcDrv_ioctl",
                             status,
                             "Driver ioctl failed!");
    }
    else {
        /* First field in the structure is the API status. */
        status = ((MultiProcDrv_CmdArgs *) args)->apiStatus;
    }

    GT_1trace (curTrace, GT_LEAVE, "MultiProcDrv_ioctl", status);

    return status;
}
