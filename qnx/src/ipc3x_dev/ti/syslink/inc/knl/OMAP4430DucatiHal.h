/**
 *  @file   OMAP4430DucatiHal.h
 *
 *  @brief      Top-level header file for Ducati Hardware Abstraction Layer.
 *
 *              This module is responsible for handling device-specific
 *              operations.
 *              The implementation is specific to Ducati.
 *
 *
 *  @ver        02.00.00.44_pre-alpha3
 *
 *  ============================================================================
 *
 *  Copyright (c) 2008-2009, Texas Instruments Incorporated
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

#ifndef OMAP4430DUCATIHAL_H_0xbbed
#define OMAP4430DUCATIHAL_H_0xbbed


/* Module level headers */
#include <_ProcDefs.h>
#include "OMAP4430DucatiHalMmu.h"


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 *  See _ProcDefs.h
 * =============================================================================
 */
/*!
 *  @brief  Register access method.
 */
//#define REG(x)              *((volatile UInt32 *) (x))

typedef enum OMAP4430DUCATIPROCTYPE
{
    PROCTYPE_APPM3 = 2,
    PROCTYPE_SYSM3 = 1
}OMAP4430DUCATIPROCTYPE;

/*!
 *  @brief  Hardware Abstraction object.
 */
typedef struct OMAP4430DUCATI_HalObject_tag {
    UInt32                      CodeBase;
    /*!< Virtual base address of the General Control module. */
    UInt32                      M3Base;
    /*!< Base address of the M3. */
    UInt32                      ConstBase;
    /*!< Base address of the Const. */
    UInt32                      mmuBase;
    /*!< Base address of the MMU module. */
    UInt32                      procId;
    /*!< Processor ID. */
    OMAP4430DUCATI_HalMmuObject mmuObj;
    /*!< MMU abstraction. */
    Processor_Handle            procHandle;
    /*!< Processor handle. */
} OMAP4430DUCATI_HalObject;


/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* Function to initialize the HAL object */
Int OMAP4430DUCATI_halInit (Ptr * halObj, Ptr params,UInt16 procId);

/* Function to finalize the HAL object */
Int OMAP4430DUCATI_halExit (Ptr halObj, Ptr params);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* OMAP4430DUCATIHAL_H_0xbbed */
