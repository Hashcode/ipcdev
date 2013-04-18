/**
 *  @file   VAYUIpuCore1Proc.h
 *
 *  @brief      Processor interface for VAYUSLAVE.
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



#ifndef VAYUIPUCORE1PROC_H_0xbbed
#define VAYUIPUCORE1PROC_H_0xbbed


/* Module headers */
#include <ti/syslink/ProcMgr.h>
#include <ProcDefs.h>


#if defined (__cplusplus)
extern "C" {
#endif


/*!
 *  @def    VAYUIPUCORE1PROC_MODULEID
 *  @brief  Module ID for VAYUSLAVE.
 */
#define VAYUIPUCORE1PROC_MODULEID           (UInt16) 0xbbec

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/*!
 *  @def    VAYUIPUCORE1PROC_STATUSCODEBASE
 *  @brief  Error code base for ProcMgr.
 */
#define VAYUIPUCORE1PROC_STATUSCODEBASE      (VAYUIPUCORE1PROC_MODULEID << 12u)

/*!
 *  @def    VAYUIPUCORE1PROC_MAKE_FAILURE
 *  @brief  Macro to make failure code.
 */
#define VAYUIPUCORE1PROC_MAKE_FAILURE(x) ((Int)(  0x80000000                   \
                                     | (VAYUIPUCORE1PROC_STATUSCODEBASE + (x))))

/*!
 *  @def    VAYUIPUCORE1PROC_MAKE_SUCCESS
 *  @brief  Macro to make success code.
 */
#define VAYUIPUCORE1PROC_MAKE_SUCCESS(x) (VAYUIPUCORE1PROC_STATUSCODEBASE + (x))

/*!
 *  @def    VAYUIPUCORE1PROC_E_MMUENTRYEXISTS
 *  @brief  Specified MMU entry already exists.
 */
#define VAYUIPUCORE1PROC_E_MMUENTRYEXISTS   VAYUIPUCORE1PROC_MAKE_FAILURE(1)

/*!
 *  @def    VAYUIPUCORE1PROC_E_ISR
 *  @brief  Error occurred during ISR operation.
 */
#define VAYUIPUCORE1PROC_E_ISR              VAYUIPUCORE1PROC_MAKE_FAILURE(2)

/*!
 *  @def    VAYUIPUCORE1PROC_E_MMUCONFIG
 *  @brief  Error occurred during MMU configuration
 */
#define VAYUIPUCORE1PROC_E_MMUCONFIG        VAYUIPUCORE1PROC_MAKE_FAILURE(3)

/*!
 *  @def    VAYUIPUCORE1PROC_E_OSFAILURE
 *  @brief  Failure in an OS-specific operation.
 */
#define VAYUIPUCORE1PROC_E_OSFAILURE        VAYUIPUCORE1PROC_MAKE_FAILURE(4)

/*!
 *  @def    VAYUIPUCORE1PROC_E_INVALIDARG
 *  @brief  Argument passed to a function is invalid.
 */
#define VAYUIPUCORE1PROC_E_INVALIDARG       VAYUIPUCORE1PROC_MAKE_FAILURE(5)

/*!
 *  @def    VAYUIPUCORE1PROC_E_MEMORY
 *  @brief  Memory allocation failed.
 */
#define VAYUIPUCORE1PROC_E_MEMORY           VAYUIPUCORE1PROC_MAKE_FAILURE(6)

/*!
 *  @def    VAYUIPUCORE1PROC_E_HANDLE
 *  @brief  Invalid object handle specified
 */
#define VAYUIPUCORE1PROC_E_HANDLE           VAYUIPUCORE1PROC_MAKE_FAILURE(7)

/*!
 *  @def    VAYUIPUCORE1PROC_E_ACCESSDENIED
 *  @brief  The operation is not permitted in this process.
 */
#define VAYUIPUCORE1PROC_E_ACCESSDENIED     VAYUIPUCORE1PROC_MAKE_FAILURE(8)

/*!
 *  @def    VAYUIPUCORE1PROC_E_FAIL
 *  @brief  Generic failure.
 */
#define VAYUIPUCORE1PROC_E_FAIL             VAYUIPUCORE1PROC_MAKE_FAILURE(9)

/*!
 *  @def    VAYUIPUCORE1PROC_SUCCESS
 *  @brief  Operation successful.
 */
#define VAYUIPUCORE1PROC_SUCCESS           VAYUIPUCORE1PROC_MAKE_SUCCESS(0)

/*!
 *  @def    VAYUIPUCORE1PROC_S_ALREADYSETUP
 *  @brief  The VAYUIPUCORE1PROC module has already been setup in this process.
 */
#define VAYUIPUCORE1PROC_S_ALREADYSETUP     VAYUIPUCORE1PROC_MAKE_SUCCESS(1)

/*!
 *  @def    VAYUIPUCORE1PROC_S_OPENHANDLE
 *  @brief  Other VAYUIPUCORE1PROC clients have still setup the
 *          VAYUIPUCORE1PROC module.
 */
#define VAYUIPUCORE1PROC_S_SETUP            VAYUIPUCORE1PROC_MAKE_SUCCESS(2)

/*!
 *  @def    VAYUIPUCORE1PROC_S_OPENHANDLE
 *  @brief  Other VAYUIPUCORE1PROC handles are still open in this process.
 */
#define VAYUIPUCORE1PROC_S_OPENHANDLE       VAYUIPUCORE1PROC_MAKE_SUCCESS(3)

/*!
 *  @def    VAYUIPUCORE1PROC_S_ALREADYEXISTS
 *  @brief  The VAYUIPUCORE1PROC instance has already been created/opened in
 *          this process
 */
#define VAYUIPUCORE1PROC_S_ALREADYEXISTS    VAYUIPUCORE1PROC_MAKE_SUCCESS(4)


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  Module configuration structure.
 */
typedef struct VAYUIPUCORE1PROC_Config {
    UInt32 reserved; /*!< Reserved field (not currently required) */
} VAYUIPUCORE1PROC_Config;

/*!
 *  @brief  Configuration parameters specific to this processor.
 */
typedef struct VAYUIPUCORE1PROC_Params_tag {
    Bool                mmuEnable;
    /*!< Determines if mmu should be used (enabled) */
    UInt32              numMemEntries;
    /*!< Number of memory regions to be configured. */
    ProcMgr_AddrInfo    memEntries[ProcMgr_MAX_MEMORY_REGIONS];
    /*!< Array of information structures for memory regions to be configured. */
    UInt32              carveoutAddr[ProcMgr_MAX_MEMORY_REGIONS];
    /*!< The address of the carveout for shared mem */
    UInt32              carveoutSize[ProcMgr_MAX_MEMORY_REGIONS];
    /*!< The length of the carveout for shared mem */
} VAYUIPUCORE1PROC_Params;

/*!
 *  @brief  VAYUIPUCORE1PROC instance object.
 */
struct VAYUIPUCORE1PROC_Object_tag {
    VAYUIPUCORE1PROC_Params  params;
    /*!< Instance parameters (configuration values) */
    Ptr                      halObject;
    /*!< Pointer to the Hardware Abstraction Layer object. */
    ProcMgr_Handle           pmHandle;
    /*!< Handle to proc manager */
    Processor_Handle         procHandle;
    /*!< Handle to processor */
};

/*!
 *  @brief  Defines the VAYUIPUCORE1PROC object type
 */
typedef struct VAYUIPUCORE1PROC_Object_tag VAYUIPUCORE1PROC_Object;

/*!
 *  @brief  Defines VAYUIPUCORE1PROC object handle
 */
typedef struct VAYUIPUCORE1PROC_Object * VAYUIPUCORE1PROC_Handle;


/* =============================================================================
 *  APIs
 * =============================================================================
 */

/* Function to get the default configuration for the VAYUIPUCORE1PROC module. */
Void VAYUIPUCORE1PROC_getConfig (VAYUIPUCORE1PROC_Config * cfg);

/* Function to setup the VAYUIPUCORE1PROC module. */
Int VAYUIPUCORE1PROC_setup (VAYUIPUCORE1PROC_Config * cfg);

/* Function to destroy the VAYUIPUCORE1PROC module. */
Int VAYUIPUCORE1PROC_destroy (Void);

/* Function to initialize the parameters for this processor instance. */
Void VAYUIPUCORE1PROC_Params_init (VAYUIPUCORE1PROC_Handle    handle,
                            VAYUIPUCORE1PROC_Params *  params);

/* Function to create an instance of this processor. */
VAYUIPUCORE1PROC_Handle VAYUIPUCORE1PROC_create (      UInt16           procId,
                                        const VAYUIPUCORE1PROC_Params * params);

/* Function to delete an instance of this processor. */
Int VAYUIPUCORE1PROC_delete (VAYUIPUCORE1PROC_Handle * handlePtr);

/* Function to open an instance of this processor. */
Int VAYUIPUCORE1PROC_open (VAYUIPUCORE1PROC_Handle * handlePtr, UInt16 procId);

/* Function to close an instance of this processor. */
Int VAYUIPUCORE1PROC_close (VAYUIPUCORE1PROC_Handle * handlePtr);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* VAYUIPUCORE1PROC_H_0xbbec */
