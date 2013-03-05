/*
 *  @file   Dm8168M3VideoProc.c
 *
 * @brief       Processor implementation for DM8168VIDEOM3.
 *
 *              This module is responsible for taking care of device-specific
 *              operations for the processor. This module can be used
 *              stand-alone or as part of ProcMgr.
 *              The implementation is specific to DM8168VIDEOM3.
 *
 *
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



#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/IGateProvider.h>
#include <ti/sysbios/gates/GateMutex.h>
#endif /* #if defined(SYSLINK_BUILD_RTOS) */

#if defined(SYSLINK_BUILD_HLOS)
#include <ti/syslink/Std.h>
/* OSAL & Utils headers */
#include <ti/syslink/utils/Cfg.h>
#include <ti/syslink/utils/String.h>
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Memory.h>
#endif /* #if defined(SYSLINK_BUILD_HLOS) */

#if defined(SYSLINK_BUILDOS_LINUX)
#include <linux/string.h>
#else
#include <string.h>
#endif

#include <ti/syslink/utils/Trace.h>
/* Module level headers */
#include <ProcDefs.h>
#include <Processor.h>
#include <Dm8168M3VideoProc.h>
#include <_Dm8168M3VideoProc.h>
#include <Dm8168M3VideoHal.h>
#include <Dm8168M3VideoHalReset.h>
#include <Dm8168M3VideoHalBoot.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */

/*!
 *  @brief  Number of static entries in address translation table.
 */
#define AddrTable_STATIC_COUNT 4

/*!
 *  @brief  Max entries in address translation table.
 */
#define AddrTable_SIZE 32

/* Config param for L2MMU. This is not a typo, we are using the
 * same name (VPSS-M3) because both Ducati M3 processors use the
 * same L2MMU. The docs expose VPSS-M3 but not the VIDEO-M3 processor.
 */
#define PARAMS_mmuEnable "ProcMgr.proc[VPSS-M3].mmuEnable="


/*!
 *  @brief  DM8168VIDEOM3PROC Module state object
 */
typedef struct DM8168VIDEOM3PROC_ModuleObject_tag {
    UInt32                  configSize;
    /*!< Size of configuration structure */
    DM8168VIDEOM3PROC_Config cfg;
    /*!< DM8168VIDEOM3PROC configuration structure */
    DM8168VIDEOM3PROC_Config defCfg;
    /*!< Default module configuration */
    DM8168VIDEOM3PROC_Params      defInstParams;
    /*!< Default parameters for the DM8168VIDEOM3PROC instances */
    Bool                    isSetup;
    /*!< Indicates whether the DM8168VIDEOM3PROC module is setup. */
    DM8168VIDEOM3PROC_Handle procHandles [MultiProc_MAXPROCESSORS];
    /*!< Processor handle array. */
    IGateProvider_Handle             gateHandle;
    /*!< Handle of gate to be used for local thread safety */
} DM8168VIDEOM3PROC_ModuleObject;

/*!
 *  @brief  DM8168VIDEOM3PROC instance object.
 */
struct DM8168VIDEOM3PROC_Object_tag {
    DM8168VIDEOM3PROC_Params params;
    /*!< Instance parameters (configuration values) */
    Ptr                     halObject;
    /*!< Pointer to the Hardware Abstraction Layer object. */
    ProcMgr_Handle          pmHandle;
    /*!< Handle to proc manager */
};

/* Defines the DM8168VIDEOM3PROC object type. */
typedef struct DM8168VIDEOM3PROC_Object_tag DM8168VIDEOM3PROC_Object;

/* Default memory regions */
static UInt32 AddrTable_count = AddrTable_STATIC_COUNT;

/* static memory regions
 * CAUTION: AddrTable_STATIC_COUNT must match number of entries below.
 */
static ProcMgr_AddrInfo AddrTable[AddrTable_SIZE] =
    {
        { /* L2 BOOT, 16 KB */
            .addr     = { -1u, -1u, 0x55020000, 0x00000000, 0x55020000 },
            .size     = 0x4000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = ProcMgr_MASTERKNLVIRT
        },
        { /* L2 RAM, 240 KB */
            .addr     = { -1u, -1u, 0x55024000, 0x20004000, 0x55024000 },
            .size     = 0x3C000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = ProcMgr_MASTERKNLVIRT
        },
        { /* OCMC0, 256 KB */
            .addr     = { -1u, -1u, 0x40300000, 0x00300000, 0x40300000 },
            .size     = 0x40000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = (ProcMgr_MASTERKNLVIRT | ProcMgr_SLAVEVIRT)
        },
        { /* OCMC1, 256 KB */
            .addr     = { -1u, -1u, 0x40400000, 0x00400000, 0x40400000 },
            .size     = 0x40000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = (ProcMgr_MASTERKNLVIRT | ProcMgr_SLAVEVIRT)
        }
    };


/* =============================================================================
 *  Globals
 * =============================================================================
 */

/*!
 *  @var    DM8168VIDEOM3PROC_state
 *
 *  @brief  DM8168VIDEOM3PROC state object variable
 */
#if !defined(SYSLINK_BUILD_DEBUG)
static
#endif /* if !defined(SYSLINK_BUILD_DEBUG) */
DM8168VIDEOM3PROC_ModuleObject DM8168VIDEOM3PROC_state =
{
    .isSetup = FALSE,
    .configSize = sizeof (DM8168VIDEOM3PROC_Config),
    .gateHandle = NULL,
    .defInstParams.mmuEnable = FALSE,
    .defInstParams.numMemEntries = AddrTable_STATIC_COUNT
};

/* config override specified in SysLinkCfg.c, defined in ProcMgr.c */
extern String ProcMgr_sysLinkCfgParams;


/* =============================================================================
 * APIs directly called by applications
 * =============================================================================
 */
/*!
 *  @brief      Function to get the default configuration for the DM8168VIDEOM3PROC
 *              module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to DM8168VIDEOM3PROC_setup filled in by the
 *              DM8168VIDEOM3PROC module with the default parameters. If the user
 *              does not wish to make any change in the default parameters, this
 *              API is not required to be called.
 *
 *  @param      cfg        Pointer to the DM8168VIDEOM3PROC module configuration
 *                         structure in which the default config is to be
 *                         returned.
 *
 *  @sa         DM8168VIDEOM3PROC_setup
 */
Void
DM8168VIDEOM3PROC_getConfig (DM8168VIDEOM3PROC_Config * cfg)
{
    GT_1trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_getConfig", cfg);

    GT_assert (curTrace, (cfg != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (cfg == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_getConfig",
                             PROCESSOR_E_INVALIDARG,
                             "Argument of type (DM8168VIDEOM3PROC_Config *) passed "
                             "is null!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        memcpy(cfg,
               &(DM8168VIDEOM3PROC_state.defCfg),
               sizeof (DM8168VIDEOM3PROC_Config));
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_0trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_getConfig");
}


/*!
 *  @brief      Function to setup the DM8168VIDEOM3PROC module.
 *
 *              This function sets up the DM8168VIDEOM3PROC module. This function
 *              must be called before any other instance-level APIs can be
 *              invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then DM8168VIDEOM3PROC_getConfig can be called to get the
 *              configuration filled with the default values. After this, only
 *              the required configuration values can be changed. If the user
 *              does not wish to make any change in the default parameters, the
 *              application can simply call DM8168VIDEOM3PROC_setup with NULL
 *              parameters. The default parameters would get automatically used.
 *
 *  @param      cfg   Optional DM8168VIDEOM3PROC module configuration. If provided as
 *                    NULL, default configuration is used.
 *
 *  @sa         DM8168VIDEOM3PROC_destroy
 *              GateMutex_create
 */
Int
DM8168VIDEOM3PROC_setup (DM8168VIDEOM3PROC_Config * cfg)
{
    Int                       status = PROCESSOR_SUCCESS;
    DM8168VIDEOM3PROC_Config  tmpCfg;
    Error_Block               eb;
    Error_init(&eb);

    GT_1trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_setup", cfg);

    if (cfg == NULL) {
        DM8168VIDEOM3PROC_getConfig (&tmpCfg);
        cfg = &tmpCfg;
    }

    /* Create a default gate handle for local module protection. */
    DM8168VIDEOM3PROC_state.gateHandle = (IGateProvider_Handle)
                                GateMutex_create ((GateMutex_Params*)NULL, &eb);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168VIDEOM3PROC_state.gateHandle == NULL) {
        /*! @retval PROCESSOR_E_FAIL Failed to create GateMutex! */
        status = PROCESSOR_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_setup",
                             status,
                             "Failed to create GateMutex!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Copy the user provided values into the state object. */
        memcpy (&DM8168VIDEOM3PROC_state.cfg,
                     cfg,
                     sizeof (DM8168VIDEOM3PROC_Config));

        /* Initialize the name to handles mapping array. */
        memset (&DM8168VIDEOM3PROC_state.procHandles,
                    0,
                    (sizeof (DM8168VIDEOM3PROC_Handle) * MultiProc_getNumProcessors()));
        DM8168VIDEOM3PROC_state.isSetup = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_setup", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to destroy the DM8168VIDEOM3PROC module.
 *
 *              Once this function is called, other DM8168VIDEOM3PROC module APIs,
 *              except for the DM8168VIDEOM3PROC_getConfig API cannot be called
 *              anymore.
 *
 *  @sa         DM8168VIDEOM3PROC_setup
 *              GateMutex_delete
 */
Int
DM8168VIDEOM3PROC_destroy (Void)
{
    Int    status = PROCESSOR_SUCCESS;
    UInt16 i;

    GT_0trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_destroy");

    /* Check if any DM8168VIDEOM3PROC instances have not been deleted so far. If not,
     * delete them.
     */
    for (i = 0 ; i < MultiProc_getNumProcessors() ; i++) {
        GT_assert (curTrace, (DM8168VIDEOM3PROC_state.procHandles [i] == NULL));
        if (DM8168VIDEOM3PROC_state.procHandles [i] != NULL) {
            DM8168VIDEOM3PROC_delete (&(DM8168VIDEOM3PROC_state.procHandles [i]));
        }
    }

    if (DM8168VIDEOM3PROC_state.gateHandle != NULL) {
        GateMutex_delete ((GateMutex_Handle *)
                                &(DM8168VIDEOM3PROC_state.gateHandle));
    }

    DM8168VIDEOM3PROC_state.isSetup = FALSE;

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_destroy", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to initialize the parameters for this Processor
 *              instance.
 *
 *  @param      params  Configuration parameters to be returned
 *
 *  @sa         DM8168VIDEOM3PROC_create
 */
Void
DM8168VIDEOM3PROC_Params_init(
        DM8168VIDEOM3PROC_Handle    handle,
        DM8168VIDEOM3PROC_Params *  params)
{
    DM8168VIDEOM3PROC_Object * procObject = (DM8168VIDEOM3PROC_Object *) handle;
    Int i                                 = 0;
    ProcMgr_AddrInfo *     ai         = NULL;

    GT_2trace(curTrace, GT_ENTER,
        "DM8168VIDEOM3PROC_Params_init: handle=0x%x, params=0x%x\n",
        handle, params);

    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_Params_init",
                             PROCESSOR_E_INVALIDARG,
                             "Argument of type (DM8168VIDEOM3PROC_Params *) "
                             "passed is null!");
    }
    else {
#endif
        if (handle == NULL) {

            /* check for instance params override */
            Cfg_propBool(PARAMS_mmuEnable, ProcMgr_sysLinkCfgParams,
                &(DM8168VIDEOM3PROC_state.defInstParams.mmuEnable));

            memcpy(params, &(DM8168VIDEOM3PROC_state.defInstParams),
                sizeof(DM8168VIDEOM3PROC_Params));

            /* initialize the translation table */
            for (i = AddrTable_count; i < AddrTable_SIZE; i++) {
                ai = &AddrTable[i];
                ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterPhys] = -1u;
                ai->addr[ProcMgr_AddrType_SlaveVirt] = -1u;
                ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                ai->size = 0u;
                ai->isCached = FALSE;
                ai->mapMask = 0u;
                ai->isMapped = FALSE;
            }

            /* initialize refCount for all entries - both static and dynamic */
            for (i = 0; i < AddrTable_SIZE; i++) {
                AddrTable[i].refCount = 0u;
            }

            memcpy((Ptr)params->memEntries, AddrTable, sizeof (AddrTable));
        }
        else {
            /* return updated DM8168VIDEOM3PROC instance specific parameters */
            memcpy(params, &(procObject->params),
                sizeof (DM8168VIDEOM3PROC_Params));
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif

    GT_0trace(curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_Params_init");
}

/*!
 *  @brief      Function to create an instance of this Processor.
 *
 *  @param      name    Name of the Processor instance.
 *  @param      params  Configuration parameters.
 *
 *  @sa         DM8168VIDEOM3PROC_delete
 */
DM8168VIDEOM3PROC_Handle
DM8168VIDEOM3PROC_create (      UInt16                procId,
                     const DM8168VIDEOM3PROC_Params * params)
{
    Int                   status    = PROCESSOR_SUCCESS;
    Processor_Object *    handle    = NULL;
    DM8168VIDEOM3PROC_Object * object    = NULL;
    IArg                  key;
    List_Params           listParams;

    GT_2trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_create", procId, params);

    GT_assert (curTrace, IS_VALID_PROCID (procId));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (!IS_VALID_PROCID (procId)) {
        /* Not setting status here since this function does not return status.*/
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_create",
                             PROCESSOR_E_INVALIDARG,
                             "Invalid procId specified");
    }
    else if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_create",
                             PROCESSOR_E_INVALIDARG,
                             "params passed is NULL!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Enter critical section protection. */
        key = IGateProvider_enter (DM8168VIDEOM3PROC_state.gateHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        /* Check if the Processor already exists for specified procId. */
        if (DM8168VIDEOM3PROC_state.procHandles [procId] != NULL) {
            status = PROCESSOR_E_ALREADYEXIST;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168VIDEOM3PROC_create",
                              status,
                              "Processor already exists for specified procId!");
        }
        else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
            /* Allocate memory for the handle */
            handle = (Processor_Object *) Memory_calloc (NULL,
                                                      sizeof (Processor_Object),
                                                      0,
                                                      NULL);
            if (handle == NULL) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168VIDEOM3PROC_create",
                                     PROCESSOR_E_MEMORY,
                                     "Memory allocation failed for handle!");
            }
            else {
                /* Populate the handle fields */
                handle->procFxnTable.attach      = &DM8168VIDEOM3PROC_attach;
                handle->procFxnTable.detach      = &DM8168VIDEOM3PROC_detach;
                handle->procFxnTable.start       = &DM8168VIDEOM3PROC_start;
                handle->procFxnTable.stop        = &DM8168VIDEOM3PROC_stop;
                handle->procFxnTable.read        = &DM8168VIDEOM3PROC_read;
                handle->procFxnTable.write       = &DM8168VIDEOM3PROC_write;
                handle->procFxnTable.control     = &DM8168VIDEOM3PROC_control;
                handle->procFxnTable.map         = &DM8168VIDEOM3PROC_map;
                handle->procFxnTable.unmap       = &DM8168VIDEOM3PROC_unmap;
                handle->procFxnTable.translateAddr = &DM8168VIDEOM3PROC_translate;
                handle->state = ProcMgr_State_Unknown;

                /* Allocate memory for the DM8168VIDEOM3PROC handle */
                handle->object = Memory_calloc (NULL,
                                                sizeof (DM8168VIDEOM3PROC_Object),
                                                0,
                                                NULL);
                if (handle->object == NULL) {
                    status = PROCESSOR_E_MEMORY;
                    GT_setFailureReason (curTrace,
                                GT_4CLASS,
                                "DM8168VIDEOM3PROC_create",
                                status,
                                "Memory allocation failed for handle->object!");
                }
                else {
                    handle->procId = procId;
                    object = (DM8168VIDEOM3PROC_Object *) handle->object;
                    object->halObject = NULL;
                    /* Copy params into instance object. */
                    memcpy (&(object->params),
                                 (Ptr) params,
                                 sizeof (DM8168VIDEOM3PROC_Params));

                    /* Set the handle in the state object. */
                    DM8168VIDEOM3PROC_state.procHandles [procId] =
                                               (DM8168VIDEOM3PROC_Handle) handle;
                    /* Initialize the list of listeners */
                    List_Params_init(&listParams);
                    handle->registeredNotifiers = List_create(&listParams);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (handle->registeredNotifiers == NULL) {
                        /*! @retval PROCESSOR_E_FAIL OsalIsr_create failed */
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace,
                                             GT_4CLASS,
                                             "DM8168VIDEOM3PROC_create",
                                             status,
                                             "List_create failed");
                    }
                    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

                        handle->notifiersLock =
                                 OsalMutex_create(OsalMutex_Type_Interruptible);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                        if (handle->notifiersLock == NULL) {
                            /*! @retval PROCESSOR_E_FAIL OsalIsr_create failed*/
                            status = PROCESSOR_E_FAIL;
                            GT_setFailureReason (curTrace,
                                                 GT_4CLASS,
                                                 "DM8168VIDEOM3PROC_create",
                                                 status,
                                                 "OsalMutex_create failed");
                        }
                    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                }
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

        /* Leave critical section protection. */
        IGateProvider_leave (DM8168VIDEOM3PROC_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    if (status < 0) {
        if (handle !=  NULL) {
            if (handle->registeredNotifiers != NULL) {
                List_delete (&handle->registeredNotifiers);
            }
            if (handle->object != NULL) {
                Memory_free (NULL,
                             handle->object,
                             sizeof (DM8168VIDEOM3PROC_Object));
            }
            Memory_free (NULL, handle, sizeof (Processor_Object));
        }
        /*! @retval NULL Function failed */
        handle = NULL;
    }

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_create", handle);

    /*! @retval Valid-Handle Operation successful */
    return (DM8168VIDEOM3PROC_Handle) handle;
}


/*!
 *  @brief      Function to delete an instance of this Processor.
 *
 *              The user provided pointer to the handle is reset after
 *              successful completion of this function.
 *
 *  @param      handlePtr  Pointer to Handle to the Processor instance
 *
 *  @sa         DM8168VIDEOM3PROC_create
 */
Int
DM8168VIDEOM3PROC_delete (DM8168VIDEOM3PROC_Handle * handlePtr)
{
    Int                   status = PROCESSOR_SUCCESS;
    DM8168VIDEOM3PROC_Object * object = NULL;
    Processor_Object *    handle;
    IArg                  key;
    List_Elem *           elem    = NULL;
    Processor_RegisterElem * regElem = NULL;

    GT_1trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_delete", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_delete",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_delete",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        handle = (Processor_Object *) (*handlePtr);
        /* Enter critical section protection. */
        key = IGateProvider_enter (DM8168VIDEOM3PROC_state.gateHandle);

        /* Reset handle in PwrMgr handle array. */
        GT_assert (curTrace, IS_VALID_PROCID (handle->procId));
        DM8168VIDEOM3PROC_state.procHandles [handle->procId] = NULL;

        /* Free memory used for the DM8168VIDEOM3PROC object. */
        if (handle->object != NULL) {
            object = (DM8168VIDEOM3PROC_Object *) handle->object;
            Memory_free (NULL,
                         object,
                         sizeof (DM8168VIDEOM3PROC_Object));
            handle->object = NULL;
        }

        /*
         * Check the list of listeners to see if any are remaining
         * and reply to them
         */
        OsalMutex_delete(&handle->notifiersLock);

        while ((elem = List_dequeue(handle->registeredNotifiers)) != NULL) {
            regElem = (Processor_RegisterElem *)elem;

            /* Check if there is an associated timer and cancel it */
            if (regElem->timer != -1) {
                struct itimerspec value ;
                value.it_value.tv_sec = 0;
                value.it_value.tv_nsec = 0;
                value.it_interval.tv_sec = 0;
                value.it_interval.tv_nsec = 0;
                timer_settime(regElem->timer, 0, &value, NULL);

                timer_delete(regElem->timer);
                regElem->timer = -1;
            }

            /* Call the callback function so it can clean up. */
            regElem->info->cbFxn(handle->procId,
                                 NULL,
                                 handle->state,
                                 handle->state,
                                 ProcMgr_EventStatus_Canceled,
                                 regElem->info->arg);
            /* Free the memory */
            Memory_free(NULL, regElem, sizeof(Processor_RegisterElem));
        }

        /* Delete the list of listeners */
        List_delete(&handle->registeredNotifiers);

        /* Free memory used for the Processor object. */
        Memory_free (NULL, handle, sizeof (Processor_Object));
        *handlePtr = NULL;

        /* Leave critical section protection. */
        IGateProvider_leave (DM8168VIDEOM3PROC_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_delete", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to open a handle to an instance of this Processor. This
 *              function is called when access to the Processor is required from
 *              a different process.
 *
 *  @param      handlePtr   Handle to the Processor instance
 *  @param      procId      Processor ID addressed by this Processor instance.
 *
 *  @sa         DM8168VIDEOM3PROC_close
 */
Int
DM8168VIDEOM3PROC_open (DM8168VIDEOM3PROC_Handle * handlePtr, UInt16 procId)
{
    Int status = PROCESSOR_SUCCESS;

    GT_2trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_open", handlePtr, procId);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, IS_VALID_PROCID (procId));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_open",
                             status,
                             "Invalid NULL handlePtr specified");
    }
    else if (!IS_VALID_PROCID (procId)) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid procId specified */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_open",
                             status,
                             "Invalid procId specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Initialize return parameter handle. */
        *handlePtr = NULL;

        /* Check if the PwrMgr exists and return the handle if found. */
        if (DM8168VIDEOM3PROC_state.procHandles [procId] == NULL) {
            /*! @retval PROCESSOR_E_NOTFOUND Specified instance not found */
            status = PROCESSOR_E_NOTFOUND;
            GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_open",
                             status,
                             "Specified DM8168VIDEOM3PROC instance does not exist!");
        }
        else {
            *handlePtr = DM8168VIDEOM3PROC_state.procHandles [procId];
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_open", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to close a handle to an instance of this Processor.
 *
 *  @param      handlePtr  Pointer to Handle to the Processor instance
 *
 *  @sa         DM8168VIDEOM3PROC_open
 */
Int
DM8168VIDEOM3PROC_close (DM8168VIDEOM3PROC_Handle * handlePtr)
{
    Int status = PROCESSOR_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_close", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_close",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_close",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Nothing to be done for close. */
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_close", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/* =============================================================================
 * APIs called by Processor module (part of function table interface)
 * =============================================================================
 */
/*!
 *  @brief      Function to initialize the slave processor
 *
 *  @param      handle  Handle to the Processor instance
 *  @param      params  Attach parameters
 *
 *  @sa         DM8168VIDEOM3PROC_detach
 */
Int
DM8168VIDEOM3PROC_attach(
        Processor_Handle            handle,
        Processor_AttachParams *    params)
{

    Int                         status = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle = (Processor_Object *)handle;
    DM8168VIDEOM3PROC_Object *  object = NULL;
    Int                         i = 0;
    Int                         j = 0;
    Int                         index = 0;
    ProcMgr_AddrInfo *          me;
    SysLink_MemEntry *          entry;
    SysLink_MemEntry_Block *    memBlock = NULL;
    DM8168VIDEOM3_HalMmuCtrlArgs_Enable mmuEnableArgs;

    GT_2trace(curTrace, GT_ENTER,
        "--> DM8168VIDEOM3PROC_attach: handle=0x%x, parms=0x%x",
        handle, params);
    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_attach",
                             status,
                             "Invalid handle specified");
    }
    else if (params == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_attach",
                                 status,
                                 "Invalid params specified");
    }
    else {
#endif
        object = (DM8168VIDEOM3PROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        /* Added for Netra Ducati core0 is cortex M3 */
        params->procArch = Processor_ProcArch_M3;

        /* check for instance params override */
        Cfg_propBool(PARAMS_mmuEnable, ProcMgr_sysLinkCfgParams,
            &(object->params.mmuEnable));

        object->pmHandle = params->pmHandle;
        GT_0trace(curTrace, GT_1CLASS,
            "DM8168VIDEOM3PROC_attach: Mapping memory regions");

        /* update translation tables with memory map */
        for (i = 0; (memBlock != NULL) && (i < memBlock->numEntries)
            && (memBlock->memEntries[i].isValid) && (status >= 0); i++) {

            entry = &memBlock->memEntries[i];

            if (entry->map == FALSE) {
                /* update table with entries which don't require mapping */
                if (AddrTable_count != AddrTable_SIZE) {
                    me = &AddrTable[AddrTable_count];

                    me->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterPhys] =
                            entry->masterPhysAddr;
                    me->addr[ProcMgr_AddrType_SlaveVirt] = entry->slaveVirtAddr;
                    me->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                    me->size = entry->size;
                    me->isCached = entry->isCached;
                    me->mapMask = entry->mapMask;

                    AddrTable_count++;
                }
                else {
                    status = PROCESSOR_E_FAIL;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                        "DM8168VIDEOM3PROC_attach", status,
                        "AddrTable_SIZE reached!");
                }
            }
            else if (entry->map == TRUE) {
                /* send these entries back to ProcMgr for mapping */
                index = object->params.numMemEntries;

                if (index != ProcMgr_MAX_MEMORY_REGIONS) {
                    me = &object->params.memEntries[index];

                    me->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterPhys] =
                            entry->masterPhysAddr;
                    me->addr[ProcMgr_AddrType_SlaveVirt] = entry->slaveVirtAddr;
                    me->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                    me->size = entry->size;
                    me->isCached = entry->isCached;
                    me->mapMask = entry->mapMask;

                    object->params.numMemEntries++;
                }
                else {
                    status = PROCESSOR_E_FAIL;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168VIDEOM3PROC_attach", status,
                    "ProcMgr_MAX_MEMORY_REGIONS reached!");
                }
            }
            else {
                status = PROCESSOR_E_INVALIDARG;
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168VIDEOM3PROC_attach", status,
                    "Memory map has entry with invalid 'map' value");
            }
        } /* for (...) */

        if (status >= 0) {
            /* populate the return params */
            me = object->params.memEntries;
            params->numMemEntries = object->params.numMemEntries;
            memcpy((Ptr)params->memEntries, (Ptr)object->params.memEntries,
                sizeof(ProcMgr_AddrInfo) * params->numMemEntries);

            status = DM8168VIDEOM3_halInit (&(object->halObject), NULL);

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168VIDEOM3PROC_attach", status,
                    "DM8168VIDEOM3_halInit failed");
            }
            else {
#endif
                if ((procHandle->bootMode == ProcMgr_BootMode_Boot)
                    ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)) {

                    /* place the slave processor in reset */
                    status = DM8168VIDEOM3_halResetCtrl(object->halObject,
                        Processor_ResetCtrlCmd_Reset, NULL);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (status < 0) {
                        GT_setFailureReason(curTrace, GT_4CLASS,
                            "DM8168VIDEOM3PROC_attach", status,
                            "Failed to reset the slave processor");
                    }
                    else {
#endif
                        GT_0trace(curTrace, GT_1CLASS,
                            "DM8168VIDEOM3PROC_attach: slave is now in reset");

                        if (object->params.mmuEnable) {
                            mmuEnableArgs.numMemEntries = 0;
                            status = DM8168VIDEOM3_halMmuCtrl(object->halObject,
                                Processor_MmuCtrlCmd_Enable, &mmuEnableArgs);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                            if (status < 0) {
                                GT_setFailureReason(curTrace, GT_4CLASS,
                                    "DM8168VIDEOM3PROC_attach", status,
                                    "Failed to enable the slave MMU");
                            }
#endif
                            GT_0trace(curTrace, GT_2CLASS,
                                "DM8168VIDEOM3PROC_attach: Slave MMU "
                                "is configured!");
                        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    }
#endif
                }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            }
#endif
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif

    GT_1trace(curTrace, GT_LEAVE,
        "<-- DM8168VIDEOM3PROC_attach: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to detach from the Processor.
 *
 *  @param      handle  Handle to the Processor instance
 *
 *  @sa         DM8168VIDEOM3PROC_attach
 */
Int
DM8168VIDEOM3PROC_detach (Processor_Handle handle)
{
    Int                       status     = PROCESSOR_SUCCESS;
    Int                       tmpStatus  = PROCESSOR_SUCCESS;
    Processor_Object *        procHandle = (Processor_Object *) handle;
    DM8168VIDEOM3PROC_Object * object     = NULL;
    Int i                              = 0;
    ProcMgr_AddrInfo *    ai;

    GT_1trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_detach", handle);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_detach",
                             PROCESSOR_E_HANDLE,
                             "Invalid handle specified");
    }
    else {
#endif
        object = (DM8168VIDEOM3PROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)) {

            if (object->params.mmuEnable) {
                GT_0trace(curTrace, GT_2CLASS,
                    "DM8168VIDEOM3PROC_detach: Disabling Slave MMU ...");

                status = DM8168VIDEOM3_halMmuCtrl(object->halObject,
                    Processor_MmuCtrlCmd_Disable, NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
                if (status < 0) {
                    GT_setFailureReason(curTrace, GT_4CLASS,
                        "DM8168VIDEOM3PROC_detach", status,
                        "Failed to disable the slave MMU");
                }
#endif
            }

            /* delete all dynamically added entries */
            for(i = AddrTable_STATIC_COUNT; i < AddrTable_count; i++) {
                ai = &AddrTable[i];
                ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterPhys] = -1u;
                ai->addr[ProcMgr_AddrType_SlaveVirt] = -1u;
                ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                ai->size = 0u;
                ai->isCached = FALSE;
                ai->mapMask = 0u;
                ai->isMapped = FALSE;
                ai->refCount = 0u;
            }

            object->params.numMemEntries = AddrTable_STATIC_COUNT;
            AddrTable_count = AddrTable_STATIC_COUNT;

            /* place the slave processor into reset */
            tmpStatus = DM8168VIDEOM3_halResetCtrl(object->halObject,
                Processor_ResetCtrlCmd_Reset, NULL);

            GT_0trace(curTrace, GT_2CLASS,
                "DM8168VIDEOM3PROC_detach: Slave processor is now in reset");

#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if ((tmpStatus < 0) && (status >= 0)) {
                status = tmpStatus;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168VIDEOM3PROC_detach",
                                     status,
                                     "Failed to reset the slave processor");
            }
#endif
        }

        GT_0trace (curTrace,
                   GT_2CLASS,
                   "    DM8168VIDEOM3PROC_detach: Unmapping memory regions\n");

        tmpStatus = DM8168VIDEOM3_halExit (object->halObject);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        if ((tmpStatus < 0) && (status >= 0)) {
            status = tmpStatus;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_detach",
                                 status,
                                 "Failed to finalize HAL object");
        }
    }
#endif

    GT_1trace(curTrace, GT_LEAVE,
        "DM8168VIDEOM3PROC_detach: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to start the slave processor
 *
 *              Start the slave processor running from its entry point.
 *              Depending on the boot mode, this involves configuring the boot
 *              address and releasing the slave from reset.
 *
 *  @param      handle    Handle to the Processor instance
 *
 *  @sa         DM8168VIDEOM3PROC_stop, DM8168VIDEOM3_halBootCtrl, DM8168VIDEOM3_halResetCtrl
 */
Int
DM8168VIDEOM3PROC_start (Processor_Handle        handle,
                        UInt32                  entryPt,
                        Processor_StartParams * params)
{
    Int                   status        = PROCESSOR_SUCCESS ;
    Processor_Object *    procHandle    = (Processor_Object *) handle;
    DM8168VIDEOM3PROC_Object * object    = NULL;


    GT_3trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_start",
               handle, entryPt, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_start",
                             status,
                             "Invalid handle specified");
    }
    else if (params == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_start",
                                 status,
                                 "Invalid params specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        object = (DM8168VIDEOM3PROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            GT_1trace (curTrace,
                       GT_2CLASS,
                       "    DM8168VIDEOM3PROC_start: Configuring boot register\n"
                       "        Reset vector [0x%x]!\n",
                       entryPt);
            /* Specify the VIDEOM3 boot address in the boot config register */
            status = DM8168VIDEOM3_halBootCtrl (
                                            object->halObject,
                                            Processor_BootCtrlCmd_SetEntryPoint,
                                           (Ptr) entryPt);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168VIDEOM3PROC_start",
                                     status,
                                     "Failed to set slave boot entry point");
            }
            else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
                /* Write the branch instruction to at the boot address to
                 * branch to _c_int00
                 */


                /* Release the slave from reset */
                status = DM8168VIDEOM3_halResetCtrl (object->halObject,
                                                Processor_ResetCtrlCmd_Release,
                                                NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
                if (status < 0) {
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "DM8168VIDEOM3PROC_start",
                                         status,
                                         "Failed to release slave from reset");
                }
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        }

        /* For NoBoot mode, send an interrupt to the slave.
         * TBD: How should Dm8168M3VideoProc interface with Notify for this?
         */
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    if (status >= 0) {
        GT_0trace (curTrace,
                   GT_1CLASS,
                   "    DM8168VIDEOM3PROC_start: Slave successfully started!\n");
    }
    else {
        GT_0trace (curTrace,
                   GT_1CLASS,
                   "    DM8168VIDEOM3PROC_start: Slave could not be started!\n");
    }

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_start", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to stop the slave processor
 *
 *              Stop the execution of the slave processor. Depending on the boot
 *              mode, this may result in placing the slave processor in reset.
 *
 *  @param      handle    Handle to the Processor instance
 *
 *  @sa         DM8168VIDEOM3PROC_start, DM8168VIDEOM3_halResetCtrl
 */
Int
DM8168VIDEOM3PROC_stop (Processor_Handle handle)
{
    Int                   status       = PROCESSOR_SUCCESS ;
    Processor_Object *    procHandle   = (Processor_Object *) handle;
    DM8168VIDEOM3PROC_Object * object       = NULL;

    GT_1trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_stop", handle);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_stop",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        object = (DM8168VIDEOM3PROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            /* Slave is to be stopped only for Boot mode and NoLoad mode. */
            /* Place the slave processor in reset. */
            status = DM8168VIDEOM3_halResetCtrl (object->halObject,
                                            Processor_ResetCtrlCmd_Reset,
                                            NULL);
            GT_0trace (curTrace,
                       GT_1CLASS,
                       "    DM8168VIDEOM3PROC_stop: Slave is now in reset!\n");
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168VIDEOM3PROC_stop",
                                     status,
                                     "Failed to place slave in reset");
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_stop", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to read from the slave processor's memory.
 *
 *              Read from the slave processor's memory and copy into the
 *              provided buffer.
 *
 *  @param      handle     Handle to the Processor instance
 *  @param      procAddr   Address in host processor's address space of the
 *                         memory region to read from.
 *  @param      numBytes   IN/OUT parameter. As an IN-parameter, it takes in the
 *                         number of bytes to be read. When the function
 *                         returns, this parameter contains the number of bytes
 *                         actually read.
 *  @param      buffer     User-provided buffer in which the slave processor's
 *                         memory contents are to be copied.
 *
 *  @sa         DM8168VIDEOM3PROC_write
 */
Int
DM8168VIDEOM3PROC_read (Processor_Handle   handle,
                       UInt32             procAddr,
                       UInt32 *           numBytes,
                       Ptr                buffer)
{
    Int       status   = PROCESSOR_SUCCESS ;
    UInt8  *  procPtr8 = NULL;

    GT_4trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_read",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_read",
                             status,
                             "Invalid handle specified");
    }
    else if (numBytes == 0) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_read",
                                 status,
                                 "Invalid numBytes specified");
    }
    else if (buffer == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_read",
                                 status,
                                 "Invalid buffer specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        procPtr8 = (UInt8 *) procAddr ;
        buffer = memcpy (buffer, procPtr8, *numBytes);
        GT_assert (curTrace, (buffer != (UInt32) NULL));
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        if (buffer == (UInt32) NULL) {
            /*! @retval PROCESSOR_E_FAIL Failed in memcpy */
            status = PROCESSOR_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_read",
                                 status,
                                 "Failed in memcpy");
            *numBytes = 0;
        }
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_read",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to write into the slave processor's memory.
 *
 *              Read from the provided buffer and copy into the slave
 *              processor's memory.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      procAddr   Address in host processor's address space of the
 *                         memory region to write into.
 *  @param      numBytes   IN/OUT parameter. As an IN-parameter, it takes in the
 *                         number of bytes to be written. When the function
 *                         returns, this parameter contains the number of bytes
 *                         actually written.
 *  @param      buffer     User-provided buffer from which the data is to be
 *                         written into the slave processor's memory.
 *
 *  @sa         DM8168VIDEOM3PROC_read, DM8168VIDEOM3PROC_translateAddr
 */
Int
DM8168VIDEOM3PROC_write (Processor_Handle handle,
                        UInt32           procAddr,
                        UInt32 *         numBytes,
                        Ptr              buffer)
{
    Int                   status       = PROCESSOR_SUCCESS ;
    UInt8  *              procPtr8     = NULL;
    UInt8                 temp8_1;
    UInt8                 temp8_2;
    UInt8                 temp8_3;
    UInt8                 temp8_4;
    UInt32                temp;

    GT_4trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_write",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_write",
                             status,
                             "Invalid handle specified");
    }
    else if (numBytes == 0) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_write",
                                 status,
                                 "Invalid numBytes specified");
    }
    else if (buffer == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168VIDEOM3PROC_write",
                                 status,
                                 "Invalid buffer specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        if (*numBytes != sizeof (UInt32)) {
            procPtr8 = (UInt8 *) procAddr ;
            procAddr = (UInt32) memcpy (procPtr8,
                                        buffer,
                                        *numBytes);
            GT_assert (curTrace, (procAddr != (UInt32) NULL));
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (procAddr == (UInt32) NULL) {
                /*! @retval PROCESSOR_E_FAIL Failed in memcpy */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168VIDEOM3PROC_write",
                                     status,
                                     "Failed in memcpy");
                *numBytes = 0;
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        }
        else  {
             /* For 4 bytes, directly write as a UInt32 */
            temp8_1 = ((UInt8 *) buffer) [0];
            temp8_2 = ((UInt8 *) buffer) [1];
            temp8_3 = ((UInt8 *) buffer) [2];
            temp8_4 = ((UInt8 *) buffer) [3];
            temp = (UInt32) (    ((UInt32) temp8_4 << 24)
                             |   ((UInt32) temp8_3 << 16)
                             |   ((UInt32) temp8_2 << 8)
                             |   ((UInt32) temp8_1));
            *((UInt32*) procAddr) = temp;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_write", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to perform device-dependent operations.
 *
 *              Performs device-dependent control operations as exposed by this
 *              implementation of the Processor module.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      cmd        Device specific processor command
 *  @param      arg        Arguments specific to the type of command.
 *
 *  @sa
 */
Int
DM8168VIDEOM3PROC_control (Processor_Handle handle, Int32 cmd, Ptr arg)
{
    Int                   status       = PROCESSOR_SUCCESS ;

    GT_3trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_control", handle, cmd, arg);

    GT_assert (curTrace, (handle   != NULL));
    /* cmd and arg can be 0/NULL, so cannot check for validity. */

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_control",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* No control operations currently implemented. */
        /*! @retval PROCESSOR_E_NOTSUPPORTED No control operations are supported
                                             for this device. */
        status = PROCESSOR_E_NOTSUPPORTED;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_control",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Translate slave virtual address to master physical address.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      dstAddr    Returned: master physical address.
 *  @param      srcAddr    Slave virtual address.
 *
 *  @sa
 */
Int
DM8168VIDEOM3PROC_translate(
        Processor_Handle    handle,
        UInt32 *            dstAddr,
        UInt32              srcAddr)
{
    Int                         status = PROCESSOR_SUCCESS;
    Processor_Object *          procHandle= (Processor_Object *)handle;
    DM8168VIDEOM3PROC_Object *  object = NULL;
    UInt32                      i;
    UInt32                      startAddr;
    UInt32                      endAddr;
    UInt32                      offset;
    ProcMgr_AddrInfo *          ai;

    GT_3trace(curTrace, GT_ENTER,
        "DM8168VIDEOM3PROC_translate: handle=0x%x, dstAddr=0x%x, srcAddr=0x%x",
        handle, dstAddr, srcAddr);

    GT_assert (curTrace, (handle  != NULL));
    GT_assert (curTrace, (dstAddr != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_translate",
                             status,
                             "Invalid handle specified");
    }
    else if (dstAddr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG sglist provided as NULL */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_translate",
                             status,
                             "dstAddr provided as NULL");
    }
    else {
#endif
        object = (DM8168VIDEOM3PROC_Object *)procHandle->object;
        GT_assert(curTrace, (object != NULL));
        *dstAddr = -1u;

        for (i = 0; i < AddrTable_count; i++) {
            ai = &AddrTable [i];
            startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
            endAddr = startAddr + ai->size;

            if ((startAddr <= srcAddr) && (srcAddr < endAddr)) {
                offset = srcAddr - startAddr;
                *dstAddr = ai->addr[ProcMgr_AddrType_MasterPhys] + offset;
                break;
            }
        }

        if (*dstAddr == -1u) {
            if (!object->params.mmuEnable) {
                /* default to direct mapping (i.e. v=p) */
                *dstAddr = srcAddr;
            }
            else {
                /* srcAddr not found in slave address space */
                status = PROCESSOR_E_INVALIDARG;
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168VIDEOM3PROC_translate", status,
                    "srcAddr not found in slave address space");
            }
        }

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif
    GT_1trace(curTrace, GT_LEAVE,
        "DM8168VIDEOM3PROC_translate: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Map the given address translation into the slave mmu
 *
 *  @param      handle      Handle to the Processor object
 *  @param      dstAddr     Base virtual address
 *  @param      nSegs       Number of given segments
 *  @param      sglist      Segment list
 */
Int
DM8168VIDEOM3PROC_map(
        Processor_Handle    handle,
        UInt32 *            dstAddr,
        UInt32              nSegs,
        Memory_SGList *     sglist)
{
    Int                         status = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle = (Processor_Object *)handle;
    DM8168VIDEOM3PROC_Object *  object = NULL;
    Bool                        found = FALSE;
    UInt32                      startAddr;
    UInt32                      endAddr;
    UInt32                      i;
    UInt32                      j;
    ProcMgr_AddrInfo *          ai;
    DM8168VIDEOM3_HalMmuCtrlArgs_AddEntry addEntryArgs;

    GT_4trace(curTrace, GT_ENTER, "DM8168VIDEOM3PROC_map:",
        handle, *dstAddr, nSegs, sglist);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (sglist != NULL));
    GT_assert (curTrace, (nSegs > 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_map",
                             status,
                             "Invalid handle specified");
    }
    else if (sglist == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG sglist provided as NULL */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_map",
                             status,
                             "sglist provided as NULL");
    }
    else if (nSegs == 0) {
        /*! @retval PROCESSOR_E_INVALIDARG Number of segments provided is 0 */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_map",
                             status,
                             "Number of segments provided is 0");
    }
    else {
#endif
        object = (DM8168VIDEOM3PROC_Object *)procHandle->object;
        GT_assert (curTrace, (object != NULL));

        for (i = 0; (i < nSegs) && (status >= 0); i++) {
            /* Update the translation table with entries for which mapping
             * is required. Add the entry only if the range does not exist
             * in the translation table.
             */

            /* check in static entries first */
            for (j = 0; j < AddrTable_STATIC_COUNT; j++) {
                ai = &AddrTable[j];
                startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
                endAddr = startAddr + ai->size;

                if ((startAddr <= *dstAddr) && (*dstAddr < endAddr)) {
                    found = TRUE;
                    ai->refCount++;
                    GT_4trace(curTrace, GT_1CLASS, "DM8168VIDEOM3PROC_map: "
                        "found static entry: [%d] sva=0x%x, mpa=0x%x size=0x%x",
                        j, ai->addr[ProcMgr_AddrType_SlaveVirt],
                        ai->addr[ProcMgr_AddrType_MasterPhys], ai->size);
                    break;
                }
            }

            /* if not found in static entries, check in dynamic entries */
            if (!found) {
                for (j = AddrTable_STATIC_COUNT; j < AddrTable_count; j++) {
                    ai = &AddrTable[j];

                    if (ai->isMapped == TRUE) {
                        startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
                        endAddr = startAddr + ai->size;

                        if ((startAddr <= *dstAddr) && (*dstAddr < endAddr)
                            && ((*dstAddr + sglist[i].size) <= endAddr)) {
                            found = TRUE;
                            ai->refCount++;
                            GT_4trace(curTrace, GT_1CLASS,
                                "DM8168VIDEOM3PROC_map: found dynamic entry: "
                                "[%d] sva=0x%x, mpa=0x%x, size=0x%x",
                                j, ai->addr[ProcMgr_AddrType_SlaveVirt],
                                ai->addr[ProcMgr_AddrType_MasterPhys],ai->size);
                            break;
                        }
                    }
                }
            }

            /* If not found, add new entry to table. If mmu is disabled,
             * the assumption is that the ammu will be used.
             */
            if (!found) {
                if (AddrTable_count != AddrTable_SIZE) {
                    ai = &AddrTable[AddrTable_count];

                    ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                    ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                    ai->addr[ProcMgr_AddrType_MasterPhys] = sglist[i].paddr;
                    ai->addr[ProcMgr_AddrType_SlaveVirt] = *dstAddr;
                    ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                    ai->size = sglist[i].size;
                    ai->isCached = sglist[i].isCached;
                    ai->refCount++;

                    AddrTable_count++;

                    GT_4trace(curTrace, GT_1CLASS,
                        "DM8168VIDEOM3PROC_map: adding dynamic entry: "
                        "[%d] sva=0x%x, mpa=0x%x, size=0x%x",
                        (AddrTable_count - 1), *dstAddr, sglist[i].paddr,
                        sglist[i].size);
                }
                else {
                    status = PROCESSOR_E_FAIL;
                    ai = NULL;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                        "DM8168VIDEOM3PROC_map", status,
                        "AddrTable_SIZE reached!");
                }
            }

            /* if new entry, map into dsp mmu */
            if ((ai != NULL) && (ai->refCount == 1) && (status >= 0)) {
                ai->isMapped = TRUE;

                if (object->params.mmuEnable) {
                    GT_3trace(curTrace, GT_1CLASS,
                        "DM8168VIDEOM3PROC_map: adding entry to L2MMU: "
                        "sva=0x%x, spa=0x%x, size=0x%x",
                        *dstAddr, sglist[i].paddr, sglist[i].size);

                    /* add entry to L2 MMU */
                    addEntryArgs.masterPhyAddr = sglist [i].paddr;
                    addEntryArgs.size          = sglist [i].size;
                    addEntryArgs.slaveVirtAddr = (UInt32)*dstAddr;
                    /* TBD: elementSize, endianism, mixedSized are
                     * hard coded now, must be configurable later
                     */
                    addEntryArgs.elementSize   = ELEM_SIZE_16BIT;
                    addEntryArgs.endianism     = LITTLE_ENDIAN;
                    addEntryArgs.mixedSize     = MMU_TLBES;

                    status = DM8168VIDEOM3_halMmuCtrl(object->halObject,
                        Processor_MmuCtrlCmd_AddEntry, &addEntryArgs);

                    GT_3trace(curTrace, GT_1CLASS,
                        "DM8168VIDEOM3PROC_map: mmu add entry, "
                        "masterPhyAddr=0x%x, slaveVirAddr=0x%x, size=0x%x",
                        addEntryArgs.masterPhyAddr, addEntryArgs.slaveVirtAddr,
                        addEntryArgs.size);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (status < 0) {
                        GT_setFailureReason(curTrace, GT_4CLASS,
                            "DM8168VIDEOM3PROC_map", status,
                            "Processor_MmuCtrlCmd_AddEntry failed");
                    }
#endif
                }
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168VIDEOM3PROC_map", status,
                    "VIDEOM3 MMU configuration failed");
            }
#endif
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif
    GT_1trace(curTrace, GT_LEAVE, "DM8168VIDEOM3PROC_map:", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to unmap slave address from host address space
 *
 *  @param      handle      Handle to the Processor object
 *  @param      dstAddr     Return parameter: Pointer to receive the mapped
 *                          address.
 *  @param      size        Size of the region to be mapped.
s *
 *  @sa
 */
Int
DM8168VIDEOM3PROC_unmap(
        Processor_Handle    handle,
        UInt32              addr,
        UInt32              size)
{
    Int                         status = PROCESSOR_SUCCESS;
    Processor_Object *          procHandle = (Processor_Object *)handle;
    DM8168VIDEOM3PROC_Object *  object = NULL;
    ProcMgr_AddrInfo *          ai;
    Int                         i;
    UInt32                      startAddr;
    UInt32                      endAddr;
    DM8168VIDEOM3_HalMmuCtrlArgs_DeleteEntry deleteEntryArgs;

    GT_3trace (curTrace, GT_ENTER, "DM8168VIDEOM3PROC_unmap",
               handle, addr, size);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (size   != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_unmap",
                             status,
                             "Invalid handle specified");
    }
    else if (size == 0) {
        /*! @retval  PROCESSOR_E_INVALIDARG Size provided is zero */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168VIDEOM3PROC_unmap",
                             status,
                             "Size provided is zero");
    }
    else {
#endif
        object = (DM8168VIDEOM3PROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        /* Delete dynamically added non-default entries from translation
         * table only in last unmap called on that entry
         */
        for (i = AddrTable_STATIC_COUNT; i < AddrTable_count; i++) {
            ai = &AddrTable[i];

            if (!ai->isMapped) {
                continue;
            }

            startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
            endAddr = startAddr + ai->size;

            if ((startAddr <= addr) && (addr < endAddr)) {
                ai->refCount--;

                if (ai->refCount == 0) {
                    ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                    ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                    ai->addr[ProcMgr_AddrType_MasterPhys] = -1u;
                    ai->addr[ProcMgr_AddrType_SlaveVirt] = -1u;
                    ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                    ai->size = 0u;
                    ai->isCached = FALSE;
                    ai->mapMask = 0u;
                    ai->isMapped = FALSE;

                    if (object->params.mmuEnable) {
                        /* Remove the entry from the VIDEOM3 MMU also */
                        deleteEntryArgs.size          = size;
                        deleteEntryArgs.slaveVirtAddr = addr;
                        /* TBD: elementSize, endianism, mixedSized are
                         * hard coded now, must be configurable later
                         */
                        deleteEntryArgs.elementSize   = ELEM_SIZE_16BIT;
                        deleteEntryArgs.endianism     = LITTLE_ENDIAN;
                        deleteEntryArgs.mixedSize     = MMU_TLBES;

                        status = DM8168VIDEOM3_halMmuCtrl(object->halObject,
                            Processor_MmuCtrlCmd_DeleteEntry, &deleteEntryArgs);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                        if (status < 0) {
                            GT_setFailureReason(curTrace, GT_4CLASS,
                                "DM8168VIDEOM3PROC_unmap", status,
                                "VIDEOM3 MMU configuration failed");
                        }
#endif
                    }
                }
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif
    GT_1trace(curTrace, GT_LEAVE,
    "DM8168VIDEOM3PROC_unmap: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
