/*
 *  @file   rprcoader.c
 *
 *  @brief      File-based rprc loader implementation
 *
 *              This rprc loader opens, parses and loads the rprc file that is
 *              present in the master file system, onto the slave processor.
 *
 *              @Example
 *              @code
 *              rprcloader_Handle     loaderHandle = NULL;
 *              rprcloader_Config     loaderConfig;
 *              rprcloader_Params     loaderParams;
 *
 *              rprcloader_getConfig (&loaderConfig);
 *              status = rprcloader_setup (&loaderConfig);
 *
 *              rprcloader_Params_init (NULL, &loaderParams);
 *              loaderHandle = rprcloader_create (procId, &loaderParams);
 *
 *              rprcloader_delete (&loaderHandle);
 *              rprcloader_destroy ();
 *              @endcode
 *
 *
 *  @ver        02.00.00.46_alpha1
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


/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <OsalKfile.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/Gate.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Trace.h>
#include <OsalDriver.h>
#include <Bitops.h>
#include <ti/syslink/utils/String.h>

/* Module level headers */
#include <LoaderDefs.h>
#include <Loader.h>
#include <rprcloader.h>
#include <_rprcloader.h>
#include <ti/syslink/ProcMgr.h>
#include <_ProcMgr.h>
#include <Processor.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>

#include "rprcfmt.h"

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/* Macro to make a correct module magic number with refCount */
#define LOADER_MAKE_MAGICSTAMP(x) ((LOADER_MODULEID << 12u) | (x))

#define START_SUFFIX       "_Start"
#define END_SUFFIX         "_End"
#define START_SUFFIX_LEN   6
#define END_SUFFIX_LEN     4

/*!
 *  @brief  rprcloader Module state object
 */
typedef struct rprcloader_ModuleObject_tag {
    Atomic                  refCount;
    /*!< Reference count */
    UInt32                  configSize;
    /*!< Size of configuration structure */
    rprcloader_Config        cfg;
    /*!< rprcloader configuration structure */
    rprcloader_Config        defCfg;
    /*!< Default module configuration */
    Bool                    isSetup;
    /*!< Indicates whether the rprcloader module is setup. */
    rprcloader_Handle        loaderHandles [MultiProc_MAXPROCESSORS];
    /*!< Loader handle array. */
    IGateProvider_Handle    gateHandle;
    /*!< Handle of gate to be used for local thread safety */
    Ptr                     osalDrvHandle;
    /*!< OsalDriver handle for rprcloader */
} rprcloader_ModuleObject;

/*!
 *  @brief  Internal rprcloader instance object.
 */
typedef struct _rprcloader_Object_tag {
    rprcloader_Params        params;
    /*!< Instance parameters (configuration values) */
    String                  fileName;
    /*!< Name of the file currently loaded. */
    Processor_Handle        procHandle;
    /*!< Handle to the Processor instance. */
    ProcMgr_Handle          pmHandle;
    /*!< Handle to the ProcMgr instance. */
    Processor_ProcArch      procArch;
    /*!< Processor architecture */
    Ptr                     fileDesc;
    /*!< File object for the slave base image file. */
} _rprcloader_Object;


/* =============================================================================
 *  Globals
 * =============================================================================
 */
/*!
 *  @var    rprcloader_state
 *
 *  @brief  rprcloader state object variable
 */
#if !defined(SYSLINK_BUILD_DEBUG)
static
#endif /* if !defined(SYSLINK_BUILD_DEBUG) */
rprcloader_ModuleObject rprcloader_state =
{
    .isSetup = FALSE,
    .configSize = sizeof (rprcloader_Config),
    .gateHandle = NULL,
    .osalDrvHandle = NULL
};

/*
 * TBD: Set rprcloader_Object for DLIF functions, since CGT loader doesn't
 * handle instances.
 * This requires that the loading can only happen sequentially, one at a time.
 */
rprcloader_Object * rprcloader_Object_g = NULL;

/* =============================================================================
 * Internal implementations of client Provided Functions
 * =============================================================================
 */

/* -----------------------------------------------------------------------------
 *  File I/O Interface
 *
 *  The client side of the loader must provide basic file I/O capabilities so
 *  that the core loader has random access into any file that it is asked to
 *  load.
 * -----------------------------------------------------------------------------
 */


/* -----------------------------------------------------------------------------
 *  Target Memory Access / Write Services
 *
 *  To complete the loading of an object segment, the segment may need to be
 *  relocated before it is actually written to target memory in the space that
 *  was allocated for it.
 *
 *  The client side of the loader provides the functions in this section to help
 *  complete the process of loading an object segment.
 *
 *  These functions help to make the core loader truly independent of
 *  whether it is running on the host or target architecture and how the
 *  client provides for reading/writing from/to target memory.
 * -----------------------------------------------------------------------------
 */
/*!
 *  @brief  Function to copy data into the provided segment and making it
 *          available to generic RPRC parser.
 *          Memory for buffer is allocated in this function.
 *
 *  @param  clientHandle Handle to the client loader object.
 *  @param  memReq       Memory request for this copy
 *
 *  @sa     rprcloaderTrgWrite_write
 */
Int
rprcloader_copy (UInt32 targetAddr, UInt32 dataAddr, UInt32 numBytes)
{
    Int                  status     = LOADER_SUCCESS;
    Char *               sectData;
    _rprcloader_Object * rprcloaderObject;
    ProcMgr_AddrInfo     aInfo;
    UInt32               dstAddr;

    GT_3trace (curTrace, GT_ENTER, "rprcloaderTrgWrite_copy",
            targetAddr, dataAddr, numBytes);

    /* TBD: temporary, until DLOAD loader passes back this object */
    GT_assert (curTrace, (rprcloader_Object_g != NULL));

    rprcloaderObject = (_rprcloader_Object *) rprcloader_Object_g->rprcloaderObject;
    GT_assert (curTrace, (rprcloaderObject != NULL));

    /* numBytes may be 0 if a section is of size 0. */
    if (numBytes != 0) {
        /* Translates slave phys to master physical address: */
        status = Processor_translateAddr (rprcloaderObject->procHandle,
                                          &dstAddr,
                                          (UInt32)targetAddr);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "rprcloaderTrgWrite_copy",
                                 status,
                                 "Processor_translateAddr failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            aInfo.addr [ProcMgr_AddrType_MasterPhys] = dstAddr;
            aInfo.addr [ProcMgr_AddrType_SlaveVirt]  = targetAddr;
            aInfo.size = numBytes;
            aInfo.isCached = FALSE;
            /*
             * Map master physical address to master virtual: this does NOT
             * program the MMU:
             */
            status = _ProcMgr_map (rprcloaderObject->pmHandle,
                                   (  ProcMgr_MASTERKNLVIRT
                                    | ProcMgr_SLAVEVIRT),
                                   &aInfo,
                                   ProcMgr_AddrType_MasterPhys);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "rprcloaderTrgWrite_copy",
                                     status,
                                     "ProcMgr_map failed!");
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                sectData = (Ptr) aInfo.addr [ProcMgr_AddrType_MasterKnlVirt];
                memcpy(sectData, (Ptr)dataAddr, numBytes);
                status = _ProcMgr_unmap(rprcloaderObject->pmHandle,
                                        (ProcMgr_MASTERKNLVIRT
                                         | ProcMgr_SLAVEVIRT),
                                        &aInfo,
                                        ProcMgr_AddrType_MasterPhys);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                if (status < 0) {
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rprcloaderTrgWrite_copy",
                                         status,
                                         "ProcMgr_unmap failed!");
                }
            }
       }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_3trace (curTrace, GT_LEAVE, "rprcloaderTrgWrite_copy", status,
               dstAddr, numBytes);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/* =============================================================================
 * APIs directly called by applications
 * =============================================================================
 */
/*!
 *  @brief      Function to get the default configuration for the rprcloader
 *              module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to rprcloader_setup filled in by the
 *              rprcloader module with the default parameters. If the user does
 *              not wish to make any change in the default parameters, this API
 *              is not required to be called.
 *
 *  @param      cfg        Pointer to the rprcloader module configuration
 *                         structure in which the default config is to be
 *                         returned.
 *
 *  @sa         rprcloader_setup
 */
Void
rprcloader_getConfig (rprcloader_Config * cfg)
{
    GT_1trace (curTrace, GT_ENTER, "rprcloader_getConfig", cfg);

    GT_assert (curTrace, (cfg != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (cfg == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_getConfig",
                             LOADER_E_INVALIDARG,
                             "Argument of type (rprcloader_Config *) passed "
                             "is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        Memory_copy (cfg,
                     &rprcloader_state.defCfg,
                     sizeof (rprcloader_Config));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "rprcloader_getConfig");
}


/*!
 *  @brief      Function to setup the rprcloader module.
 *
 *              This function sets up the rprcloader module. This function must
 *              be called before any other instance-level APIs can be invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then rprcloader_getConfig can be called to get the
 *              configuration filled with the default values. After this, only
 *              the required configuration values can be changed. If the user
 *              does not wish to make any change in the default parameters, the
 *              application can simply call rprcloader_setup with NULL
 *              parameters. The default parameters would get automatically used.
 *
 *  @param      cfg   Optional rprcloader module configuration. If provided as
 *                    NULL, default configuration is used.
 *
 *  @sa         rprcloader_destroy
 *              GateMutex_create
 */
Int
rprcloader_setup (rprcloader_Config * cfg)
{
    Int               status = LOADER_SUCCESS;
    rprcloader_Config tmpCfg;
    Error_Block       eb;

    GT_1trace (curTrace, GT_ENTER, "rprcloader_setup", cfg);

    Error_init(&eb);

    /* This sets the refCount variable is not initialized, upper 16 bits is
     * written with module Id to ensure correctness of refCount variable.
     */
    Atomic_cmpmask_and_set (&rprcloader_state.refCount,
                            LOADER_MAKE_MAGICSTAMP(0),
                            LOADER_MAKE_MAGICSTAMP(0));

    if (   Atomic_inc_return (&rprcloader_state.refCount)
        != LOADER_MAKE_MAGICSTAMP(1u)) {
        status = LOADER_S_ALREADYSETUP;
        GT_0trace (curTrace,
                   GT_2CLASS,
                   "rprcloader Module already initialized!");
    }
    else {
        if (cfg == NULL) {
            rprcloader_getConfig (&tmpCfg);
            cfg = &tmpCfg;
        }

        /* Create a default gate handle for local module protection. */
        rprcloader_state.gateHandle = (IGateProvider_Handle)
                              GateMutex_create ((GateMutex_Params *) NULL, &eb);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (rprcloader_state.gateHandle == NULL) {
            /*! @retval LOADER_E_FAIL Failed to create GateMutex! */
            status = LOADER_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "rprcloader_setup",
                                 status,
                                 "Failed to create GateMutex!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            /* Copy the user provided values into the state object. */
            Memory_copy (&rprcloader_state.cfg,
                         cfg,
                         sizeof (rprcloader_Config));

            /* Initialize the name to handles mapping array. */
            Memory_set (&rprcloader_state.loaderHandles,
                        0,
                        (sizeof (rprcloader_Handle) * MultiProc_MAXPROCESSORS));

            /* Initialize the generic RPRC parser. */
            //DLOAD_initialize();

            rprcloader_state.isSetup = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_setup", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to destroy the rprcloader module.
 *
 *              Once this function is called, other rprcloader module APIs, except
 *              for the rprcloader_getConfig API cannot be called anymore.
 *
 *  @sa         rprcloader_setup
 *              GateMutex_delete
 */
Int
rprcloader_destroy (Void)
{
    Int    status = LOADER_SUCCESS;
    UInt16 i;

    GT_0trace (curTrace, GT_ENTER, "rprcloader_destroy");

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (   Atomic_cmpmask_and_lt (&(rprcloader_state.refCount),
                                  LOADER_MAKE_MAGICSTAMP(0),
                                  LOADER_MAKE_MAGICSTAMP(1))
        == TRUE) {
        /*! @retval LOADER_E_INVALIDSTATE Module was not initialized */
        status = LOADER_E_INVALIDSTATE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_destroy",
                             status,
                             "Module was not initialized!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        if (   Atomic_dec_return (&rprcloader_state.refCount)
            == LOADER_MAKE_MAGICSTAMP(0)) {
            /* Check if any rprcloader instances have not been deleted so far. If not,
             * delete them.
             */
            for (i = 0 ; i < MultiProc_MAXPROCESSORS ; i++) {
                GT_assert (curTrace, (rprcloader_state.loaderHandles [i] == NULL));
                if (rprcloader_state.loaderHandles [i] != NULL) {
                    rprcloader_delete (&(rprcloader_state.loaderHandles [i]));
                }
            }

            if (rprcloader_state.gateHandle != NULL) {
                GateMutex_delete ((GateMutex_Handle *)
                                        &(rprcloader_state.gateHandle));
            }

            /* Finalize DLOAD module */
            //DLOAD_finalize ();

            rprcloader_state.isSetup = FALSE;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_destroy", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to initialize the parameters for this loader instance.
 *
 *  @param      handle  Handle to the loader instance. If provided as NULL, the
 *                      default parameters are returned, otherwise parameters
 *                      as configured for the loader instance are returned.
 *  @param      params  Configuration parameters.
 *
 *  @sa         rprcloader_create
 */
Void
rprcloader_Params_init (rprcloader_Handle handle, rprcloader_Params * params)
{
    rprcloader_Object *  rprcloader    = (rprcloader_Object *) handle;
    _rprcloader_Object * rprcloaderObject;

    GT_2trace (curTrace, GT_ENTER, "rprcloader_Params_init", handle, params);

    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (params == NULL) {
        GT_setFailureReason (curTrace,
                          GT_4CLASS,
                          "rprcloader_Params_init",
                          LOADER_E_INVALIDARG,
                          "Argument of type (rprcloader_Params *) passed "
                          "is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        if (handle == NULL) {
            params->reserved = 0;
        }
        else {
            rprcloaderObject = (_rprcloader_Object *)
                                        rprcloader->rprcloaderObject;
            GT_assert (curTrace, (rprcloaderObject != NULL));

            /* Return updated rprc loader instance specific parameters. */
            Memory_copy (params,
                         &(rprcloaderObject->params),
                         sizeof (rprcloader_Params));
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "rprcloader_Params_init");
}

/*!
 *  @brief      Function to create an instance of this loader.
 *
 *  @param      procId  Processor ID for which this loader instance is required.
 *  @param      params  Configuration parameters.
 *
 *  @sa         rprcloader_delete
 */
rprcloader_Handle
rprcloader_create (      UInt16              procId,
                   const rprcloader_Params * params)
{
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int                 status  = LOADER_SUCCESS;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    Loader_Object *     handle  = NULL;
    IArg                key;

    GT_2trace (curTrace, GT_ENTER, "rprcloader_create", procId, params);

    GT_assert (curTrace, IS_VALID_PROCID (procId));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (!IS_VALID_PROCID (procId)) {
        /*! @retval NULL Function failed */
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_create",
                             status,
                             "Invalid procId specified");
    }
    else if (params == NULL) {
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_create",
                             status,
                             "params passed is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Enter critical section protection. */
        key = IGateProvider_enter (rprcloader_state.gateHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        /* Check if the loader already exists for specified procId. */
        if (rprcloader_state.loaderHandles [procId] != NULL) {
            status = LOADER_E_ALREADYEXIST;
            GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_create",
                             status,
                             "Loader already exists for specified procId!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            /* Allocate memory for the handle */
            handle = (Loader_Object *) Memory_calloc (NULL,
                                                      sizeof (Loader_Object),
                                                      0,
                                                      NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (handle == NULL) {
                status = LOADER_E_MEMORY;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "rprcloader_create",
                                     status,
                                     "Memory allocation failed for handle!");
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

                /* Populate the handle fields */
                /* Loader functions. */
                handle->loaderFxnTable.attach = &rprcloader_attach;
                handle->loaderFxnTable.detach = &rprcloader_detach;
                handle->loaderFxnTable.load = &rprcloader_load;
                //handle->loaderFxnTable.loadSymbols =
                //                                   &rprcloader_loadSymbols;
                handle->loaderFxnTable.unload = &rprcloader_unload;
                handle->loaderFxnTable.getSymbolAddress =
                                               &rprcloader_getSymbolAddress;
                handle->loaderFxnTable.getEntryPt = &rprcloader_getEntryPt;
                //handle->loaderFxnTable.getSectionInfo =
                //                                     &rprcloader_getSectionInfo;
                //handle->loaderFxnTable.getSectionData =
                //                                     &rprcloader_getSectionData;

#if 0           /* The following are *not* used by DLOAD RPRC loader: */
                /* File I/O Interface */
                handle->fileFxnTable.seek = &rprcloaderFile_seek;
                handle->fileFxnTable.tell = &rprcloaderFile_tell;
                handle->fileFxnTable.read = &rprcloaderFile_read;
                handle->fileFxnTable.close = &rprcloaderFile_close;

                /* Host Memory management Interface */
                handle->memFxnTable.alloc = &rprcloaderMem_alloc;
                handle->memFxnTable.free  = &rprcloaderMem_free;

                /* Target Memory Allocator Interface */
                handle->trgMemFxnTable.alloc = &rprcloaderTrgMem_alloc;
                handle->trgMemFxnTable.free  = &rprcloaderTrgMem_free;

                /* Target Memory Access / Write Services */
                handle->trgWriteFxnTable.copy    = &rprcloaderTrgWrite_copy;
                handle->trgWriteFxnTable.write   = &rprcloaderTrgWrite_write;
                handle->trgWriteFxnTable.execute = &rprcloaderTrgWrite_execute;
                handle->trgWriteFxnTable.map     = &rprcloaderTrgWrite_map;
                handle->trgWriteFxnTable.unmap   = &rprcloaderTrgWrite_unmap;
                handle->trgWriteFxnTable.translate =
                                                  &rprcloaderTrgWrite_translate;
                /* Loading and Unloading of Dependent Files */
                handle->dllFxnTable.load   = &rprcloaderDll_loadDependent;
                handle->dllFxnTable.unload = &rprcloaderDll_unloadDependent;
#endif

                /* Allocate memory for the rprcloader object */
                handle->object = (rprcloader_Object *)
                                  Memory_calloc (NULL,
                                                 sizeof (rprcloader_Object),
                                                 0,
                                                 NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                if (handle->object == NULL) {
                    status = LOADER_E_MEMORY;
                    GT_setFailureReason (curTrace,
                         GT_4CLASS,
                         "rprcloader_create",
                         status,
                         "Memory allocation failed for rprcloader object!");
                }
                else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                    /* Allocate memory for the internal rprcloader object */
                    ((rprcloader_Object *) (handle->object))->rprcloaderObject =
                           (_rprcloader_Object *)
                                      Memory_calloc (NULL,
                                                sizeof (_rprcloader_Object),
                                                0,
                                                NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (((rprcloader_Object *)
                            (handle->object))->rprcloaderObject == NULL) {
                        status = LOADER_E_MEMORY;
                        GT_setFailureReason (curTrace,
                                    GT_4CLASS,
                                    "rprcloader_create",
                                    status,
                                    "Memory allocation failed for internal"
                                    " rprcloader object!");
                    }
                    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                        handle->procId = procId;
                        rprcloader_state.loaderHandles [procId] =
                                                    (rprcloader_Handle) handle;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    }
                }
            }
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Leave critical section protection. */
        IGateProvider_leave (rprcloader_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (status < 0) {
        if (NULL != handle) {
            if (NULL != handle->object) {
                Memory_free (NULL, handle->object, sizeof (rprcloader_Object));
            }
            Memory_free (NULL, handle, sizeof (Loader_Object));
        }
        /*! @retval NULL Function failed */
        handle = NULL;
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_create", handle);

    /*! @retval Valid-Handle Operation successful */
    return (rprcloader_Handle) handle;
}


/*!
 *  @brief      Function to delete an instance of this loader.
 *
 *              The user provided pointer to the handle is reset after
 *              successful completion of this function.
 *
 *  @param      handlePtr   Pointer to handle to the loader instance
 *
 *  @sa         rprcloader_create
 */
Int
rprcloader_delete (rprcloader_Handle * handlePtr)
{
    Int                  status           = LOADER_SUCCESS;
    rprcloader_Object *  object           = NULL;
    _rprcloader_Object * rprcloaderObject = NULL;
    Loader_Object *      handle;
    IArg                 key;

    GT_1trace (curTrace, GT_ENTER, "rprcloader_delete", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handlePtr == NULL) {
        /*! @retval LOADER_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_delete",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval LOADER_E_HANDLE Invalid NULL *handlePtr specified */
        status = LOADER_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_delete",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        handle = (Loader_Object *) (*handlePtr);
        /* Enter critical section protection. */
        key = IGateProvider_enter (rprcloader_state.gateHandle);

        GT_assert (curTrace, (IS_VALID_PROCID (handle->procId)));
        /* Reset handle in Loader handle array. */
        rprcloader_state.loaderHandles [handle->procId] = NULL;

        object = (rprcloader_Object *) handle->object;

        /* Free memory used for the rprcloader object. */
        if (handle->object != NULL) {
            rprcloaderObject = ((rprcloader_Object *)
                                (handle->object))->rprcloaderObject;
            /* Free memory used for the internal rprcloader object. */
            if (rprcloaderObject != NULL) {
                Memory_free (NULL,
                             rprcloaderObject,
                             sizeof (_rprcloader_Object));
            }

            Memory_free (NULL,
                         handle->object,
                         sizeof (rprcloader_Object));
            handle->object = NULL;
            rprcloader_Object_g = NULL;
        }

        /* Free memory used for the Loader object. */
        Memory_free (NULL, handle, sizeof (Loader_Object));
        *handlePtr = NULL;

        /* Leave critical section protection. */
        IGateProvider_leave (rprcloader_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "rprcloader_delete");

    /*! @retval LOADER_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to open a handle to an instance of this loader. This
 *              function is called when access to the loader is required from
 *              a different process.
 *
 *  @param      procId  Processor ID addressed by this rprcloader instance.
 *  @param      handle  Return parameter: Handle to the loader instance
 *
 *  @sa         rprcloader_close
 */
Int
rprcloader_open (rprcloader_Handle * handlePtr, UInt16 procId)
{
    Int status = LOADER_SUCCESS;

    GT_2trace (curTrace, GT_ENTER, "rprcloader_open", handlePtr, procId);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, IS_VALID_PROCID (procId));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handlePtr == NULL) {
        /*! @retval LOADER_E_HANDLE Invalid MULL handlePtr specified */
        status = LOADER_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_open",
                             status,
                             "Invalid NULL handlePtr specified");
    }
    else if (!IS_VALID_PROCID (procId)) {
        /*! @retval LOADER_E_INVALIDARG Invalid procId specified */
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_open",
                             status,
                             "Invalid procId specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Initialize return parameter handle. */
        *handlePtr = NULL;

        /* Check if the Loader exists and return the handle if found. */
        if (rprcloader_state.loaderHandles [procId] == NULL) {
            /*! @retval LOADER_E_NOTFOUND Specified instance not found */
            status = LOADER_E_NOTFOUND;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "rprcloader_open",
                              status,
                              "Specified rprcloader instance does not exist!");
        }
        else {
            *handlePtr = rprcloader_state.loaderHandles [procId];
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_open", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to close a handle to an instance of this loader.
 *
 *  @param      handlePtr   Pointer to handle to the loader instance
 *
 *  @sa         rprcloader_open
 */
Int
rprcloader_close (rprcloader_Handle * handlePtr)
{
    Int status = LOADER_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "rprcloader_close", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handlePtr == NULL) {
        /*! @retval LOADER_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_close",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval LOADER_E_HANDLE Invalid NULL *handlePtr specified */
        status = LOADER_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_close",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Nothing to be done for close. */
        *handlePtr = NULL;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_close", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/* =============================================================================
 * APIs called by Loader module (part of function table interface)
 * =============================================================================
 */
/*!
 *  @brief      Function to attach to the Loader.
 *
 *  @param      handle  Handle to the loader instance
 *  @param      params  Attach params
 *
 *  @sa         rprcloader_detach
 */
Int
rprcloader_attach (Loader_Handle handle, Loader_AttachParams * params)
{
    Int                 status = LOADER_SUCCESS ;
    rprcloader_Object * rprcloaderObj;

    GT_2trace (curTrace, GT_ENTER, "rprcloader_attach", handle, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

    /* Set the procHandle. */
    rprcloaderObj = (rprcloader_Object *) (((Loader_Object *) handle)->object);
    ((_rprcloader_Object *) (rprcloaderObj->rprcloaderObject))->procHandle =
                                                            params->procHandle;
    ((_rprcloader_Object *) (rprcloaderObj->rprcloaderObject))->pmHandle =
                                                               params->pmHandle;

    /* Set the procArch. */
    ((_rprcloader_Object *) (rprcloaderObj->rprcloaderObject))->procArch =
                                                            params->procArch;

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_attach",status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to detach from the Loader.
 *
 *  @param      handle  Handle to the loader instance
 *
 *  @sa         rprcloader_attach
 */
Int
rprcloader_detach (Loader_Handle handle)
{
    Int status = LOADER_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "rprcloader_detach", handle);

    /* Nothing to be done for this. */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_detach",status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to load the slave processor.
 *
 *  @param      handle    Handle to the loader instance
 *  @param      imagePath Full file path of the executable to be loaded
 *  @param      argc      Number of arguments to be sent to the slave main
 *                        function
 *  @param      argv      String array for the arguments
 *  @param      params    Loader specific parameters (if any)
 *  @param      fileId    Return parameter: ID of the loaded file
 *
 *  @sa         rprcloader_unload
 */
Int
rprcloader_load (Loader_Handle       handle,
                 String              imagePath,
                 UInt32              argc,
                 String *            argv,
                 Ptr                 params,
                 UInt32 *            fileId)
{
    Int                  status    = LOADER_SUCCESS ;
    OsalKfile_Handle     fileDesc  = NULL;
    Char *               mode      = "r";
    Loader_Object *      loaderObj = (Loader_Object *) handle;
    rprcloader_Object *  rprcloaderObj;
    _rprcloader_Object * _rprcloaderObj;

    GT_5trace (curTrace, GT_ENTER, "rprcloader_load",
               handle, imagePath, argc, argv, params);

    /* params are not applicable for file based rprcloader. */
    GT_assert (curTrace, (handle    != NULL));
    GT_assert (curTrace, (imagePath != NULL));
    //GT_assert (curTrace,
    //           (   ((argc == 0) && (argv == NULL))
    //            || ((argc != 0) && (argv != NULL)))) ;
    GT_assert (curTrace, (fileId   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval LOADER_E_HANDLE NULL provided for argument handle. */
        status = LOADER_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_load",
                             status,
                             "NULL provided for argument handle");
    }
    else if (imagePath == NULL) {
        /*! @retval LOADER_E_INVALIDARG NULL provided for argument imagePath. */
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_load",
                             status,
                             "NULL provided for argument imagePath");
    }
#if 0
    else if (   ((argc == 0) && (argv != NULL))
             || ((argc != 0) && (argv == NULL))) {
        /*! @retval LOADER_E_INVALIDARG Invalid values provided for argc/argv.*/
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_load",
                             status,
                             "Invalid values provided for argc/argv");
    }
#endif
    else if (fileId == NULL) {
        /*! @retval LOADER_E_INVALIDARG NULL provided for argument fileId. */
        status = LOADER_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_load",
                             status,
                             "NULL provided for argument fileId");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) */
        status = OsalKfile_open (imagePath, mode, &fileDesc);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "rprcloader_load",
                                 status,
                                 "Failed to open file!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            GT_assert (curTrace, (fileDesc != NULL));
            rprcloaderObj = (rprcloader_Object *) loaderObj->object;
            GT_assert (curTrace, (rprcloaderObj != NULL));
            _rprcloaderObj = rprcloaderObj->rprcloaderObject;
            GT_assert (curTrace, (_rprcloaderObj != NULL));
            _rprcloaderObj->fileDesc = fileDesc;

            /* TBD: Temporarily set the global to the processor being loaded. */
            rprcloader_Object_g = rprcloaderObj;

            status = load_firmware_file(_rprcloaderObj->procHandle, imagePath);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "rprcloader_load",
                                     status,
                                     "Failed to load RPRC file!");
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                /* Set the state of the Processor to loaded. */
                Processor_setState (_rprcloaderObj->procHandle,
                                    ProcMgr_State_Loaded);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            }
            OsalKfile_close(&fileDesc);
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_load", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to unload the previously loaded file on the slave
 *              processor.
 *
 *  @param      handle   Handle to the loader instance
 *  @param      fileId   ID of the file received from the load function
 *
 *  @sa         rprcloader_load
 */
Int
rprcloader_unload (Loader_Handle handle, UInt32 fileId)
{
    Int                  status    = LOADER_SUCCESS;
    Int                  tmpStatus = LOADER_SUCCESS;
    Loader_Object *      loaderObj = (Loader_Object *) handle;
    rprcloader_Object *  rprcloaderObj;
    _rprcloader_Object * _rprcloaderObj;

    GT_2trace (curTrace, GT_ENTER, "rprcloader_unload", handle, fileId);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval LOADER_E_HANDLE NULL provided for argument handle. */
        status = LOADER_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_load",
                             status,
                             "NULL provided for argument handle");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        rprcloaderObj = (rprcloader_Object *) loaderObj->object;
        GT_assert (curTrace, (rprcloaderObj != NULL));
        _rprcloaderObj = rprcloaderObj->rprcloaderObject;
        GT_assert (curTrace, (_rprcloaderObj != NULL));

        unload_firmware_file();

        /* TBD: Clear the loaded global object. */
        rprcloader_Object_g = NULL;

#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "rprcloader_unload",
                                 status,
                                 "Failed to unload RPRC file!");
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        //tmpStatus = OsalKfile_close ((OsalKfile_Handle *)
        //                                         &(_rprcloaderObj->fileDesc));
        if ((tmpStatus < 0) && (status >= 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "rprcloader_unload",
                                 status,
                                 "Failed to close the file!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_unload", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to retrieve the target address of a symbol from the
 *              specified file.
 *
 *  @param      handle   Handle to the loader instance
 *  @param      fileId   ID of the file received from the load function
 *  @param      symName  Name of the symbol
 *  @param      symValue Return parameter: Symbol address
 *
 *  @sa
 */
Int
rprcloader_getSymbolAddress (Loader_Handle       handle,
                            UInt32              fileId,
                            String              symName,
                            UInt32 *            symValue)
{
    Int status = LOADER_SUCCESS;

    GT_4trace (curTrace, GT_ENTER, "rprcloader_getSymbolAddress",
               handle, fileId, symName, symValue);

    GT_1trace (curTrace, GT_1CLASS,
                 "rprcloader_getSymbolAddress: symName [%s]",
                 symName);

    //if (!DLOAD_query_symbol(fileId, (const char *)symName,
    //                        (TARGET_ADDRESS *)symValue))  {
    //    status = LOADER_E_FAIL;
    //}

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_getSymbolAddress", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}

/*!
 *  @brief      Function to retrieve the entry point of the specified file.
 *
 *  @param      handle   Handle to the loader instance
 *  @param      fileId   ID of the file received from the load function
 *  @param      entryPt  Return parameter: Entry point of the executable
 *
 *  @sa         rprcloader_load
 */
Int
rprcloader_getEntryPt (Loader_Handle     handle,
                       UInt32            fileId,
                       UInt32 *          entryPt)
{
    Int status = LOADER_SUCCESS;

    GT_3trace (curTrace, GT_ENTER, "rprcloader_getEntryPt",
               handle, fileId, entryPt);

    GT_assert (curTrace, (handle  != NULL));
    GT_assert (curTrace, (entryPt != NULL));


    //if (!DLOAD_get_entry_point(fileId, (TARGET_ADDRESS)entryPt))  {
    //   status = LOADER_E_FAIL;
    //}

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "rprcloader_getEntryPt",
                             status,
                             "Failed to get rprc file entry point!");
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "rprcloader_getEntryPt", status);

    /*! @retval LOADER_SUCCESS Operation successful */
    return status;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
