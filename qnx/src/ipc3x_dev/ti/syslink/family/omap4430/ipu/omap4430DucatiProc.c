/*
 *  @file   omap4430DucatiProc.c
 *
 * @brief       Processor implementation for OMAP4430DUCATI.
 *
 *              This module is responsible for taking care of device-specific
 *              operations for the processor. This module can be used
 *              stand-alone or as part of ProcMgr.
 *              The implementation is specific to OMAP4430DUCATI.
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


/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Trace.h>

/* Module level headers */
#include <ProcDefs.h>
#include <Processor.h>
#include <OMAP4430DucatiHal.h>
#include <OMAP4430DucatiHalReset.h>
#include <OMAP4430DucatiHalMmu.h>
#include <OMAP4430DucatiProc.h>
#include <OMAP4430DucatiEnabler.h>
#include <_MultiProc.h>
#include <hw/inout.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define INREG32(x) in32(x)
#define OUTREG32(x, y) out32(x, y)


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */

/*!
 *  @brief  Checks if a value lies in given range.
 */
#define IS_RANGE_VALID(x,min,max) (((x) < (max)) && ((x) >= (min)))



/*!
 *  @brief  OMAP4430DUCATIPROC Module state object
 */


/*OMAP4430 Module state object */
typedef struct OMAP4430DUCATIPROC_module_object_tag {
    UInt32                              cfgSize;
    /* Size of configuration structure */
    struct OMAP4430DUCATIPROC_Config    cfg;
    /* OMAP4430 configuration structure */
    struct OMAP4430DUCATIPROC_Config    defCfg;
    /* Default module configuration */
    Bool                                isSetup;
    /* Flag to indicate if module is setup */
    OMAP4430DUCATIPROC_Handle           procHandle;
    /* Processor handle array. */
    IGateProvider_Handle                gateHandle;
    /* void * of gate to be used for local thread safety */
}OMAP4430DUCATIPROC_ModuleObject;



#define SYSLINK_CARVEOUT
/* Default memory regions */
static ProcMgr_AddrInfo OMAP4430DUCATIPROC_defaultMemRegions [] =
#ifdef SYSLINK_CARVEOUT
{
    {
        /*addr     = */{ -1u, -1u, -1u, 0xA0000000, -1u},
        /*size     = */0x00100000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
    {
        /*addr     = */{ -1u, -1u, -1u, 0x80000000, -1u},
        /*size     = */0x02900000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
    {
        /*addr     = */{ -1u, -1u, -1u, 0x9F000000, -1u},
        /*size     = */0x00100000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
    {
        /*addr     = */{ -1u, -1u, -1u, 0x00000000, -1u},
        /*size     = */0x00600000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
};
#else
{
    {
        /*addr     = */{ -1u, -1u, -1u, 0xA0000000, -1u},
        /*size     = */0x00100000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
    {
        /*addr     = */{ -1u, -1u, -1u, 0x9F000000, -1u},
        /*size     = */0x00100000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
    {
        /*addr     = */{ -1u, -1u, -1u, 0x00000000, -1u},
        /*size     = */0x00600000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
    {
        /*addr     = */{ -1u, -1u, -1u, 0x80000000, -1u},
        /*size     = */0x06000000,
        /*isCached = */FALSE,
        /*isMapped = */FALSE,
        /*mapMask  = */ProcMgr_SLAVEVIRT | ProcMgr_MASTERKNLVIRT
    },
};
#endif

/* =============================================================================
 *  Globals
 * =============================================================================
 */
/*!
 *  @var    OMAP4430DUCATIPROC_state
 *
 *  @brief  OMAP4430DUCATIPROC state object variable
 */
#if !defined(SYSLINK_BUILD_DEBUG)
static
#endif /* if !defined(SYSLINK_BUILD_DEBUG) */
OMAP4430DUCATIPROC_ModuleObject OMAP4430SYSM3PROC_state =
{
    sizeof (OMAP4430DUCATIPROC_Config),
    {0},
    {0},
    FALSE,
    NULL,
    NULL
};

OMAP4430DUCATIPROC_ModuleObject OMAP4430APPM3PROC_state =
{
    sizeof (OMAP4430DUCATIPROC_Config),
    {0},
    {0},
    FALSE,
    NULL,
    NULL
};


/* =============================================================================
 * APIs directly called by applications
 * =============================================================================
 */
/*!
 *  @brief      Function to get the default configuration for the
 *              OMAP4430DUCATIPROC module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to OMAP4430DUCATIPROC_setup filled in by
 *              the OMAP4430DUCATIPROC module with the default parameters. If
 *              the user does not wish to make any change in the default
 *              parameters, this API is not required to be called.
 *
 *  @param      cfg        Pointer to the OMAP4430DUCATIPROC module
 *                         configuration structure in which the default config
 *                         is to be returned.
 *
 *  @sa         OMAP4430DUCATIPROC_setup
 */
Void
OMAP4430DUCATIPROC_get_config (OMAP4430DUCATIPROC_Config * cfg, Int ProcType)
{
    GT_1trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_get_config", cfg);

    GT_assert (curTrace, (cfg != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (cfg == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_get_config",
                             PROCESSOR_E_INVALIDARG,
                             "Argument of type (OMAP4430DUCATIPROC_Config *) "
                             "passed is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        switch (ProcType)
        {
            case PROCTYPE_SYSM3:
                Memory_copy (cfg,
                             &(OMAP4430SYSM3PROC_state.defCfg),
                             sizeof (OMAP4430DUCATIPROC_Config));
                break;

            case PROCTYPE_APPM3:
                Memory_copy (cfg,
                             &(OMAP4430APPM3PROC_state.defCfg),
                             sizeof (OMAP4430DUCATIPROC_Config));
            break;

            //case PROCTYPE_TESLA:
            //    Memory_copy (cfg,
            //                 &(OMAP4430TESLAPROC_state.defCfg),
            //                 sizeof (OMAP4430DUCATIPROC_Config));
            //break;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_get_config");
}


/*!
 *  @brief      Function to setup the OMAP4430DUCATIPROC module.
 *
 *              This function sets up the OMAP4430DUCATIPROC module. This
 *              function must be called before any other instance-level APIs
 *              can be invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then OMAP4430DUCATIPROC_getConfig can be called to
 *              get the configuration filled with the default values. After
 *              this, only the required configuration values can be changed. If
 *              the user does not wish to make any change in the default
 *              parameters, the application can simply call
 *              OMAP4430DUCATIPROC_setup with NULL parameters. The default
 *              parameters would get automatically used.
 *
 *  @param      cfg   Optional OMAP4430DUCATIPROC module configuration. If
 *                    provided as NULL, default configuration is used.
 *
 *  @sa         OMAP4430DUCATIPROC_destroy
 *              GateMutex_create
 */
Int
OMAP4430DUCATIPROC_setup (OMAP4430DUCATIPROC_Config * cfg, Int ProcType)
{
    Int                                 status  = PROCESSOR_SUCCESS;
    OMAP4430DUCATIPROC_Config           tmpCfg;
    OMAP4430DUCATIPROC_ModuleObject *   pState  = NULL;
    Error_Block                         eb;

    GT_1trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_setup", cfg);

    Error_init(&eb);

    if (cfg == NULL) {
        OMAP4430DUCATIPROC_get_config (&tmpCfg, ProcType);
        cfg = &tmpCfg;
    }

    switch (ProcType)
    {
        case PROCTYPE_SYSM3:
            pState = &OMAP4430SYSM3PROC_state;
            break;
        case PROCTYPE_APPM3:
            pState = &OMAP4430APPM3PROC_state;
            break;
        //case PROCTYPE_TESLA:
        //    pState = &OMAP4430TESLAPROC_state;
        //    break;
    }

    if (pState == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_setup",
                             PROCESSOR_E_INVALIDARG,
                             "Unsupported procType");
        return PROCESSOR_E_INVALIDARG;
    }

    /* Create a default gate handle for local module protection. */
    pState->gateHandle = (IGateProvider_Handle)
                               GateMutex_create ((GateMutex_Params *)NULL, &eb);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (pState->gateHandle == NULL) {
        /*! @retval PROCESSOR_E_FAIL Failed to create GateMutex! */
        status = PROCESSOR_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_setup",
                             status,
                             "Failed to create GateMutex!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Copy the user provided values into the state object. */
        Memory_copy (&pState->cfg,
                     cfg,
                     sizeof (OMAP4430DUCATIPROC_Config));

        /* Initialize the name to handles mapping array. */
        pState->procHandle = NULL;

        pState->isSetup = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_setup", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to destroy the OMAP4430DUCATIPROC module.
 *
 *              Once this function is called, other OMAP4430DUCATIPROC module
 *              APIs, except for the OMAP4430DUCATIPROC_getConfig API cannot be
 *              called anymore.
 *
 *  @sa         OMAP4430DUCATIPROC_setup
 *              GateMutex_delete
 */
Int
OMAP4430DUCATIPROC_destroy (Int ProcType)
{
    Int                                 status  = PROCESSOR_SUCCESS;
    OMAP4430DUCATIPROC_ModuleObject *   pState  = NULL;

    GT_0trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_destroy");

    switch(ProcType)
    {
        case PROCTYPE_SYSM3:
            pState = &OMAP4430SYSM3PROC_state;
            break;
        case PROCTYPE_APPM3:
            pState = &OMAP4430APPM3PROC_state;
            break;
        //case PROCTYPE_TESLA:
        //    pState = &OMAP4430TESLAPROC_state;
        //    break;
    }

    if (pState == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_setup",
                             PROCESSOR_E_INVALIDARG,
                             "Unsupported procType");
        return PROCESSOR_E_INVALIDARG;
    }

    /* Check if any OMAP4430DUCATIPROC instances have not been deleted so far.
     * If not, delete them.
     */
    if (pState->procHandle != NULL) {
        OMAP4430DUCATIPROC_delete(&pState->procHandle);
    }

    if (pState->gateHandle != NULL) {
        GateMutex_delete ((GateMutex_Handle *)
                                &(pState->gateHandle));
    }

    pState->isSetup = FALSE;

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_destroy", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to initialize the parameters for this Processor
 *              instance.
 *
 *  @param      params  Configuration parameters to be returned
 *
 *  @sa         OMAP4430DUCATIPROC_create
 */
Void
OMAP4430DUCATIPROC_Params_init (OMAP4430DUCATIPROC_Handle handle,
                                OMAP4430DUCATIPROC_Params * params,
                                Int ProcType)
{
    OMAP4430DUCATIPROC_Object * procObject =
                                           (OMAP4430DUCATIPROC_Object *) handle;
    UInt32                      numMemEntries   = 0;
    ProcMgr_AddrInfo *          pMemRegn        = NULL;

    GT_2trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_Params_init",
               handle, params);

    GT_assert (curTrace, (params != NULL));

    switch(ProcType)
    {
        case PROCTYPE_SYSM3:
            pMemRegn = OMAP4430DUCATIPROC_defaultMemRegions;
            numMemEntries = sizeof (OMAP4430DUCATIPROC_defaultMemRegions) /
                                sizeof(ProcMgr_AddrInfo);
            break;
        case PROCTYPE_APPM3:
            pMemRegn = OMAP4430DUCATIPROC_defaultMemRegions;
            numMemEntries = sizeof (OMAP4430DUCATIPROC_defaultMemRegions) /
                                sizeof(ProcMgr_AddrInfo);
            break;
        //case PROCTYPE_TESLA:
        //    pMemRegn = OMAP4430TESLAPROC_defaultMemRegions;
        //    break;
    }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_Params_init",
                             PROCESSOR_E_INVALIDARG,
                             "Argument of type (OMAP4430DUCATIPROC_Params *) "
                             "passed is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        if (handle == NULL) {
            params->numMemEntries =  numMemEntries;
            Memory_copy ((Ptr) params->memEntries,
                         pMemRegn,
                         (sizeof(ProcMgr_AddrInfo) * params->numMemEntries));
        }
        else {
            /* Return updated OMAP4430DUCATIPROC instance specific parameters */
            Memory_copy (params,
                         &(procObject->params),
                         sizeof (OMAP4430DUCATIPROC_Params));
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_Params_init");
}

/*!
 *  @brief      Function to create an instance of this Processor.
 *
 *  @param      name    Name of the Processor instance.
 *  @param      params  Configuration parameters.
 *
 *  @sa         OMAP4430DUCATIPROC_delete
 */

OMAP4430DUCATIPROC_Handle
OMAP4430DUCATIPROC_create (UInt16 procId,
                           const OMAP4430DUCATIPROC_Params * params)
{
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int                                 status    = PROCESSOR_SUCCESS;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    Processor_Object *                  handle    = NULL;
    OMAP4430DUCATIPROC_Object *         object    = NULL;
    IArg                                key;
    OMAP4430DUCATIPROC_ModuleObject *   pState    = NULL;
    List_Params                         listParams;

    switch(procId)
    {
        case PROCTYPE_SYSM3:
            pState = &OMAP4430SYSM3PROC_state;
            break;
        case PROCTYPE_APPM3:
            pState = &OMAP4430APPM3PROC_state;
            break;
        //case PROCTYPE_TESLA:
        //    pState = &OMAP4430TESLAPROC_state;
        //    break;
    }

    GT_2trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_create", procId, params);

    GT_assert (curTrace, IS_VALID_PROCID (procId));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (pState == NULL) {
        /* Not setting status here since this function does not return status.*/
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_create",
                             PROCESSOR_E_INVALIDARG,
                             "Invalid procId specified");
    }
    else if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_create",
                             PROCESSOR_E_INVALIDARG,
                             "params passed is NULL!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Enter critical section protection. */
        key = IGateProvider_enter (pState->gateHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        /* Check if the Processor already exists for specified procId. */
        if (pState->procHandle!= NULL) {
            status = PROCESSOR_E_ALREADYEXIST;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "OMAP4430DUCATIPROC_create",
                              status,
                              "Processor already exists for specified procId!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            /* Allocate memory for the handle */
            handle = (Processor_Object *) Memory_calloc (NULL,
                                                      sizeof (Processor_Object),
                                                      0,
                                                      NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (handle == NULL) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "OMAP4430DUCATIPROC_create",
                                     PROCESSOR_E_MEMORY,
                                     "Memory allocation failed for handle!");
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                /* Populate the handle fields */
                handle->procFxnTable.attach      = &OMAP4430DUCATIPROC_attach;
                handle->procFxnTable.detach      = &OMAP4430DUCATIPROC_detach;
                handle->procFxnTable.start       = &OMAP4430DUCATIPROC_start;
                handle->procFxnTable.stop        = &OMAP4430DUCATIPROC_stop;
                handle->procFxnTable.read        = &OMAP4430DUCATIPROC_read;
                handle->procFxnTable.write       = &OMAP4430DUCATIPROC_write;
                handle->procFxnTable.control     = &OMAP4430DUCATIPROC_control;
                handle->procFxnTable.map         = &OMAP4430DUCATIPROC_map;
                handle->procFxnTable.unmap       = &OMAP4430DUCATIPROC_unmap;
                handle->procFxnTable.translateAddr = &OMAP4430DUCATIPROC_translate;
                handle->procFxnTable.virtToPhys  = &OMAP4430DUCATI_virtToPhys;
                handle->procFxnTable.getProcInfo = &OMAP4430DUCATIPROC_procInfo;
                handle->state = ProcMgr_State_Unknown;

                /* Allocate memory for the OMAP4430DUCATIPROC handle */
                handle->object = Memory_calloc (NULL,
                                             sizeof (OMAP4430DUCATIPROC_Object),
                                             0,
                                             NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                if (handle->object == NULL) {
                    status = PROCESSOR_E_MEMORY;
                    GT_setFailureReason (curTrace,
                                GT_4CLASS,
                                "OMAP4430DUCATIPROC_create",
                                status,
                                "Memory allocation failed for handle->object!");
                }
                else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                    handle->procId = procId;
                    object = (OMAP4430DUCATIPROC_Object *) handle->object;
                    object->halObject = NULL;
                    object->procHandle = (Processor_Handle)handle;
                    /* Copy params into instance object. */
                    Memory_copy (&(object->params),
                                 (Ptr) params,
                                 sizeof (OMAP4430DUCATIPROC_Params));
                    object->params.procHandle = object->procHandle;

                    /* Set the handle in the state object. */
                    pState->procHandle = handle->object;

                    /* Initialize the list of listeners */
                    List_Params_init(&listParams);
                    handle->registeredNotifiers = List_create(&listParams);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (handle->registeredNotifiers == NULL) {
                        /*! @retval PROCESSOR_E_FAIL OsalIsr_create failed */
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason (curTrace,
                                             GT_4CLASS,
                                             "OMAP4430DUCATIPROC_create",
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
                                                 "OMAP4430DUCATIPROC_create",
                                                 status,
                                                 "OsalMutex_create failed");
                        }
                    }
                }
            }
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        /* Leave critical section protection. */
        IGateProvider_leave (pState->gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (status < 0) {
        if (handle !=  NULL) {
            if (handle->registeredNotifiers != NULL) {
                List_delete (&handle->registeredNotifiers);
            }
            if (handle->object != NULL) {
                Memory_free (NULL,
                             handle->object,
                             sizeof (OMAP4430DUCATIPROC_Object));
            }
            Memory_free (NULL, handle, sizeof (Processor_Object));
        }
        /*! @retval NULL Function failed */
        handle = NULL;
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_create", handle);

    /*! @retval Valid-Handle Operation successful */
    return (void*) handle;
}


/*!
 *  @brief      Function to delete an instance of this Processor.
 *
 *              The user provided pointer to the handle is reset after
 *              successful completion of this function.
 *
 *  @param      handlePtr  Pointer to Handle to the Processor instance
 *
 *  @sa         OMAP4430DUCATIPROC_create
 */
Int
OMAP4430DUCATIPROC_delete (OMAP4430DUCATIPROC_Handle * handlePtr)
{
    Int                                 status  = PROCESSOR_SUCCESS;
    OMAP4430DUCATIPROC_Object *         object  = NULL;
    Processor_Object *                  handle  = NULL;
    IArg                                key     = NULL;
    OMAP4430DUCATIPROC_ModuleObject *   pState  = NULL;
    List_Elem *                         elem    = NULL;
    Processor_RegisterElem *            regElem = NULL;


    GT_1trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_delete", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_delete",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_delete",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        handle = (Processor_Object *) (*handlePtr);
        /* Enter critical section protection. */
        if (handle->object != NULL) {
            object = (OMAP4430DUCATIPROC_Object *) handle->object;
            switch (handle->procId)
            {
                case PROCTYPE_SYSM3:
                    pState = &OMAP4430SYSM3PROC_state;
                    break;
                case PROCTYPE_APPM3:
                    pState = &OMAP4430APPM3PROC_state;
                    break;
                //case PROCTYPE_TESLA:
                //    pState = &OMAP4430TESLAPROC_state;
                //    break;
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (pState == NULL) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_delete",
                                 PROCESSOR_E_INVALIDARG,
                                 "Unsupported procType");
            return PROCESSOR_E_INVALIDARG;
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        key = IGateProvider_enter (pState->gateHandle);

        /* Reset handle in PwrMgr handle array. */
        GT_assert (curTrace, IS_VALID_PROCID (handle->procId));
        pState->procHandle = NULL;

        /* Free memory used for the PROC object. */
        Memory_free (NULL,
                     handle->object,
                     sizeof (Ptr));
        handle->object = NULL;

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
        IGateProvider_leave (pState->gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_delete", status);

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
 *  @sa         OMAP4430DUCATIPROC_close
 */
Int
OMAP4430DUCATIPROC_open (OMAP4430DUCATIPROC_Handle * handlePtr, UInt16 procId)
{
    Int                                 status  = PROCESSOR_SUCCESS;
    OMAP4430DUCATIPROC_ModuleObject *   pState  = NULL;

    GT_2trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_open",
               handlePtr, procId);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, IS_VALID_PROCID (procId));

    switch(procId)
    {
        case PROCTYPE_SYSM3:
            pState = &OMAP4430SYSM3PROC_state;
            break;
        case PROCTYPE_APPM3:
            pState = &OMAP4430APPM3PROC_state;
            break;
        //case PROCTYPE_TESLA:
        //    pState = &OMAP4430TESLAPROC_state;
        //    break;
    }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_open",
                             status,
                             "Invalid NULL handlePtr specified");
    }
    else if (pState == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid procId specified */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_open",
                             status,
                             "Invalid procId specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Initialize return parameter handle. */
        *handlePtr = NULL;

        /* Check if the PwrMgr exists and return the handle if found. */
        if (pState->procHandle == NULL) {
            /*! @retval PROCESSOR_E_NOTFOUND Specified instance not found */
            status = PROCESSOR_E_NOTFOUND;
            GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_open",
                             status,
                             "Specified instance does not exist!");
        }
        else {
            *handlePtr = pState->procHandle;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_open", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to close a handle to an instance of this Processor.
 *
 *  @param      handlePtr  Pointer to Handle to the Processor instance
 *
 *  @sa         OMAP4430DUCATIPROC_open
 */
Int
OMAP4430DUCATIPROC_close (OMAP4430DUCATIPROC_Handle * handlePtr)
{
    Int status = PROCESSOR_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "OMAP4430M3VIDEOPROC_close", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_close",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_close",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* Nothing to be done for close. */
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430M3VIDEOPROC_close", status);

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
 *  @sa         OMAP4430DUCATIPROC_detach
 */
Int
OMAP4430DUCATIPROC_attach (Processor_Handle         handle,
                           Processor_AttachParams * params)
{

    Int                            status       = PROCESSOR_SUCCESS ;
    Processor_Object *             procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object *    object       = NULL;
    ProcMgr_AddrInfo *             me;
    OMAP4430DUCATIPROC_ModuleObject *       pState;
    OMAP4430DUCATI_HalMmuCtrlArgs_Enable    enableArgs;

    GT_2trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_attach", handle, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

    switch(procHandle->procId)
    {
        case PROCTYPE_SYSM3:
            pState = &OMAP4430SYSM3PROC_state;
            break;
        case PROCTYPE_APPM3:
            pState = &OMAP4430APPM3PROC_state;
            break;
    }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_attach",
                             status,
                             "Invalid handle specified");
    }
    else if (params == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_attach",
                                 status,
                                 "Invalid params specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        /* Added for Netra Ducati core0 is cortex M3 */
        params->procArch = Processor_ProcArch_M3;
        GT_0trace (curTrace,
                   GT_2CLASS,
                   "    OMAP4430DUCATIPROC_attach: Mapping memory regions");

        /* Populate the return params */

        status = OMAP4430DUCATI_halInit (&(object->halObject), &object->params,
                                         procHandle->procId);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_attach",
                                 status,
                                 "OMAP4430DUCATI_halInit failed");
        }
        else if (object->params.numMemEntries > ProcMgr_MAX_MEMORY_REGIONS) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_attach",
                                 PROCESSOR_E_INVALIDARG,
                                 "numMemEntries out of range");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            me = object->params.memEntries;
            params->numMemEntries = object->params.numMemEntries;

            Memory_copy ((Ptr) params->memEntries,
                         (Ptr) object->params.memEntries,
                         sizeof (ProcMgr_AddrInfo) * params->numMemEntries);

            if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
                ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)) {

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                if (status < 0) {
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "OMAP4430DUCATIPROC_attach",
                                         status,
                                         "Failed to reset the slave processor");
                }
                else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                    GT_0trace (curTrace,
                               GT_1CLASS,
                               "    OMAP4430DUCATIPROC_attach: Slave is now "
                               "in reset!");

                    if (procHandle->procId == PROCTYPE_SYSM3) {
                        /* Enable MMU */
                        GT_0trace (curTrace,
                                   GT_2CLASS,
                                   "OMAP4430DUCATIPROC_attach: "
                                   "Enabling Slave MMU ...");
                        enableArgs.memEntries = NULL;
                        enableArgs.numMemEntries = 0;
                        status = OMAP4430DUCATI_halMmuCtrl (object->halObject,
                                                    Processor_MmuCtrlCmd_Enable,
                                                    &enableArgs);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                        if (status < 0) {
                            GT_setFailureReason (curTrace,
                                              GT_4CLASS,
                                              "OMAP4430DUCATIPROC_attach",
                                              status,
                                              "Failed to enable the slave MMU");
                        }
                    }
                }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_attach", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;

}


/*!
 *  @brief      Function to detach from the Processor.
 *
 *  @param      handle  Handle to the Processor instance
 *
 *  @sa         OMAP4430DUCATIPROC_attach
 */
Int
OMAP4430DUCATIPROC_detach (Processor_Handle handle)
{
    Int                       status     = PROCESSOR_SUCCESS;
    Int                       tmpStatus  = PROCESSOR_SUCCESS;
    Processor_Object *        procHandle = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object   = NULL;

    GT_1trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_detach", handle);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_detach",
                             PROCESSOR_E_HANDLE,
                             "Invalid handle specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)) {
            if (procHandle->procId == PROCTYPE_SYSM3) {
                /* Disable MMU */
                GT_0trace (curTrace,
                           GT_2CLASS,
                           "    OMAP4430DUCATIPROC_detach: "
                           "Disabling Slave MMU ...");
                status = OMAP4430DUCATI_halMmuCtrl (object->halObject,
                                                   Processor_MmuCtrlCmd_Disable,
                                                   NULL);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                if (status < 0) {
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "OMAP4430DUCATIPROC_detach",
                                         status,
                                         "Failed to disable the slave MMU");
                }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            }
            //No need to reset.. that will be done in STOP
           /* tmpStatus = OMAP4430DUCATI_halResetCtrl (object->halObject,
                                                   Processor_ResetCtrlCmd_Reset,
                                                   NULL);
            GT_0trace (curTrace,
                       GT_2CLASS,
                       "    OMAP4430DUCATIPROC_detach: Slave processor is "
                       "now in reset");*/
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if ((tmpStatus < 0) && (status >= 0)) {
                status = tmpStatus;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "OMAP4430DUCATIPROC_detach",
                                     status,
                                     "Failed to reset the slave processor");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }

        GT_0trace (curTrace,
                   GT_2CLASS,
                   "    OMAP4430DUCATIPROC_detach: Unmapping memory regions");

        tmpStatus = OMAP4430DUCATI_halExit(object->halObject, &object->params);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if ((tmpStatus < 0) && (status >= 0)) {
            status = tmpStatus;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_detach",
                                 status,
                                 "Failed to finalize HAL object");
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_detach", status);

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
 *  @sa         OMAP4430DUCATIPROC_stop, OMAP4430DUCATIPROC_halBootCtrl,
 *              OMAP4430DUCATIPROC_halResetCtrl
 */
Int
OMAP4430DUCATIPROC_start (Processor_Handle        handle,
                          UInt32                  entryPt,
                          Processor_StartParams * params)
{
    Int                          status        = PROCESSOR_SUCCESS ;
    Processor_Object *           procHandle    = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object  * object        = procHandle->object;

    GT_3trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_start",
               handle, entryPt, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_start",
                             status,
                             "Invalid handle specified");
    }
    else if (params == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_start",
                                 status,
                                 "Invalid params specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                             Processor_ResetCtrlCmd_MMU_Release,
                                             entryPt);
        if (status < 0) {
            /*! @retval status */
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATI_halResetCtrl",
                                 status,
                                 "Reset MMU_Release failed");
        }
        else {
            ducati_setup(object->halObject);
            status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                                 Processor_ResetCtrlCmd_Release,
                                                 entryPt);
            if (status < 0) {
                /*! @retval status */
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "OMAP4430DUCATI_halResetCtrl",
                                     status,
                                     "Reset Release failed");
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
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
 *  @sa         OMAP4430DUCATIPROC_start, OMAP4430DUCATIPROC_halResetCtrl
 */
Int
OMAP4430DUCATIPROC_stop (Processor_Handle handle)
{
    Int                         status       = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object       = procHandle->object;

    GT_1trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_stop", handle);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_stop",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                             Processor_ResetCtrlCmd_Reset,
                                             0);
        if (status < 0) {
            /*! @retval status */
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATI_halResetCtrl",
                                 status,
                                 "Reset failed");
        }

        ducati_destroy(object->halObject);
        status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                             Processor_ResetCtrlCmd_MMU_Reset,
                                             0);
        if (status < 0) {
            /*! @retval status */
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATI_halResetCtrl",
                                 status,
                                 "Reset MMU failed");
        }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_stop", status);

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
 *  @sa         OMAP4430DUCATIPROC_write
 */
Int
OMAP4430DUCATIPROC_read (Processor_Handle   handle,
                         UInt32             procAddr,
                         UInt32 *           numBytes,
                         Ptr                buffer)
{
    Int       status   = PROCESSOR_SUCCESS ;
    UInt8  *  procPtr8 = NULL;

    GT_4trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_read",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_read",
                             status,
                             "Invalid handle specified");
    }
    else if (numBytes == 0) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_read",
                                 status,
                                 "Invalid numBytes specified");
    }
    else if (buffer == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_read",
                                 status,
                                 "Invalid buffer specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        procPtr8 = (UInt8 *) procAddr ;
        buffer = Memory_copy (buffer, procPtr8, *numBytes);
        GT_assert (curTrace, (buffer != (UInt32) NULL));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (buffer == (UInt32) NULL) {
            /*! @retval PROCESSOR_E_FAIL Failed in Memory_copy */
            status = PROCESSOR_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_read",
                                 status,
                                 "Failed in Memory_copy");
            *numBytes = 0;
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_read",status);

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
 *  @sa         OMAP4430DUCATIPROC_read
 */
Int
OMAP4430DUCATIPROC_write (Processor_Handle handle,
                          UInt32           procAddr,
                          UInt32 *         numBytes,
                          Ptr              buffer)
{
    Int                   status       = PROCESSOR_SUCCESS ;
    Processor_Object *    procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object = NULL;
    UInt8  *              procPtr8     = NULL;
    UInt8                 temp8_1;
    UInt8                 temp8_2;
    UInt8                 temp8_3;
    UInt8                 temp8_4;
    UInt32                temp;

    GT_4trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_write",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_write",
                             status,
                             "Invalid handle specified");
    }
    else if (numBytes == 0) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_write",
                                 status,
                                 "Invalid numBytes specified");
    }
    else if (buffer == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_write",
                                 status,
                                 "Invalid buffer specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        if (*numBytes != sizeof (UInt32)) {
            procPtr8 = (UInt8 *) procAddr ;
            procAddr = (UInt32) Memory_copy (procPtr8,
                                             buffer,
                                             *numBytes);
            GT_assert (curTrace, (procAddr != (UInt32) NULL));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (procAddr == (UInt32) NULL) {
                /*! @retval PROCESSOR_E_FAIL Failed in Memory_copy */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "OMAP4430DUCATIPROC_write",
                                     status,
                                     "Failed in Memory_copy");
                *numBytes = 0;
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
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
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_write", status);

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
OMAP4430DUCATIPROC_control (Processor_Handle handle, Int32 cmd, Ptr arg)
{
    Int                         status       = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object       = NULL;

    GT_3trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_control",
               handle, cmd, arg);

    GT_assert (curTrace, (handle != NULL));
    /* cmd and arg can be 0/NULL, so cannot check for validity. */

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_control",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        /* No control operations currently implemented. */
        /*! @retval PROCESSOR_E_NOTSUPPORTED No control operations are supported
                                             for this device. */

        switch (cmd) {
            case Omap4430DucatiProc_CtrlCmd_Suspend:
                if (procHandle->state == ProcMgr_State_Running) {
                    if (procHandle->procId == PROCTYPE_SYSM3) {
                        status = save_mmu_ctxt(object->halObject,
                                               procHandle->procId);
                    }
                    if (status < 0) {
                        GT_setFailureReason(curTrace, GT_4CLASS,
                                            "OMAP4430DUCATIPROC_control",
                                            status,
                                            "Error while saving the MMU context");
                    }
                    else {
                        status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                                       Processor_ResetCtrlCmd_Reset,
                                                       0);
                        if (status < 0) {
                            GT_setFailureReason(curTrace, GT_4CLASS,
                                                "OMAP4430DUCATIPROC_control",
                                                status,
                                                "Error while Resetting proc");
                        }
                        else {
                            status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                                   Processor_ResetCtrlCmd_MMU_Reset,
                                                   0);
                            if (status < 0) {
                                GT_setFailureReason(curTrace, GT_4CLASS,
                                                    "OMAP4430DUCATIPROC_control",
                                                    status,
                                                    "Error while Resetting MMU");
                            }
                            else {
                                Processor_setState(handle, ProcMgr_State_Suspended);
                            }
                        }
                    }
                }
                else {
                    status = PROCESSOR_E_INVALIDSTATE;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                                        "OMAP4430DUCATIPROC_control",
                                        status,
                                        "Processor not is correct state to Suspend");
                }
                break;
            case Omap4430DucatiProc_CtrlCmd_Resume:
                if (procHandle->state == ProcMgr_State_Suspended) {
                    status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                                 Processor_ResetCtrlCmd_MMU_Release,
                                                 0);
                    if (status < 0) {
                        GT_setFailureReason(curTrace, GT_4CLASS,
                                            "OMAP4430DUCATIPROC_control",
                                            status,
                                            "Error while releasing proc MMU reset");
                    }
                    else {
                        if (procHandle->procId == PROCTYPE_SYSM3) {
                            status = restore_mmu_ctxt(object->halObject,
                                                      procHandle->procId);
                        }
                        if (status < 0) {
                            GT_setFailureReason(curTrace, GT_4CLASS,
                                               "OMAP4430DUCATIPROC_control",
                                               status,
                                               "Error while restoring MMU context");
                        }
                        else {
                            status = OMAP4430DUCATI_halResetCtrl(object->halObject,
                                                     Processor_ResetCtrlCmd_Release,
                                                     0);
                            if (status < 0) {
                                GT_setFailureReason(curTrace, GT_4CLASS,
                                                    "OMAP4430DUCATIPROC_control",
                                                    status,
                                                    "Error while releasing reset");
                            }
                            else {
                                Processor_setState(handle, ProcMgr_State_Running);
                            }
                        }
                    }
                }
                else {
                    status = PROCESSOR_E_INVALIDSTATE;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                                        "OMAP4430DUCATIPROC_control",
                                        status,
                                        "Processor not is correct state to Resume");
                }
                break;
            default:
                status = PROCESSOR_E_NOTSUPPORTED;
        }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_control",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to translate slave physical address to master physical
 *              address.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      dstAddr    Returned: master physical address.
 *  @param      srcAddr    Slave physical address.
 *
 *  @sa
 */
Int
OMAP4430DUCATIPROC_translate (Processor_Handle handle,
                              UInt32 *         dstAddr,
                              UInt32           srcAddr)
{
    Int                         status       = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object       = NULL;
    UInt32                      i;
    ProcMgr_AddrInfo *          ai;
    ProcMgr_AddrInfo *          pMemRegn     = NULL;
    UInt32                      nRegions     = 0;

    GT_3trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_translate",
               handle, dstAddr, srcAddr);

    GT_assert (curTrace, (handle  != NULL));
    GT_assert (curTrace, (dstAddr != NULL));

    GT_1trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_Params_init", handle);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_translate",
                             status,
                             "Invalid handle specified");
    }
    else if (dstAddr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG sglist provided as NULL */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_translate",
                             status,
                             "dstAddr provided as NULL");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        pMemRegn = object->params.memEntries;
        nRegions = object->params.numMemEntries;

        *dstAddr = -1u;
        for (i = 0;i < nRegions;i++)
        {
             ai = &pMemRegn [i];
             if (   (srcAddr >= ai->addr [ProcMgr_AddrType_SlaveVirt])
                 && (srcAddr < (  ai->addr [ProcMgr_AddrType_SlaveVirt]
                                + ai->size))) {
                 *dstAddr =   ai->addr [ProcMgr_AddrType_MasterPhys]
                            + (srcAddr - ai->addr [ProcMgr_AddrType_SlaveVirt]);
                 break;
             }
        }

        if (*dstAddr == -1u) {
            /*! @retval PROCESSOR_E_FAIL srcAddr not found in slave address
             *          space */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_translate",
                                 status,
                                 "srcAddr not found in slave address space");
        }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_translate",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to map slave address to host address space
 *
 *              Map the provided slave address to master address space. This
 *              function also maps the specified address to slave MMU space.
 *
 *  @param      handle      Handle to the Processor object
 *  @param      mapType     Type of mapping to be performed.
 *  @param      addrInfo    Structure containing map info.
 *  @param      srcAddrType Source address type.
 *
 *  @sa
 */
Int
OMAP4430DUCATIPROC_map (Processor_Handle handle,
                        UInt32 *         dstAddr,
                        UInt32           nSegs,
                        Memory_SGList *  sglist)
{
    Int                         status       = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object       = NULL;
    UInt32                      i;
    OMAP4430DUCATI_HalMmuCtrlArgs_AddEntry addEntryArgs;

    GT_4trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_map",
               handle, dstAddr, nSegs, sglist);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (sglist != NULL));
    GT_assert (curTrace, (nSegs > 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_map",
                             status,
                             "Invalid handle specified");
    }
    else if (sglist == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG sglist provided as NULL */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_map",
                             status,
                             "sglist provided as NULL");
    }
    else if (nSegs == 0) {
        /*! @retval PROCESSOR_E_INVALIDARG Number of segments provided is 0 */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_map",
                             status,
                             "Number of segments provided is 0");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        /* Program the mmu with the sglist */
        /* Program the DSP MMU also */
        for (i = 0; (i < nSegs) && (status >= 0); i++)
        {
            addEntryArgs.masterPhyAddr = sglist [i].paddr;
            addEntryArgs.size          = sglist [i].size;
            addEntryArgs.slaveVirtAddr = (UInt32)*dstAddr;
            /*TBD : elementSize, endianism, mixedSized are hard coded now,
             *      must be configurable later*/
            addEntryArgs.elementSize   = ELEM_SIZE_16BIT;
            addEntryArgs.endianism     = LITTLE_ENDIAN;
            addEntryArgs.mixedSize     = MMU_TLBES;
            addEntryArgs.slaveVirtAddr = get_DucatiVirtAdd(object->halObject,
                                                    addEntryArgs.masterPhyAddr);
            *(dstAddr) = addEntryArgs.slaveVirtAddr;
            if(addEntryArgs.slaveVirtAddr == 0) {
                status = OMAP4430DUCATI_halMmuCtrl (object->halObject,
                                                  Processor_MmuCtrlCmd_AddEntry,
                                                  &addEntryArgs);

            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "OMAP4430DUCATIPROC_map",
                                 status,
                                 "DSP MMU configuration failed");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_map",status);

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
OMAP4430DUCATIPROC_unmap (Processor_Handle handle,
                          UInt32           addr,
                          UInt32           size)
{
    Int                         status       = PROCESSOR_SUCCESS ;
    Processor_Object *          procHandle   = (Processor_Object *) handle;
    OMAP4430DUCATIPROC_Object * object       = NULL;
    OMAP4430DUCATI_HalMmuCtrlArgs_DeleteEntry deleteEntryArgs;

    GT_3trace (curTrace, GT_ENTER, "OMAP4430DUCATIPROC_unmap",
               handle, addr, size);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (size   != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_unmap",
                             status,
                             "Invalid handle specified");
    }
    else if (size == 0) {
        /*! @retval  PROCESSOR_E_INVALIDARG Size provided is zero */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_unmap",
                             status,
                             "Size provided is zero");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        object = (OMAP4430DUCATIPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        /* Remove the entry from the DSP MMU also */

        deleteEntryArgs.size          = size;
        deleteEntryArgs.slaveVirtAddr = addr;
        /*TBD : elementSize, endianism, mixedSized are hard coded now,
         *        must be configurable later*/
        deleteEntryArgs.elementSize   = ELEM_SIZE_16BIT;
        deleteEntryArgs.endianism     = LITTLE_ENDIAN;
        deleteEntryArgs.mixedSize     = MMU_TLBES;

        status = OMAP4430DUCATI_halMmuCtrl(object->halObject,
                                           Processor_MmuCtrlCmd_DeleteEntry,
                                           &deleteEntryArgs);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "OMAP4430DUCATIPROC_unmap",
                             status,
                             "DSP MMU configuration failed");
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    GT_1trace (curTrace, GT_LEAVE, "OMAP4430DUCATIPROC_unmap",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}

Int
OMAP4430DUCATIPROC_procInfo (Processor_Handle   handle,
                             ProcMgr_ProcInfo * procInfo)
{
    Processor_Object *              procHandle  = NULL;
    OMAP4430DUCATIPROC_Object *     object      = NULL;
    ProcMgr_AddrInfo *              entry       = NULL;
    Int                             i           = 0;

    procHandle = (Processor_Object *)handle;
    object = procHandle->object;
    for (i = 0; i < object->params.numMemEntries; i++) {
        entry = &object->params.memEntries[i];

        procInfo->memEntries[i].info.addr[ProcMgr_AddrType_MasterKnlVirt] =
            entry->addr[ProcMgr_AddrType_MasterKnlVirt];

        procInfo->memEntries[i].info.addr[ProcMgr_AddrType_SlaveVirt] =
            entry->addr[ProcMgr_AddrType_SlaveVirt];


    }
    procInfo->numMemEntries = object->params.numMemEntries;
    return 0;
}

Int
OMAP4430DUCATI_virtToPhys (Processor_Handle handle,
                           UInt32           da,
                           UInt32 *         mappedEntries,
                           UInt32           numEntries)
{
    if(!handle || !mappedEntries || !numEntries) {
        return -1;
    }
    return 0;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
