/**
 *  @file   _MultiProcPlat.h
 *
 *  @brief      header file for_MultiProcPlat on HLOS side
 *
 *
 *  @ver        02.00.00.46_alpha1
 *
 *  ============================================================================
 *
 *  Copyright (c) 2011-2012, Texas Instruments Incorporated
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


#ifndef _MULTIPROCPLAT_H_0XB522
#define _MULTIPROCPLAT_H_0XB522


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Platform-specific definitions.
 * =============================================================================
 */
#if defined(SYSLINK_PLATFORM_OMAP4430)
MultiProc_Config MultiProc_PlatformConfig = {
    .numProcessors = 4, /* numProcessors */
    .nameList[0] = "HOST",
    .nameList[1] = "CORE0",
    .nameList[2] = "CORE1",
    .nameList[3] = "DSP",
    .id = 0,
};
#endif

#if defined(SYSLINK_PLATFORM_OMAP5430)
#ifdef SYSLINK_SYSBIOS_SMP
MultiProc_Config MultiProc_PlatformConfig = {
    .numProcessors = 3, /* numProcessors */
    .nameList[0] = "HOST",
    .nameList[1] = "IPU",
    .nameList[2] = "DSP",
    .id = 0,
};
#else
MultiProc_Config MultiProc_PlatformConfig = {
    .numProcessors = 4, /* numProcessors */
    .nameList[0] = "HOST",
    .nameList[1] = "CORE0",
    .nameList[2] = "CORE1",
    .nameList[3] = "DSP",
    .id = 0,
};
#endif
#endif

#if defined(SYSLINK_PLATFORM_TI81XX)
#if !defined (SYSLINK_VARIANT_TI813X)
MultiProc_Config MultiProc_PlatformConfig = {
    .numProcessors = 4, /* numProcessors */
    .nameList[0] = "HOST",
    .nameList[1] = "VPSS-M3",
    .nameList[2] = "VIDEO-M3",
    .nameList[3] = "DSP",
    .id = 0,
};
#else
MultiProc_Config MultiProc_PlatformConfig = {
    .numProcessors = 4, /* numProcessors */
    .nameList[0] = "HOST",
    .nameList[1] = "VPSS-M3",
    .nameList[2] = "DSP",
    .id = 0,
};
#endif
#endif

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* if !defined(_MULTIPROCPLAT_H_0XB522) */

