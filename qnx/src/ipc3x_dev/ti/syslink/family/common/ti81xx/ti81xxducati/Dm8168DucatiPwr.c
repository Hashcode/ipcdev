/*
 *  @file   Dm8168DucatiPwr.c
 *
 *  @brief      PwrMgr implementation for DM8168DUCATI.
 *
 *              This module is responsible for handling power requests for
 *              the ProcMgr. The implementation is common to DM8168VIDEOM3 and
 *              DM8168VPSSM3.
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


#if defined(SYSLINK_BUILD_HLOS)
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/String.h>
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/Gate.h>
#if defined (SYSLINK_BUILDOS_LINUX)
#include <ti/syslink/inc/knl/Linux/LinuxClock.h>
#endif /* #if defined(SYSLINK_BUILDOS_LINUX) */
#endif /* #if defined(SYSLINK_BUILD_HLOS) */


#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/IGateProvider.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/syslink/utils/_Memory.h>
#include <ti/syslink/inc/Dm8168Clock.h>
#endif /* #if defined(SYSLINK_BUILD_RTOS) */

#if defined(SYSLINK_BUILDOS_LINUX)
#include <linux/string.h>
#else
#include <string.h>
#endif

#include <ti/syslink/utils/Trace.h>

/* Module level headers */
#include <ti/ipc/MultiProc.h>
#include <ti/syslink/inc/_MultiProc.h>
#include <ti/syslink/inc/knl/PwrDefs.h>
#include <ti/syslink/inc/knl/PwrMgr.h>
#include <ti/syslink/inc/knl/Dm8168DucatiPwr.h>
#include <ti/syslink/inc/knl/_Dm8168DucatiPwr.h>
#include <ti/syslink/inc/knl/Linux/Dm8168DucatiMmu.h>

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  DM8168VIDEOM3 module and mmr addresses (physical)
 */

#define DM8168M3_PRCM_BASE_ADDR      0x48180000
#define DM8168M3_PRCM_SIZE           0x00002FFF

/* Ducati MMU base */
#define DUCATI_MMU_CFG               0x55080000
#define DUCATI_MMU_CFG_SIZE          0x00000FFF


#define DUCATI_BASE_ADDR             0x55020000
#define DUCATI_BASE_ADDR_SIZE        0x00000008

#define MAX_WAIT_COUNT               0x50000

#define REG(x)              *((volatile UInt32 *) (x))
#define MEM(x)              *((volatile UInt32 *) (x))

/* Macro to make a correct module magic number with refCount */
#define DM8168DUCATIPWR_MAKE_MAGICSTAMP(x) \
                            ((DM8168DUCATIPWR_MODULEID << 12u) | (x))

/*!
 *  @brief  DM8168DUCATIPWR Module state object
 */
typedef struct DM8168DUCATIPWR_ModuleObject_tag {
    UInt32                  refCount;
    /* Module Reference count */
    UInt32                  pwrstateRefCount;
    /* Reference count */
    UInt32                 configSize;
    /*!< Size of configuration structure */
    DM8168DUCATIPWR_Config  cfg;
    /*!< DM8168DUCATIPWR configuration structure */
    DM8168DUCATIPWR_Config  defCfg;
    /*!< Default module configuration */
    Bool                   isSetup;
    /*!< Indicates whether the DM8168DUCATIPWR module is setup. */
    DM8168DUCATIPWR_Handle  pwrHandles [MultiProc_MAXPROCESSORS];
    /*!< PwrMgr handle array. */
    IGateProvider_Handle   gateHandle;
    /*!< Handle of gate to be used for local thread safety */
    Ptr                    ducatiClkHandle;
    /*!< clock handle used to call kernel APIs */
    Ptr                    ducatiSpinlockHandle;
    /*!< dsp Spinlock handle */
    Ptr                    ducatiMailboxHandle;
    /*!< dsp Mailbox handle */
} DM8168DUCATIPWR_ModuleObject;

/* =============================================================================
 *  Globals
 * =============================================================================
 */
/*!
 *  @var    DM8168DUCATIPWR_state
 *
 *  @brief  DM8168DUCATIPWR state object variable
 */
#if !defined(SYSLINK_BUILD_DEBUG)
static
#endif /* if !defined(SYSLINK_BUILD_DEBUG) */
DM8168DUCATIPWR_ModuleObject DM8168DUCATIPWR_state =
{
    .isSetup = FALSE,
    .configSize = sizeof (DM8168DUCATIPWR_Config),
    .refCount   = 0,
    .pwrstateRefCount = 0,
    .defCfg.reserved = 0,
    .gateHandle = NULL,
};

/* =============================================================================
 * APIs directly called by applications
 * =============================================================================
 */
/*!
 *  @brief      Function to get the default configuration for the DM8168DUCATIPWR
 *              module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to DM8168DUCATIPWR_setup filled in by the
 *              DM8168DUCATIPWR module with the default parameters. If the user does
 *              not wish to make any change in the default parameters, this API
 *              is not required to be called.
 *
 *  @param      cfg        Pointer to the DM8168DUCATIPWR module configuration
 *                         structure in which the default config is to be
 *                         returned.
 *
 *  @sa         DM8168DUCATIPWR_setup
 */
Void
DM8168DUCATIPWR_getConfig (DM8168DUCATIPWR_Config * cfg)
{
    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_getConfig", cfg);

    GT_assert (curTrace, (cfg != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (cfg == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_getConfig",
                             PWRMGR_E_INVALIDARG,
                             "Argument of type (DM8168DUCATIPWR_Config *) passed "
                             "is null!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

        if (DM8168DUCATIPWR_state.refCount == 0) {
            memcpy (cfg,
                         &DM8168DUCATIPWR_state.defCfg,
                         sizeof (DM8168DUCATIPWR_Config));
        }
        else {
            memcpy (cfg,
                         &DM8168DUCATIPWR_state.cfg,
                         sizeof (DM8168DUCATIPWR_Config));
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_0trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_getConfig");
}

/*!
 *  @brief      Function to setup the DM8168DUCATIPWR module.
 *
 *              This function sets up the DM8168DUCATIPWR module. This function must
 *              be called before any other instance-level APIs can be invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then DM8168DUCATIPWR_getConfig can be called to get the
 *              configuration filled with the default values. After this, only
 *              the required configuration values can be changed. If the user
 *              does not wish to make any change in the default parameters, the
 *              application can simply call DM8168DUCATIPWR_setup with NULL
 *              parameters. The default parameters would get automatically used.
 *
 *  @param      cfg   Optional DM8168DUCATIPWR module configuration. If provided as
 *                    NULL, default configuration is used.
 *
 *  @sa         DM8168DUCATIPWR_destroy
 *              GateMutex_create
 */
Int
DM8168DUCATIPWR_setup (DM8168DUCATIPWR_Config * cfg)
{
    Int                      status = PWRMGR_SUCCESS;
    DM8168DUCATIPWR_Config   tmpCfg;
    IArg                     key;
    Error_Block              eb;

#if defined(SYSLINK_BUILD_RTOS)
    Error_init(&eb);
#endif /* #if defined(SYSLINK_BUILD_RTOS) */
#if defined(SYSLINK_BUILD_HLOS)
    eb = 0;
#endif /* #if defined(SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_setup", cfg);
    if (cfg == NULL) {
        DM8168DUCATIPWR_getConfig (&tmpCfg);
        cfg = &tmpCfg;
    }

    /* This sets the refCount variable is not initialized, upper 16 bits is
     * written with module Id to ensure correctness of refCount variable.
     */
    key = Gate_enterSystem();
    DM8168DUCATIPWR_state.refCount++;
    if (DM8168DUCATIPWR_state.refCount > 1) {
        status = DM8168DUCATIPWR_S_ALREADYSETUP;
        Gate_leaveSystem(key);
    }
    else {
        Gate_leaveSystem(key);

        /* Create a default gate handle for local module protection. */
        DM8168DUCATIPWR_state.gateHandle = (IGateProvider_Handle)
                                GateMutex_create ((GateMutex_Params*)NULL, &eb);
        if (DM8168DUCATIPWR_state.gateHandle == NULL) {
            key = Gate_enterSystem();
            DM8168DUCATIPWR_state.refCount = 0;
            Gate_leaveSystem(key);
            /*! @retval PWRMGR_E_FAIL Failed to create GateMutex! */
            status = PWRMGR_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DUCATIPWR_setup",
                                 status,
                                 "Failed to create GateMutex!");
        }
        else {
            /* Copy the user provided values into the state object. */
            memcpy (&DM8168DUCATIPWR_state.cfg,
                         cfg,
                         sizeof (DM8168DUCATIPWR_Config));

            /* Initialize the name to handles mapping array. */
            memset (&DM8168DUCATIPWR_state.pwrHandles,
                        0,
                        (sizeof (DM8168DUCATIPWR_Handle) * MultiProc_MAXPROCESSORS));
            DM8168DUCATIPWR_state.isSetup = TRUE;
        }
    }

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_setup", status);
    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

/*!
 *  @brief      Function to destroy the DM8168DUCATIPWR module.
 *
 *              Once this function is called, other DM8168DUCATIPWR module APIs, except
 *              for the DM8168DUCATIPWR_getConfig API cannot be called anymore.
 *
 *  @sa         DM8168DUCATIPWR_setup
 *              GateMutex_delete
 */
Int
DM8168DUCATIPWR_destroy (Void)
{
    Int    status = PWRMGR_SUCCESS;
    UInt16 i;
    IArg key;

    GT_0trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_destroy");

    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        status = DM8168DUCATIPWR_E_INVALIDSTATE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_destroy",
                             status,
                             "Module was not initialized!");
    }
    else {
#endif /* !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        key = Gate_enterSystem();
        DM8168DUCATIPWR_state.refCount--;
        Gate_leaveSystem(key);

        if (DM8168DUCATIPWR_state.refCount == 0) {
            /* Temporarily increment refCount here. */
            key = Gate_enterSystem();
            DM8168DUCATIPWR_state.refCount = 1;
            Gate_leaveSystem(key);

            /* Check if any DM8168DUCATIPWR instances have not been deleted so far. If not,
             * delete them.
             */
            for (i = 0 ; i < MultiProc_MAXPROCESSORS ; i++) {
                GT_assert (curTrace, (DM8168DUCATIPWR_state.pwrHandles [i] == NULL));
                if (DM8168DUCATIPWR_state.pwrHandles [i] != NULL) {
                    DM8168DUCATIPWR_delete (&(DM8168DUCATIPWR_state.pwrHandles [i]));
                }
            }
            /* restore refCount here. */
            key = Gate_enterSystem();
            DM8168DUCATIPWR_state.refCount = 0;
            Gate_leaveSystem(key);

            if (DM8168DUCATIPWR_state.gateHandle != NULL) {
                GateMutex_delete ((GateMutex_Handle *)
                                        &(DM8168DUCATIPWR_state.gateHandle));
            }
            DM8168DUCATIPWR_state.isSetup = FALSE;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_destroy", status);

    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

/*!
 *  @brief      Function to initialize the parameters for this PwrMgr instance.
 *
 *  @param      params  Configuration parameters.
 *
 *  @sa         DM8168DUCATIPWR_create
 */
Void
DM8168DUCATIPWR_Params_init (DM8168DUCATIPWR_Params * params)
{
    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_Params_init", params);

    GT_assert (curTrace, (params != NULL));
    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_Params_initv",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_Params_init",
                             PWRMGR_E_INVALIDARG,
                             "Argument of type (DM8168DUCATIPWR_Params *) "
                             "passed is null!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        params->reserved = 0;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_0trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_Params_init");
}

/*!
 *  @brief      Function to create an instance of this PwrMgr.
 *
 *  @param      procId  Processor ID addressed by this PwrMgr instance.
 *  @param      params  Configuration parameters.
 *
 *  @sa         DM8168DUCATIPWR_delete
 */
DM8168DUCATIPWR_Handle
DM8168DUCATIPWR_create (      UInt16               procId,
                    const DM8168DUCATIPWR_Params * params)
{
    Int                  status = PWRMGR_SUCCESS;
    PwrMgr_Object *      handle = NULL;
    IArg                 key;

    GT_2trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_create", procId, params);

    GT_assert (curTrace, IS_VALID_PROCID (procId));
    GT_assert (curTrace, (params != NULL));
    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_create",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (!IS_VALID_PROCID (procId)) {
        /* Not setting status here since this function does not return status.*/
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_create",
                             PWRMGR_E_INVALIDARG,
                             "Invalid procId specified");
    }
    else if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_create",
                             PWRMGR_E_INVALIDARG,
                             "params passed is null!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Enter critical section protection. */
        key = IGateProvider_enter (DM8168DUCATIPWR_state.gateHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        /* Check if the PwrMgr already exists for specified procId. */
        if (DM8168DUCATIPWR_state.pwrHandles [procId] != NULL) {
            status = PWRMGR_E_ALREADYEXIST;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DUCATIPWR_create",
                                 status,
                                 "PwrMgr already exists for specified procId!");
        }
        else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
            /* Allocate memory for the handle */
            handle = (PwrMgr_Object *) Memory_calloc (NULL,
                                                      sizeof (PwrMgr_Object),
                                                      0,
                                                      NULL);
            if (handle == NULL) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DUCATIPWR_create",
                                     PWRMGR_E_MEMORY,
                                     "Memory allocation failed for handle!");
            }
            else {
                /* Populate the handle fields */
                handle->pwrFxnTable.attach = &DM8168DUCATIPWR_attach;
                handle->pwrFxnTable.detach = &DM8168DUCATIPWR_detach;
                handle->pwrFxnTable.on     = &DM8168DUCATIPWR_on;
                handle->pwrFxnTable.off    = &DM8168DUCATIPWR_off;
                /* TBD: Other functions */

                /* Allocate memory for the DM8168DUCATIPWR handle */
                handle->object = Memory_calloc (NULL,
                                                sizeof (DM8168DUCATIPWR_Object),
                                                0,
                                                NULL);
                if (handle == NULL) {
                    status = PWRMGR_E_MEMORY;
                    GT_setFailureReason (curTrace,
                                        GT_4CLASS,
                                        "DM8168DUCATIPWR_create",
                                        status,
                                        "Memory allocation failed for handle!");
                }
                else {
#if defined (SYSLINK_BUILDOS_LINUX)
                    ((DM8168DUCATIPWR_Object *)handle->object)->clockHandle
                                       = (ClockOps_Handle) LinuxClock_create();
#endif/* #if defined (SYSLINK_BUILDOS_LINUX) */
#if defined (SYSLINK_BUILD_RTOS)
                    ((DM8168DUCATIPWR_Object *)handle->object)->clockHandle
                                       = (ClockOps_Handle) DM8168CLOCK_create();
#endif/* #if defined (SYSLINK_BUILD_RTOS) */
                    handle->procId = procId;
                    DM8168DUCATIPWR_state.pwrHandles [procId] =
                                                (DM8168DUCATIPWR_Handle) handle;
                }
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Leave critical section protection. */
        IGateProvider_leave (DM8168DUCATIPWR_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    if (status < 0) {
        if (handle !=  NULL) {
            if (handle->object != NULL) {
                Memory_free (NULL, handle->object, sizeof (DM8168DUCATIPWR_Object));
            }
            Memory_free (NULL, handle, sizeof (PwrMgr_Object));
        }
        /*! @retval NULL Function failed */
        handle = NULL;
    }

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_create", handle);

    /*! @retval Valid-Handle Operation successful */
    return (DM8168DUCATIPWR_Handle) handle;
}

/*!
 *  @brief      Function to delete an instance of this PwrMgr.
 *
 *              The user provided pointer to the handle is reset after
 *              successful completion of this function.
 *
 *  @param      handlePtr  Pointer to Handle to the PwrMgr instance
 *
 *  @sa         DM8168DUCATIPWR_create
 */
Int
DM8168DUCATIPWR_delete (DM8168DUCATIPWR_Handle * handlePtr)
{
    Int                  status = PWRMGR_SUCCESS;
    DM8168DUCATIPWR_Object * object = NULL;
    PwrMgr_Object *      handle;
    IArg                 key;

    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_delete", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_delete",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (handlePtr == NULL) {
        /*! @retval PWRMGR_E_INVALIDARG Invalid NULL handlePtr specified*/
        status = PWRMGR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_delete",
                             status,
                             "Invalid NULL handlePtr specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PWRMGR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PWRMGR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_delete",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        handle = (PwrMgr_Object *) (*handlePtr);
        /* Enter critical section protection. */
        key = IGateProvider_enter (DM8168DUCATIPWR_state.gateHandle);

        /* Reset handle in PwrMgr handle array. */
        GT_assert (curTrace, IS_VALID_PROCID (handle->procId));
        DM8168DUCATIPWR_state.pwrHandles [handle->procId] = NULL;

        object = (DM8168DUCATIPWR_Object *) handle->object;
        /* Free memory used for the DM8168DUCATIPWR object. */
        if (handle->object != NULL) {
            /* Free memory used for the clock handle */
#if defined (SYSLINK_BUILDOS_LINUX)
            LinuxClock_delete(((DM8168DUCATIPWR_Object *)handle->object)->clockHandle);
#endif /* #if defined (SYSLINK_BUILDOS_LINUX) */
#if defined (SYSLINK_BUILD_RTOS)
            DM8168CLOCK_delete(((DM8168DUCATIPWR_Object *)handle->object)->clockHandle);
#endif /* #if defined (SYSLINK_BUILD_RTOS) */
            Memory_free (NULL,
                         object,
                         sizeof (DM8168DUCATIPWR_Object));
            handle->object = NULL;
        }

        /* Free memory used for the PwrMgr object. */
        Memory_free (NULL, handle, sizeof (PwrMgr_Object));
        *handlePtr = NULL;

        /* Leave critical section protection. */
        IGateProvider_leave (DM8168DUCATIPWR_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_delete", status);

    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

/*!
 *  @brief      Function to open a handle to an instance of this PwrMgr. This
 *              function is called when access to the PwrMgr is required from
 *              a different process.
 *
 *  @param      handlePtr   Handle to the PwrMgr instance
 *  @param      procId      Processor ID addressed by this PwrMgr instance.
 *
 *  @sa         DM8168DUCATIPWR_close
 */
Int
DM8168DUCATIPWR_open (DM8168DUCATIPWR_Handle * handlePtr, UInt16 procId)
{
    Int status = PWRMGR_SUCCESS;

    GT_2trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_open", handlePtr, procId);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, IS_VALID_PROCID (procId));

    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_open",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (handlePtr == NULL) {
        /*! @retval PWRMGR_E_HANDLE Invalid NULL handlePtr specified */
        status = PWRMGR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_open",
                             status,
                             "Invalid NULL handlePtr specified");
    }
    else if (!IS_VALID_PROCID (procId)) {
        /*! @retval PWRMGR_E_INVALIDARG Invalid procId specified */
        status = PWRMGR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_open",
                             status,
                             "Invalid procId specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Initialize return parameter handle. */
        *handlePtr = NULL;

        /* Check if the PwrMgr exists and return the handle if found. */
        if (DM8168DUCATIPWR_state.pwrHandles [procId] == NULL) {
            /*! @retval PWRMGR_E_NOTFOUND Specified instance not found */
            status = PWRMGR_E_NOTFOUND;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168DUCATIPWR_open",
                              status,
                              "Specified DM8168DUCATIPWR instance does not exist!");
        }
        else {
            *handlePtr = DM8168DUCATIPWR_state.pwrHandles [procId];
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_open", status);

    /*! @retval PWRMGR_SUCCESS Operation successful */
    return status;
}

/*!
 *  @brief      Function to close a handle to an instance of this PwrMgr.
 *
 *  @param      handlePtr  Pointer to Handle to the PwrMgr instance
 *
 *  @sa         DM8168DUCATIPWR_open
 */
Int
DM8168DUCATIPWR_close (DM8168DUCATIPWR_Handle * handlePtr)
{
    Int status = PWRMGR_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_close", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));
    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_close",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (handlePtr == NULL) {
        /*! @retval PWRMGR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PWRMGR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_close",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PWRMGR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PWRMGR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_close",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
        /* Nothing to be done for close. */
        *handlePtr = NULL;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_close", status);

    /*! @retval PWRMGR_SUCCESS Operation successful */
    return status;
}

/* =============================================================================
 * APIs called by PwrMgr module (part of function table interface)
 * =============================================================================
 */
/*!
 *  @brief      Function to attach to the PwrMgr.
 *
 *  @param      handle  Handle to the PwrMgr instance
 *  @param      params  Attach parameters
 *
 *  @sa         DM8168DUCATIPWR_detach
 */
Int
DM8168DUCATIPWR_attach (PwrMgr_Handle handle, PwrMgr_AttachParams * params)
{

    Int status                            = PWRMGR_SUCCESS;
    PwrMgr_Object *          pwrMgrHandle = (PwrMgr_Object *) handle;
    DM8168DUCATIPWR_Object * object       = NULL;
    Memory_MapInfo           mapInfo;
    /* Mapping for prcm base is done in DM8168VIDEOM3_phyShmemInit */

    GT_2trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_attach", handle, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));
    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_attach",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (handle == NULL) {
        /*! @retval PWRMGR_E_HANDLE Invalid argument */
        status = PWRMGR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_attach",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
        object = (DM8168DUCATIPWR_Object *) pwrMgrHandle->object;
        GT_assert (curTrace, (object != NULL));
        /* Map and get the virtual address for system control module */
        mapInfo.src      = DM8168M3_PRCM_BASE_ADDR;
        mapInfo.size     = DM8168M3_PRCM_SIZE;
        mapInfo.isCached = FALSE;
        status = Memory_map (&mapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        if (status < 0) {
            status = DM8168DUCATIPWR_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DUCATIPWR_attach",
                                 status,
                                 "Failure in mapping prcm module");
        }
        else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
            object->prcmVA = mapInfo.dst;
            /* Map and get the virtual address for system control module */
            mapInfo.src      = DUCATI_MMU_CFG;
            mapInfo.size     = DUCATI_MMU_CFG_SIZE;
            mapInfo.isCached = FALSE;
            status = Memory_map (&mapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                status = DM8168DUCATIPWR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DUCATIPWR_attach",
                                     status,
                                     "Failure in mapping ducatimmu module");
            }
            else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
                object->ducatiMmuVA = mapInfo.dst;
                /* Map and get the virtual address for system control module */
                mapInfo.src      = DUCATI_BASE_ADDR;
                mapInfo.size     = DUCATI_BASE_ADDR_SIZE;
                mapInfo.isCached = FALSE;
                status = Memory_map (&mapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
                if (status < 0) {
                    status = DM8168DUCATIPWR_E_FAIL;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "DM8168DUCATIPWR_attach",
                                         status,
                                         "Failure in mapping ducatibase module");
                }
                else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
                    object->ducatibaseVA = mapInfo.dst;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
                }
            }
        }
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_attach", status);
    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

/*!
 *  @brief      Function to detach from the PwrMgr.
 *
 *  @param      handle  Handle to the PwrMgr instance
 *
 *  @sa         DM8168DUCATIPWR_attach
 */
Int
DM8168DUCATIPWR_detach (PwrMgr_Handle handle)
{
    Int status                     = PWRMGR_SUCCESS;
    PwrMgr_Object * pwrMgrHandle   = (PwrMgr_Object *) handle;
    DM8168DUCATIPWR_Object * object = NULL;
    Memory_UnmapInfo     unmapInfo;


    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_detach", handle);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_detach",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (handle == NULL) {
        /*! @retval PWRMGR_E_HANDLE Invalid argument */
        status = PWRMGR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_detach",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
        object = (DM8168DUCATIPWR_Object *) pwrMgrHandle->object;
        GT_assert (curTrace, (object != NULL));

        /* Unmap the virtual address for prcm module */
        unmapInfo.addr = object->prcmVA;
        unmapInfo.size = DM8168M3_PRCM_SIZE;
        unmapInfo.isCached = FALSE;
        if (unmapInfo.addr != 0) {
            status = Memory_unmap (&unmapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                status = DM8168DUCATIPWR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DUCATIPWR_detach",
                                     status,
                                     "Failure in mapping prcm module");
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
        }
        /* Unmap the virtual address for mmu base*/
        unmapInfo.addr = object->ducatiMmuVA;
        unmapInfo.size = DUCATI_MMU_CFG_SIZE;
        unmapInfo.isCached = FALSE;
        if (unmapInfo.addr != 0) {
            status = Memory_unmap (&unmapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                status = DM8168DUCATIPWR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DUCATIPWR_detach",
                                     status,
                                     "Failure in mapping ducatiMmu module");
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
        }

        /* Unmap the virtual address for ducati control base */
        unmapInfo.addr = object->ducatibaseVA;
        unmapInfo.size = DUCATI_BASE_ADDR_SIZE;
        unmapInfo.isCached = FALSE;
        if (unmapInfo.addr != 0) {
            status = Memory_unmap (&unmapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                status = DM8168DUCATIPWR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DUCATIPWR_detach",
                                     status,
                                     "Failure in mapping ducatiMmu module");
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_detach", status);
    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

/*!
 *  @brief      Function to power on the slave processor.
 *
 *              Power on the IVA subsystem, hold the DSP and SEQ in reset, and
 *              release IVA2_RST. This is a hostile reset of the IVA. If the IVA
 *              is already powered on, then it must be powered off in order to
 *              terminate all current activity and initiate a power-on-reset
 *              transition to bring the IVA to a know good state.
 *
 *  @param      handle    Handle to the PwrMgr instance
 *
 *  @sa         DM8168DUCATIPWR_off
 */
Int
DM8168DUCATIPWR_on (PwrMgr_Handle handle)
{
    Int                  status       = PWRMGR_SUCCESS ;
    Int32                tmpstatus    = 0;
    PwrMgr_Object *      pwrMgrHandle = (PwrMgr_Object *) handle;
    DM8168DUCATIPWR_Object * object   = NULL;
    IArg                     key;
    (Void)tmpstatus;
    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_on", handle);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_on",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* This sets the refCount variable is not initialized, upper 16 bits is
         * written with module Id to ensure correctness of refCount variable.
         */
        key = Gate_enterSystem();
        DM8168DUCATIPWR_state.pwrstateRefCount++;
        if (DM8168DUCATIPWR_state.pwrstateRefCount > 1) {
            status = DM8168DUCATIPWR_S_ALREADYSETUP;
            Gate_leaveSystem(key);
        }
        else {
            Gate_leaveSystem(key);
#if !defined(SYSLINK_BUILD_OPTIMIZE)&& defined(SYSLINK_BUILD_HLOS)
            if (handle == NULL) {
                /*! @retval PWRMGR_E_HANDLE Invalid argument */
                status = PWRMGR_E_HANDLE;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DUCATIPWR_on",
                                     status,
                                     "Invalid handle specified");
            }
            else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
                 object = (DM8168DUCATIPWR_Object *) pwrMgrHandle->object;

                 GT_assert (curTrace, (object != NULL));
                 /* Enable spinlocks, mailbox and timers before powering on ducati */

                 DM8168DUCATIPWR_state.ducatiSpinlockHandle
                            = ClockOps_get(object->clockHandle, "spinbox_ick");
                     /* Do not put this check under SYSLINK_BUILD_OPTIMIZE */
                 GT_assert (curTrace,
                               (DM8168DUCATIPWR_state.ducatiSpinlockHandle != NULL));
                 status = ClockOps_enable(object->clockHandle,
                                         DM8168DUCATIPWR_state.ducatiSpinlockHandle);
                 GT_assert (curTrace, (status >= 0));

                 DM8168DUCATIPWR_state.ducatiMailboxHandle
                            = ClockOps_get(object->clockHandle, "mailbox_ick");
                             /* Do not put this check under SYSLINK_BUILD_OPTIMIZE */
                 GT_assert (curTrace, (DM8168DUCATIPWR_state.ducatiMailboxHandle != NULL));
                 status = ClockOps_enable(object->clockHandle,
                                          DM8168DUCATIPWR_state.ducatiMailboxHandle);
                 GT_assert (curTrace, (status >= 0));

                 /* poer on ducati */
                 DM8168DUCATIPWR_state.ducatiClkHandle
                             = ClockOps_get(object->clockHandle, "ducati_ick");
                 /* Do not put this check under SYSLINK_BUILD_OPTIMIZE */
                 if (DM8168DUCATIPWR_state.ducatiClkHandle == NULL) {
                         /*! @retval PWRMGR_E_HANDLE Invalid argument */
                         status = PWRMGR_E_HANDLE;
                         GT_setFailureReason (curTrace,
                                              GT_4CLASS,
                                              "DM8168DUCATIPWR_on",
                                              status,
                                          "object->clkHandle retuned NULL clk_get failed for ducati");
                 }
                 else {
                     tmpstatus = ClockOps_enable(object->clockHandle,
                                              DM8168DUCATIPWR_state.ducatiClkHandle);
                     GT_assert (curTrace, (tmpstatus >= 0));
                     /* Complete the remaining power sequence here*/
                     DM8168DUCATIMMU_enable(pwrMgrHandle);
                 }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS)
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS)*/
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)*/
    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_on", status);
    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

/*!
 *  @brief      Function to power off the slave processor.
 *
 *              Turn the IVA power domain off. To ensure a clean power-off
 *              transition, the IVA subsystem must first be turned on so that
 *              the DSP can initiate an autonomous power-off transition.
 *
 *  @param      handle    Handle to the PwrMgr instance
 *  @param      force     Indicates whether power-off is to be forced
 *
 *  @sa         DM8168DUCATIPWR_on
 */
Int
DM8168DUCATIPWR_off (PwrMgr_Handle handle, Bool force)
{
    Int                  status       = PWRMGR_SUCCESS ;
    PwrMgr_Object *      pwrMgrHandle = (PwrMgr_Object *) handle;
    DM8168DUCATIPWR_Object * object   = NULL;
    IArg                     key;

    GT_1trace (curTrace, GT_ENTER, "DM8168DUCATIPWR_off", handle);

    GT_assert (curTrace, (handle != NULL));

    GT_assert (curTrace, (DM8168DUCATIPWR_state.refCount != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (DM8168DUCATIPWR_state.refCount == 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_off",
                             DM8168DUCATIPWR_E_INVALIDSTATE,
                             "Module was not initialized!");
    }
    else if (handle == NULL) {
        /*! @retval PWRMGR_E_HANDLE Invalid argument */
        status = PWRMGR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DUCATIPWR_off",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
                 object = (DM8168DUCATIPWR_Object *) pwrMgrHandle->object;
        key = Gate_enterSystem();
        DM8168DUCATIPWR_state.pwrstateRefCount--;
        Gate_leaveSystem(key);

        if (DM8168DUCATIPWR_state.pwrstateRefCount == 0) {
            DM8168DUCATIMMU_disable(pwrMgrHandle);
            /* Disable Mailbox clocks */
            if(DM8168DUCATIPWR_state.ducatiMailboxHandle) {
                ClockOps_disable(object->clockHandle,
                                     DM8168DUCATIPWR_state.ducatiMailboxHandle);
                ClockOps_put(object->clockHandle,
                                     DM8168DUCATIPWR_state.ducatiMailboxHandle);
            }
            /* Disable Spinlock clocks */
            if(DM8168DUCATIPWR_state.ducatiSpinlockHandle) {
                ClockOps_disable(object->clockHandle,
                                    DM8168DUCATIPWR_state.ducatiSpinlockHandle);
                ClockOps_put(object->clockHandle,
                                    DM8168DUCATIPWR_state.ducatiSpinlockHandle);
            }
            if(DM8168DUCATIPWR_state.ducatiClkHandle) {
                ClockOps_disable(object->clockHandle,
                                         DM8168DUCATIPWR_state.ducatiClkHandle);
                ClockOps_put(object->clockHandle,
                                         DM8168DUCATIPWR_state.ducatiClkHandle);
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined(SYSLINK_BUILD_HLOS) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168DUCATIPWR_off", status);
    /*! @retval PWRMGR_SUCCESS Operation successful */
    return (status);
}

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
