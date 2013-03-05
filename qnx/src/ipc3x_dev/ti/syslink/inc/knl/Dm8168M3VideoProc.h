/**
 *  @file   Dm8168M3VideoProc.h
 *
 *  @brief      Processor interface for DM8168SLAVE.
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



#ifndef DM8168VIDEOM3PROC_H_0xbbed
#define DM8168VIDEOM3PROC_H_0xbbed


/* Module headers */
#include <ti/syslink/ProcMgr.h>
#include <ProcDefs.h>


#if defined (__cplusplus)
extern "C" {
#endif


/*!
 *  @def    DM8168VIDEOM3PROC_MODULEID
 *  @brief  Module ID for DM8168SLAVE.
 */
#define DM8168VIDEOM3PROC_MODULEID           (UInt16) 0xbbec

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/*!
 *  @def    DM8168VIDEOM3PROC_STATUSCODEBASE
 *  @brief  Error code base for ProcMgr.
 */
#define DM8168VIDEOM3PROC_STATUSCODEBASE      (DM8168VIDEOM3PROC_MODULEID << 12u)

/*!
 *  @def    DM8168VIDEOM3PROC_MAKE_FAILURE
 *  @brief  Macro to make failure code.
 */
#define DM8168VIDEOM3PROC_MAKE_FAILURE(x)    ((Int)(  0x80000000                    \
                                         | (DM8168VIDEOM3PROC_STATUSCODEBASE + (x))))

/*!
 *  @def    DM8168VIDEOM3PROC_MAKE_SUCCESS
 *  @brief  Macro to make success code.
 */
#define DM8168VIDEOM3PROC_MAKE_SUCCESS(x) (DM8168VIDEOM3PROC_STATUSCODEBASE + (x))

/*!
 *  @def    DM8168VIDEOM3PROC_E_MMUENTRYEXISTS
 *  @brief  Specified MMU entry already exists.
 */
#define DM8168VIDEOM3PROC_E_MMUENTRYEXISTS   DM8168VIDEOM3PROC_MAKE_FAILURE(1)

/*!
 *  @def    DM8168VIDEOM3PROC_E_ISR
 *  @brief  Error occurred during ISR operation.
 */
#define DM8168VIDEOM3PROC_E_ISR              DM8168VIDEOM3PROC_MAKE_FAILURE(2)

/*!
 *  @def    DM8168VIDEOM3PROC_E_MMUCONFIG
 *  @brief  Error occurred during MMU configuration
 */
#define DM8168VIDEOM3PROC_E_MMUCONFIG        DM8168VIDEOM3PROC_MAKE_FAILURE(3)

/*!
 *  @def    DM8168VIDEOM3PROC_E_OSFAILURE
 *  @brief  Failure in an OS-specific operation.
 */
#define DM8168VIDEOM3PROC_E_OSFAILURE        DM8168VIDEOM3PROC_MAKE_FAILURE(4)

/*!
 *  @def    DM8168VIDEOM3PROC_E_INVALIDARG
 *  @brief  Argument passed to a function is invalid.
 */
#define DM8168VIDEOM3PROC_E_INVALIDARG       DM8168VIDEOM3PROC_MAKE_FAILURE(5)

/*!
 *  @def    DM8168VIDEOM3PROC_E_MEMORY
 *  @brief  Memory allocation failed.
 */
#define DM8168VIDEOM3PROC_E_MEMORY           DM8168VIDEOM3PROC_MAKE_FAILURE(6)

/*!
 *  @def    DM8168VIDEOM3PROC_E_HANDLE
 *  @brief  Invalid object handle specified
 */
#define DM8168VIDEOM3PROC_E_HANDLE           DM8168VIDEOM3PROC_MAKE_FAILURE(7)

/*!
 *  @def    DM8168VIDEOM3PROC_E_ACCESSDENIED
 *  @brief  The operation is not permitted in this process.
 */
#define DM8168VIDEOM3PROC_E_ACCESSDENIED     DM8168VIDEOM3PROC_MAKE_FAILURE(8)

/*!
 *  @def    DM8168VIDEOM3PROC_E_FAIL
 *  @brief  Generic failure.
 */
#define DM8168VIDEOM3PROC_E_FAIL             DM8168VIDEOM3PROC_MAKE_FAILURE(9)

/*!
 *  @def    DM8168VIDEOM3PROC_SUCCESS
 *  @brief  Operation successful.
 */
#define DM8168VIDEOM3PROC_SUCCESS           DM8168VIDEOM3PROC_MAKE_SUCCESS(0)

/*!
 *  @def    DM8168VIDEOM3PROC_S_ALREADYSETUP
 *  @brief  The DM8168VIDEOM3PROC module has already been setup in this process.
 */
#define DM8168VIDEOM3PROC_S_ALREADYSETUP     DM8168VIDEOM3PROC_MAKE_SUCCESS(1)

/*!
 *  @def    DM8168VIDEOM3PROC_S_OPENHANDLE
 *  @brief  Other DM8168VIDEOM3PROC clients have still setup the DM8168VIDEOM3PROC module.
 */
#define DM8168VIDEOM3PROC_S_SETUP            DM8168VIDEOM3PROC_MAKE_SUCCESS(2)

/*!
 *  @def    DM8168VIDEOM3PROC_S_OPENHANDLE
 *  @brief  Other DM8168VIDEOM3PROC handles are still open in this process.
 */
#define DM8168VIDEOM3PROC_S_OPENHANDLE       DM8168VIDEOM3PROC_MAKE_SUCCESS(3)

/*!
 *  @def    DM8168VIDEOM3PROC_S_ALREADYEXISTS
 *  @brief  The DM8168VIDEOM3PROC instance has already been created/opened in this
 *          process
 */
#define DM8168VIDEOM3PROC_S_ALREADYEXISTS    DM8168VIDEOM3PROC_MAKE_SUCCESS(4)


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  Module configuration structure.
 */
typedef struct DM8168VIDEOM3PROC_Config {
    UInt32 reserved; /*!< Reserved field (not currently required) */
} DM8168VIDEOM3PROC_Config;

/*!
 *  @brief  Configuration parameters specific to this processor.
 */
typedef struct DM8168VIDEOM3PROC_Params_tag {
    Bool                mmuEnable;
    /*!< Determines if mmu should be used (enabled) */
    UInt32              numMemEntries;
    /*!< Number of memory regions to be configured. */
    ProcMgr_AddrInfo    memEntries[ProcMgr_MAX_MEMORY_REGIONS];
    /*!< Array of information structures for memory regions to be configured. */
} DM8168VIDEOM3PROC_Params;

/*!
 *  @brief  Defines DM8168VIDEOM3PROC object handle
 */
typedef struct DM8168VIDEOM3PROC_Object * DM8168VIDEOM3PROC_Handle;


/* =============================================================================
 *  APIs
 * =============================================================================
 */

/* Function to get the default configuration for the DM8168VIDEOM3PROC module. */
Void DM8168VIDEOM3PROC_getConfig (DM8168VIDEOM3PROC_Config * cfg);

/* Function to setup the DM8168VIDEOM3PROC module. */
Int DM8168VIDEOM3PROC_setup (DM8168VIDEOM3PROC_Config * cfg);

/* Function to destroy the DM8168VIDEOM3PROC module. */
Int DM8168VIDEOM3PROC_destroy (Void);

/* Function to initialize the parameters for this processor instance. */
Void DM8168VIDEOM3PROC_Params_init (DM8168VIDEOM3PROC_Handle    handle,
                            DM8168VIDEOM3PROC_Params *  params);

/* Function to create an instance of this processor. */
DM8168VIDEOM3PROC_Handle DM8168VIDEOM3PROC_create (      UInt16                   procId,
                                         const DM8168VIDEOM3PROC_Params * params);

/* Function to delete an instance of this processor. */
Int DM8168VIDEOM3PROC_delete (DM8168VIDEOM3PROC_Handle * handlePtr);

/* Function to open an instance of this processor. */
Int DM8168VIDEOM3PROC_open (DM8168VIDEOM3PROC_Handle * handlePtr, UInt16 procId);

/* Function to close an instance of this processor. */
Int DM8168VIDEOM3PROC_close (DM8168VIDEOM3PROC_Handle * handlePtr);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* DM8168VIDEOM3PROC_H_0xbbec */
