/**
 *  @file   PerfUtils.h
 *
 *  @brief      Performance functions
 *
 *              This will have the definitions for the performance measurement
 *              functions.
 *
 *  ============================================================================
 *
 *  Copyright (c) 2009-2012, Texas Instruments Incorporated
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


#ifndef PERFUTILS_H
#define PERFUTILS_H

/* Linux specific header files */
#include <sys/time.h>

/* Standard headers */
#include <stdio.h>


#if defined (__cplusplus)
extern "C" {
#endif


#define SYSM3_PROC 2
#define APPM3_PROC 3


/* USING MEM_IPC_HEAP2 make sure that,not using the
 * this address
 */

#define PERF_UTILS_GPTIMER_TEN         (10)
#define TOTAL_PERF_BUFFER_SIZE         (SYSM3_PERF_BUFFER_SIZE \
                                            + APPM3_PERF_BUFFER_SIZE)

// Must match struct definition on Ducati side
#define MAX_ENTRIES_PER_FUNCTION    200
#define MAX_ENTRIES_PER_KERNEL_FUNC 64
#define MAX_NUM_RECORDS             64
#define GPTIMER10 10


/* =============================================================================
 *  APIs
 * =============================================================================
 */
int PerfUtils_init(void);
int PerfUtils_destroy (void);
void PerfUtils_addFxn(unsigned int fxnIndex, char * fxnName, unsigned int clientproc, unsigned int serverproc, unsigned int msgsize);
int PerfUtils_enter (unsigned int fxnIndex);
int PerfUtils_exit (unsigned int fxnIndex);
void PerfUtils_printReport (int printLevel);
void PerfUtils_startGpTimer (int gpTimerNo);
void PerfUtils_stopGpTimer (int gpTimerNo);
#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ifndef PERFUTILS_H */
