/**
 *  @file   _VAYUDspProc.h
 *
 *  @brief      Internal header for Processor interface for omap3530dsp.
 *
 *
 */
/*
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



#ifndef _VAYUDSPPROC_H_0xbbed
#define _VAYUDSPPROC_H_0xbbed


/* Module level headers */
#include <ProcDefs.h>
#include <ti/syslink/ProcMgr.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 *  See ProcDefs.h and VAYUDspProc.h
 * =============================================================================
 */


/* =============================================================================
 *  Internal functions accessed through function table
 * =============================================================================
 */
/* Function to initialize the slave processor */
Int VAYUDSPPROC_attach (Processor_Handle         handle,
                      Processor_AttachParams * params);

/* Function to finalize the slave processor */
Int VAYUDSPPROC_detach (Processor_Handle handle);

/* Function to start the slave processor */
Int VAYUDSPPROC_start (Processor_Handle        handle,
                     UInt32                  entryPt,
                     Processor_StartParams * params);

/* Function to start the stop processor */
Int VAYUDSPPROC_stop (Processor_Handle handle);

/* Function to read from the slave processor's memory. */
Int VAYUDSPPROC_read (Processor_Handle handle,
                    UInt32           procAddr,
                    UInt32 *         numBytes,
                    Ptr              buffer);

/* Function to write into the slave processor's memory. */
Int VAYUDSPPROC_write (Processor_Handle handle,
                     UInt32           procAddr,
                     UInt32 *         numBytes,
                     Ptr              buffer);

/* Function to perform device-dependent operations. */
Int VAYUDSPPROC_control (Processor_Handle handle, Int32 cmd, Ptr arg);

/* Function to translate a slave physical address to master physical address */
Int VAYUDSPPROC_translate (Processor_Handle handle,
                            UInt32 *         dstAddr,
                            UInt32           srcAddr);

/* Function to map slave address to host address space */
Int VAYUDSPPROC_map (Processor_Handle handle,
                      UInt32 *         dstAddr,
                      UInt32           nSegs,
                      Memory_SGList *  sglist);

/* Function to map slave address to host address space */
Int VAYUDSPPROC_unmap (Processor_Handle handle,
                        UInt32           slaveVirtAddr,
                        UInt32           size);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* _VAYUDSPPROC_H_0xbbec */
