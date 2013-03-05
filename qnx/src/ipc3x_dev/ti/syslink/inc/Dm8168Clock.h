/**
 *  @file   Dm8168Clock.h
 *
 *  @brief      RTOS side Clock interface for DM8168
 *
 *
 *
 */
/*
 *  ============================================================================
 *
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
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


#ifndef DM8168CLOCK_H_0xbbec
#define DM8168CLOCK_H_0xbbec

#include <ClockOps.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* Function creates the clock object */
ClockOps_Handle DM8168CLOCK_create(Void);
/* Function deletes the clock object */
Int DM8168CLOCK_delete (ClockOps_Handle handle);
/* Function to get handle to a clock given its name */
Ptr DM8168CLOCK_get(String clkname);
/* Function to release a Clock. */
Void DM8168CLOCK_put(Ptr clkHandle);
/* Function to enable a clock. */
Int32 DM8168CLOCK_enable(Ptr clkHandle);
/* Function to disable a clock */
Void DM8168CLOCK_disable(Ptr clkHandle);
/* Function to get clock speed */
ULong DM8168CLOCK_getRate(Ptr clkHandle);
/* Function to set clock speed */
Int32 DM8168CLOCK_setRate(Ptr clkHandle, ULong rate);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* omap3530_hal_mmu_H_0xbbec */
