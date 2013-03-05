/**
 *  @file   Dm8168DucatiPwr.h
 *
 *  @brief      Power Manager interface for DM8168DUCATIPWR.
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



#ifndef DM8168DUCATIPWR_H_0xa862
#define DM8168DUCATIPWR_H_0xa862


#if defined (__cplusplus)
extern "C" {
#endif

#include <ti/syslink/inc/ClockOps.h>

/*!
 *  @def    DM8168DUCATIPWR_MODULEID
 *  @brief  Module ID for DM8168DUCATIPWR.
 */
#define DM8168DUCATIPWR_MODULEID           (UInt16) 0xa860

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/*!
 *  @def    DM8168DUCATIPWR_STATUSCODEBASE
 *  @brief  Error code base for DM8168DUCATIPWR.
 */
#define DM8168DUCATIPWR_STATUSCODEBASE      (DM8168DUCATIPWR_MODULEID << 12u)

/*!
 *  @def    DM8168DUCATIPWR_MAKE_FAILURE
 *  @brief  Macro to make failure code.
 */
#define DM8168DUCATIPWR_MAKE_FAILURE(x)((Int)(  0x80000000                    \
                                      | (DM8168DUCATIPWR_STATUSCODEBASE + (x))))

/*!
 *  @def    DM8168DUCATIPWR_MAKE_SUCCESS
 *  @brief  Macro to make success code.
 */
#define DM8168DUCATIPWR_MAKE_SUCCESS(x)   (DM8168DUCATIPWR_STATUSCODEBASE + (x))

/*!
 *  @def    DM8168DUCATIPWR_E_OSFAILURE
 *  @brief  Failure in an OS-specific operation.
 */
#define DM8168DUCATIPWR_E_OSFAILURE        DM8168DUCATIPWR_MAKE_FAILURE(1)

/*!
 *  @def    DM8168DUCATIPWR_E_INVALIDARG
 *  @brief  Argument passed to a function is invalid.
 */
#define DM8168DUCATIPWR_E_INVALIDARG       DM8168DUCATIPWR_MAKE_FAILURE(2)

/*!
 *  @def    DM8168DUCATIPWR_E_MEMORY
 *  @brief  Memory allocation failed.
 */
#define DM8168DUCATIPWR_E_MEMORY           DM8168DUCATIPWR_MAKE_FAILURE(3)

/*!
 *  @def    DM8168DUCATIPWR_E_HANDLE
 *  @brief  Invalid object handle specified
 */
#define DM8168DUCATIPWR_E_HANDLE           DM8168DUCATIPWR_MAKE_FAILURE(4)

/*!
 *  @def    DM8168DUCATIPWR_E_ACCESSDENIED
 *  @brief  The operation is not permitted in this process.
 */
#define DM8168DUCATIPWR_E_ACCESSDENIED     DM8168DUCATIPWR_MAKE_FAILURE(5)

/*!
 *  @def    DM8168DUCATIPWR_E_FAIL
 *  @brief  Generic failure.
 */
#define DM8168DUCATIPWR_E_FAIL             DM8168DUCATIPWR_MAKE_FAILURE(6)
/*!
 *  @def    DM8168DUCATIPWR_E_FAIL
 *  @brief  Generic failure.
 */
#define DM8168DUCATIPWR_E_INVALIDSTATE     DM8168DUCATIPWR_MAKE_FAILURE(7)

/*!
 *  @def    DM8168DUCATIPWR_SUCCESS
 *  @brief  Operation successful.
 */
#define DM8168DUCATIPWR_SUCCESS            DM8168DUCATIPWR_MAKE_SUCCESS(0)

/*!
 *  @def    DM8168DUCATIPWR_S_ALREADYSETUP
 *  @brief  The DM8168DUCATIPWR module has already been setup in this process.
 */
#define DM8168DUCATIPWR_S_ALREADYSETUP     DM8168DUCATIPWR_MAKE_SUCCESS(1)

/*!
 *  @def    DM8168DUCATIPWR_S_OPENHANDLE
 *  @brief  Other DM8168DUCATIPWR clients have still setup the DM8168DUCATIPWR module.
 */
#define DM8168DUCATIPWR_S_SETUP            DM8168DUCATIPWR_MAKE_SUCCESS(2)

/*!
 *  @def    DM8168DUCATIPWR_S_OPENHANDLE
 *  @brief  Other DM8168DUCATIPWR handles are still open in this process.
 */
#define DM8168DUCATIPWR_S_OPENHANDLE       DM8168DUCATIPWR_MAKE_SUCCESS(3)

/*!
 *  @def    DM8168DUCATIPWR_S_ALREADYEXISTS
 *  @brief  The DM8168DUCATIPWR instance has already been created/opened in this
 *          process
 */
#define DM8168DUCATIPWR_S_ALREADYEXISTS    DM8168DUCATIPWR_MAKE_SUCCESS(4)
/*!
 *  @def    DM8168DUCATIPWR_S_ALREADYPOWEREDON
 *  @brief  The DM8168DUCATIPWR power on code has been executed
 */
#define  DM8168DUCATIPWR_S_ALREADYPOWEREDON DM8168DUCATIPWR_MAKE_SUCCESS(5)


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  Module configuration structure.
 */
typedef struct DM8168DUCATIPWR_Config {
    UInt32 reserved; /*!< Reserved field (not currently required) */
} DM8168DUCATIPWR_Config;

/*!
 *  @brief  Configuration parameters specific to this processor.
 */
typedef struct DM8168DUCATIPWR_Params_tag {
    UInt32             reserved;
    /*!< Reserved field (not currently required) */
    Ptr                clockHandle;
    /*!< Pointer to the Clock Object that will be passed to create */
} DM8168DUCATIPWR_Params;



/*!
 *  @brief  DM8168DUCATIPWR instance object.
 */
struct DM8168DUCATIPWR_Object_tag {

    DM8168DUCATIPWR_Params params;
    /*!< Instance parameters (configuration values) */
    UInt32             prcmVA;
    /*!< Virtual address for prcm module */
    UInt32             ducatiMmuVA;
    /*!< Virtual address for prcm module */
    UInt32             ducatibaseVA;
    /*!< Virtual address for prcm module */
    ClockOps_Handle     clockHandle;
    /*!< Pointer to the Clock object. */

};


/*!
 *  @brief  Defines DM8168DUCATIPWR object handle
 */
typedef struct DM8168DUCATIPWR_Object_tag DM8168DUCATIPWR_Object;


/*!
 *  @brief  Defines DM8168DUCATIPWR object handle
 */
typedef struct DM8168DUCATIPWR_Object * DM8168DUCATIPWR_Handle;


/* =============================================================================
 *  APIs
 * =============================================================================
 */

/* Function to get the default configuration for the DM8168DUCATIPWR module. */
Void DM8168DUCATIPWR_getConfig (DM8168DUCATIPWR_Config * cfg);

/* Function to setup the DM8168DUCATIPWR module. */
Int DM8168DUCATIPWR_setup (DM8168DUCATIPWR_Config * cfg);

/* Function to destroy the DM8168DUCATIPWR module. */
Int DM8168DUCATIPWR_destroy (Void);

/* Function to initialize the parameters for this PwrMgr instance. */
Void DM8168DUCATIPWR_Params_init (DM8168DUCATIPWR_Params * params);

/* Function to create an instance of this PwrMgr. */
DM8168DUCATIPWR_Handle DM8168DUCATIPWR_create (       UInt16               procId,
                                       const  DM8168DUCATIPWR_Params * params);

/* Function to delete an instance of this PwrMgr. */
Int DM8168DUCATIPWR_delete (DM8168DUCATIPWR_Handle * handlePtr);

/* Function to open an instance of this PwrMgr. */
Int DM8168DUCATIPWR_open (DM8168DUCATIPWR_Handle * handlePtr, UInt16 procId);

/* Function to close an instance of this PwrMgr. */
Int DM8168DUCATIPWR_close (DM8168DUCATIPWR_Handle * handlePtr);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* DM8168DUCATIPWR_H_0xa860 */
