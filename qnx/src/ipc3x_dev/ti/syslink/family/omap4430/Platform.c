/*
 *  @file   Platform.c
 *
 *  @brief      Implementation of Platform initialization logic.
 *
 *
 *  @ver        02.00.00.46_alpha1
 *
 *  ============================================================================
 *
 *  Copyright (c) 2011, Texas Instruments Incorporated
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


/* Standard header files */
#include <ti/syslink/Std.h>

/* Utilities & Osal headers */
#include <ti/syslink/utils/Gate.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/Trace.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>
#include <ti/syslink/utils/OsalPrint.h>
#include <ti/syslink/utils/String.h>

/* SysLink device specific headers */
#include <ProcDefs.h>
#include <Processor.h>
#include <OMAP4430DucatiHal.h>
#include <OMAP4430DucatiHalReset.h>
#include <OMAP4430DucatiHalMmu.h>
#include <OMAP4430DucatiProc.h>
#include <Omap4430IpcInt.h>

/* Module level headers */
#include <_MessageQCopy.h>
#include <ti/ipc/MessageQCopy.h>
#include <_MessageQCopyDefs.h>

#include <ti/syslink/ProcMgr.h>
#include <_ProcMgr.h>
#include <ti/syslink/inc/knl/Platform.h>
#include <rprcloader.h>

#include <ti/ipc/Ipc.h>
#include <_Ipc.h>
#include <IpcKnl.h>
#include <ipu_pm.h>

#if defined (__cplusplus)
extern "C" {
#endif


/** ============================================================================
 *  Macros.
 *  ============================================================================
 */
#define RESETVECTOR_SYMBOL          "_Ipc_ResetVector"

#define IPC_MEM_VRING0          0xA0000000
#define IPC_MEM_VRING1          0xA0004000
#define IPC_MEM_VRING2          0xA0008000
#define IPC_MEM_VRING3          0xA000c000

/** ============================================================================
 *  Application specific configuration, please change these value according to
 *  your application's need.
 *  ============================================================================
 */
/*!
 *  @brief  Structure defining config parameters for overall System.
 */
typedef struct Platform_Config {
    MultiProc_Config                multiProcConfig;
    /*!< Multiproc config parameter */

    MessageQCopy_Config             MQCopyConfig;
    /*!< Notify config parameter */

    ProcMgr_Config                  procMgrConfig;
    /*!< Processor manager config parameter */

    ipu_pm_config                   ipu_pm_config;
    /* ipu_pm config parameter */

    rprcloader_Config               rprcloaderConfig;
    /*!< Elf loader config parameter */
} Platform_Config;


/* Struct embedded into slave binary */
typedef struct Platform_SlaveConfig {
    UInt32  cacheLineSize;
    UInt32  brOffset;
} Platform_SlaveConfig;

typedef struct Platform_SlaveSRConfig {
    UInt32 entryBase;
    UInt32 entryLen;
    UInt32 ownerProcId;
    UInt32 id;
    UInt32 createHeap;
    UInt32 cacheLineSize;
} Platform_SlaveSRConfig;

/* Shared region configuration information for host side. */
typedef struct Platform_HostSRConfig {
    UInt16 refCount;
} Platform_HostSRConfig;

/*! @brief structure for platform instance */
typedef struct Platform_Object {
    /*!< Flag to indicate platform initialization status */
    ProcMgr_Handle                pmHandle;
    /*!< Handle to the ProcMgr instance used */
    union{
        struct {
            OMAP4430DUCATIPROC_Handle    pHandle;
            /*!< Handle to the Processor instance used */
            /*!< Handle to the PwrMgr instance used */
            rprcloader_Handle            ldrHandle;
            /*!< Handle to the Loader instance used */
            UInt32                       fileId;
            /*!< File ID of loaded image, needed for un-loading */
        } sysm3;
        struct {
            OMAP4430DUCATIPROC_Handle    pHandle;
            /*!< Handle to the Processor instance used */
            /*!< Handle to the PwrMgr instance used */
            /* NOTE: no loader used for appm3, both cores loaded at once */
            /*!< Handle to the Loader instance used */
        } appm3;
    } sHandles;
    /*!< Slave specific handles */
    Platform_SlaveConfig          slaveCfg;
    /*!< Slave embedded config */
    Platform_SlaveSRConfig *      slaveSRCfg;
    /*!< Shared region details from slave */
} Platform_Object, *Platform_Handle;


/*! @brief structure for platform instance */
typedef struct Platform_Module_State {
    Bool              multiProcInitFlag;
    /*!< MultiProc Initialize flag */
    Bool              ipu_pm_init_flag;
    /*!< ipu_pm Initialize flag */
    Bool              procMgrInitFlag;
    /*!< Processor manager Initialize flag */
    Bool              rprcloaderInitFlag;
    /*!< rprc loader Initialize flag */
    Bool              ipcIntInitFlag;
    /*!< IpcInt Initialize flag */
    Bool              platformInitFlag;
    /*!< Flag to indicate platform initialization status */
    Bool              mqcopyInitFlag;
    /*!< rprc loader Initialize flag */
    Bool              platform_mem_init_flag;
} Platform_Module_State;


/* =============================================================================
 * GLOBALS
 * =============================================================================
 */
static Platform_Object Platform_objects [MultiProc_MAXPROCESSORS];
static Platform_Module_State Platform_Module_state;
static Platform_Module_State * Platform_module = &Platform_Module_state;

Int32 _Platform_setup  (Ipc_Config * cfg);
Int32 _Platform_destroy (void);

/** ============================================================================
 *  APIs.
 *  ============================================================================
 */
/* Function to read slave memory */
Int32
_Platform_readSlaveMemory (UInt16   procId,
                           UInt32   addr,
                           Ptr      value,
                           UInt32 * numBytes);

/* Function to write slave memory */
Int32
_Platform_writeSlaveMemory (UInt16   procId,
                            UInt32   addr,
                            Ptr      value,
                            UInt32 * numBytes);
/*!
 *  @brief      Function to get the default values for configurations.
 *
 *  @param      config   Configuration values.
 */
Void
Platform_getConfig (Platform_Config * config)
{
    GT_1trace (curTrace, GT_ENTER, "Platform_getConfig", config);

    GT_assert (curTrace, (config != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (config == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Platform_getConfig",
                             Platform_E_INVALIDARG,
                             "Argument of type (Platform_getConfig *) passed "
                             "is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        /* Get the gatepeterson default config */
        MultiProc_getConfig (&config->multiProcConfig);

        /* Get the PROCMGR default config */
        ProcMgr_getConfig (&config->procMgrConfig);

        /* Get the ElfLoader default config */
        rprcloader_getConfig (&config->rprcloaderConfig);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "Platform_getConfig");
}

/*!
 *  @brief      Function to override the default confiuration values.
 *
 *  @param      config   Configuration values.
 */
Int32
Platform_overrideConfig (Platform_Config * config, Ipc_Config * cfg)
{
    Int32  status = Platform_S_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "Platform_overrideConfig", config);

    GT_assert (curTrace, (config != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (config == NULL) {
        /*! @retval Platform_E_INVALIDARG Argument of type
         *  (Platform_Config *) passed is null*/
        status = Platform_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Platform_overrideConfig",
                             status,
                             "Argument of type (Platform_getConfig *) passed "
                             "is null!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        /* Override the gatepeterson default config */
        config->multiProcConfig.numProcessors = 4;
        config->multiProcConfig.id            = 0;

        String_cpy (config->multiProcConfig.nameList [0],
                    "HOST");
        String_cpy (config->multiProcConfig.nameList [1],
                    "CORE0");
        String_cpy (config->multiProcConfig.nameList [2],
                    "CORE1");
        String_cpy (config->multiProcConfig.nameList [3],
                    "DSP");

        /* Override the PROCMGR default config */

        /* Override the MessageQCopy default config */
        config->MQCopyConfig.intId = 58;

        config->ipu_pm_config.int_id = 58;
        config->ipu_pm_config.num_procs = 2;
        config->ipu_pm_config.proc_ids[0] = 2; // CORE0 is set as 2 above
        config->ipu_pm_config.proc_ids[1] = 1; // CORE1 is set as 1 above

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_ENTER, "Platform_overrideConfig", status);

    /*! @retval Platform_S_SUCCESS operation was successful */
    return status;
}

/*!
 *  @brief      Function to setup platform.
 *              TBD: logic would change completely in the final system.
 */
Int32
Platform_setup (Ipc_Config * cfg)
{
    Int32                   status      = Platform_S_SUCCESS;
    Platform_Config         _config;
    Platform_Config *       config;
    Omap4430IpcInt_Config   omap4430cfg;

    Platform_getConfig (&_config);
    config = &_config;

/* Initialize PlatformMem */
    status = MemoryOS_setup();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Platform_setup",
                             status,
                             "platform_mem_setup!");
    } else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        Platform_module->platform_mem_init_flag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    Platform_overrideConfig (config, cfg);

    status = MultiProc_setup (&(config->multiProcConfig));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Platform_setup",
                             status,
                             "MultiProc_setup failed!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->multiProcInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

/* Initialize PROCMGR */
    if (status >= 0) {
        status = ProcMgr_setup (&(config->procMgrConfig));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_setup",
                                 status,
                                 "ProcMgr_setup failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->procMgrInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Initialize IpcInt required for VirtQueue/MessageQCopy. */
    if (status >= 0) {
        /* Do the IPC interrupt setup for the full platform (cfg is not used) */
        Omap4430IpcInt_setup(&omap4430cfg);
        Platform_module->ipcIntInitFlag = TRUE;
    }

/* Initialize rprc loader */
    if (status >= 0) {
        status = rprcloader_setup (&config->rprcloaderConfig);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_setup",
                                 status,
                                 "rprcloader_setup failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->rprcloaderInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

/* Initialize ipu_pm */
    if (status >= 0) {
        status = ipu_pm_setup (&config->ipu_pm_config);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_setup",
                                 status,
                                 "ipu_pm_setup failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->ipu_pm_init_flag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

/* Initialize MessageQCopy */
    if (status >= 0) {
        status = MessageQCopy_setup (&config->MQCopyConfig);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_setup",
                                 status,
                                 "MessageQCopy_setup failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->mqcopyInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    if (status >= 0) {
        Memory_set (Platform_objects,
                    0,
                    (sizeof (Platform_Object) * MultiProc_getNumProcessors()));
    }

    if (status >= 0) {
        /* Doing per remote-proc init stuff */
        status = _Platform_setup (cfg);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_setup",
                                 status,
                                 "_Platform_setup failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->platformInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    if (status < 0) {
        Platform_destroy();
    }

    return status;
}


/*!
 *  @brief      Function to destroy the System.
 *
 *  @sa         Platform_setup
 */
Int32
Platform_destroy (void)
{
    Int32  status = Platform_S_SUCCESS;

    GT_0trace (curTrace, GT_ENTER, "Platform_destroy");

    /* Finalize Platform-specific destroy */
    if (Platform_module->platformInitFlag == TRUE) {
        status = _Platform_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "Platform_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->platformInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Finalize MessageQCopy */
    if (Platform_module->mqcopyInitFlag == TRUE) {
        status = MessageQCopy_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "MessageQCopy_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->mqcopyInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Finalize ipu_pm */
    if (Platform_module->ipu_pm_init_flag == TRUE) {
        status = ipu_pm_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "ipu_pm_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->ipu_pm_init_flag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Finalize rprc loader */
    if (Platform_module->rprcloaderInitFlag == TRUE) {
        status = rprcloader_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "rprcloader_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->rprcloaderInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    if (Platform_module->ipcIntInitFlag == TRUE) {
        Omap4430IpcInt_destroy ();
        Platform_module->ipcIntInitFlag = FALSE;
    }

    /* Finalize PROCMGR */
    if (Platform_module->procMgrInitFlag == TRUE) {
        status = ProcMgr_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "ProcMgr_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->procMgrInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Finalize MultiProc */
    if (Platform_module->multiProcInitFlag == TRUE) {
        status = MultiProc_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "MultiProc_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->multiProcInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    if (status >= 0) {
        Memory_set (Platform_objects,
                    0,
                    (sizeof (Platform_Object) * MultiProc_getNumProcessors()));
    }

    if (Platform_module->platform_mem_init_flag == TRUE) {
        status = MemoryOS_destroy();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "MemoryOS_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->platform_mem_init_flag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_1trace (curTrace, GT_LEAVE, "Platform_destroy", status);

    /*! @retval Platform_S_SUCCESS Operation successful */
    return status;
}

/*!
 *  @brief      Function to setup platform.
 *              TBD: logic would change completely in the final system.
 */
Int32
_Platform_setup (Ipc_Config * cfg)
{
    Int32                       status              = Platform_S_SUCCESS;
    ProcMgr_Params              params;
    OMAP4430DUCATIPROC_Config   sysM3ProcConfig;
    OMAP4430DUCATIPROC_Config   appM3ProcConfig;
    OMAP4430DUCATIPROC_Params   sysM3ProcParams;
    OMAP4430DUCATIPROC_Params   appM3ProcParams;
    ProcMgr_AddrInfo *          memEntries;
    rprcloader_Params           rprcloaderParams;
    rprcloader_Handle           ldrHandle;
    UInt16                      procId;
    Platform_Handle             handle;
    UInt32                      pa, va;
    UInt32                      i                   = 0;
    Bool                        core0Setup          = FALSE;
    Bool                        core1Setup          = FALSE;
    ProcMgr_AttachParams        attachParams;

    GT_0trace (curTrace, GT_ENTER, "_Platform_setup");

    /* Get MultiProc ID by name. */
    procId = MultiProc_getId ("CORE0");

    handle = &Platform_objects [procId];

    OMAP4430DUCATIPROC_get_config(&sysM3ProcConfig, procId );
    status = OMAP4430DUCATIPROC_setup (&sysM3ProcConfig, procId);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_setup",
                             status,
                             "OMAP4430PROC_setup failed!");
    }
    else {
        core0Setup = TRUE;
        /* Create an instance of the Processor object for
         * OMAP4430 */
        OMAP4430DUCATIPROC_Params_init (NULL, &sysM3ProcParams, procId);
        pa = cfg->pAddr;
        va = cfg->vAddr;
        memEntries = sysM3ProcParams.memEntries;
        for (i = 0; i < sysM3ProcParams.numMemEntries; i++) {
            memEntries[i].addr[ProcMgr_AddrType_MasterPhys] = pa;
            memEntries[i].addr[ProcMgr_AddrType_MasterKnlVirt] = va;
            pa += memEntries[i].size;
            va += memEntries[i].size;
        }
        handle->sHandles.sysm3.pHandle = OMAP4430DUCATIPROC_create (
                                                      procId,
                                                      &sysM3ProcParams);

        /* Create an instance of the rprc Loader object */
        rprcloader_Params_init (NULL, &rprcloaderParams);
        handle->sHandles.sysm3.ldrHandle = rprcloader_create (procId,
                                                          &rprcloaderParams);

        if (handle->sHandles.sysm3.pHandle == NULL) {
            status = Platform_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "OMAP4430PROC_create failed!");
        }
        else if (handle->sHandles.sysm3.ldrHandle ==  NULL) {
            status = Platform_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "Failed to create loader instance!");
        }
        else {
            /* Initialize parameters */
            ProcMgr_Params_init (NULL, &params);
            params.procHandle = handle->sHandles.sysm3.pHandle;
            params.loaderHandle = handle->sHandles.sysm3.ldrHandle;
            ldrHandle = params.loaderHandle;
            String_cpy (params.rstVectorSectionName,
                        RESETVECTOR_SYMBOL);
            handle->pmHandle = ProcMgr_create (procId, &params);
            if (handle->pmHandle == NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "ProcMgr_create failed!");
            }
            else {
                ProcMgr_getAttachParams(handle->pmHandle, &attachParams);
                ProcMgr_attach(handle->pmHandle, &attachParams);
                MessageQCopy_attach(procId, (Ptr)IPC_MEM_VRING0, 0);
            }
        }
    }

    if (status >= 0) {
        /* Get MultiProc ID by name. */
        procId = MultiProc_getId ("CORE1");

        handle = &Platform_objects [procId];
        OMAP4430DUCATIPROC_get_config (&appM3ProcConfig, procId );
        status = OMAP4430DUCATIPROC_setup (&appM3ProcConfig, procId);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "OMAP4430PROC_setup failed!");
        }
        else {
            core1Setup = TRUE;
            /* Create an instance of the Processor object for
             * OMAP4430 */
            OMAP4430DUCATIPROC_Params_init(NULL, &appM3ProcParams,procId);
            pa = cfg->pAddr;
            va = cfg->vAddr;
            memEntries = appM3ProcParams.memEntries;
            for (i = 0; i < appM3ProcParams.numMemEntries; i++) {
                memEntries[i].addr[ProcMgr_AddrType_MasterPhys] = pa;
                memEntries[i].addr[ProcMgr_AddrType_MasterKnlVirt] = va;
                pa += memEntries[i].size;
                va += memEntries[i].size;
            }
            handle->sHandles.appm3.pHandle = OMAP4430DUCATIPROC_create(procId,
                                                          &appM3ProcParams);

            /* Don't create an instance of the rprc Loader - not needed */

            if (handle->sHandles.appm3.pHandle == NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "OMAP4430PROC_create failed!");
            }
            else {
                /* Initialize parameters */
                ProcMgr_Params_init (NULL, &params);
                params.procHandle = handle->sHandles.appm3.pHandle;
                /* Use the same loader handle as sysm3 to avoid ProcMgr errors */
                params.loaderHandle = ldrHandle;
                String_cpy (params.rstVectorSectionName,
                            RESETVECTOR_SYMBOL);
                handle->pmHandle = ProcMgr_create (procId, &params);
                if (handle->pmHandle == NULL) {
                    status = Platform_E_FAIL;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "_Platform_setup",
                                         status,
                                         "ProcMgr_create failed!");
                }
                else {
                    ProcMgr_getAttachParams(handle->pmHandle, &attachParams);
                    ProcMgr_attach(handle->pmHandle, &attachParams);
                    MessageQCopy_attach(procId, (Ptr)IPC_MEM_VRING2, 200);
                }
            }
        }
    }

    if (status < 0) {
        /* Cleanup in case of error */
        procId = MultiProc_getId ("CORE1");
        handle = &Platform_objects [procId];
        if (handle->pmHandle) {
            ProcMgr_delete(&handle->pmHandle);
            handle->pmHandle = NULL;
        }
        if (handle->sHandles.appm3.pHandle) {
            OMAP4430DUCATIPROC_delete(&handle->sHandles.appm3.pHandle);
            handle->sHandles.appm3.pHandle = NULL;
        }
        if (core1Setup)
            OMAP4430DUCATIPROC_destroy(procId);

        procId = MultiProc_getId ("CORE0");
        handle = &Platform_objects [procId];
        if (handle->pmHandle) {
            ProcMgr_delete(&handle->pmHandle);
            handle->pmHandle = NULL;
        }
        if (handle->sHandles.sysm3.ldrHandle) {
            rprcloader_delete(&handle->sHandles.sysm3.ldrHandle);
            handle->sHandles.sysm3.ldrHandle = NULL;
        }
        if (handle->sHandles.sysm3.pHandle) {
            OMAP4430DUCATIPROC_delete(&handle->sHandles.sysm3.pHandle);
            handle->sHandles.sysm3.pHandle = NULL;
        }
        if (core0Setup)
            OMAP4430DUCATIPROC_destroy(procId);
    }
    GT_1trace (curTrace, GT_LEAVE, "_Platform_setup", status);

    /*! @retval Platform_S_SUCCESS operation was successful */
    return status;
}


/*!
 *  @brief      Function to setup platform.
 *              TBD: logic would change completely in the final system.
 */
Int32
_Platform_destroy (void)
{
    Int32           status    = Platform_S_SUCCESS;
    Int32           tmpStatus = Platform_S_SUCCESS;
    Platform_Handle handle;
    /*UInt16          procId;*/

    /* Get MultiProc ID by name. */

    GT_0trace (curTrace, GT_ENTER, "_Platform_destroy");

    /* ------------------------- APPM3 cleanup ------------------------------ */
    handle = &Platform_objects [MultiProc_getId ("CORE1")];
    if (handle->pmHandle != NULL) {
        tmpStatus = ProcMgr_delete (&handle->pmHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "ProcMgr_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    /* Delete the Processor, Loader and PwrMgr instances */

    if (handle->sHandles.appm3.pHandle != NULL) {
        tmpStatus = OMAP4430DUCATIPROC_delete (&handle->sHandles.appm3.pHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "OMAP4430PROC_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    tmpStatus = OMAP4430DUCATIPROC_destroy (MultiProc_getId ("CORE1"));
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "OMAP4430PROC_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* ------------------------- SYSM3 cleanup ------------------------------ */
    handle = &Platform_objects [MultiProc_getId ("CORE0")];
    if (handle->pmHandle != NULL) {
        tmpStatus = ProcMgr_delete (&handle->pmHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "ProcMgr_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    /* Delete the Processor, Loader and PwrMgr instances */

    if (handle->sHandles.sysm3.ldrHandle != NULL) {
        tmpStatus = rprcloader_delete (&handle->sHandles.sysm3.ldrHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "Failed to delete loader instance!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    if (handle->sHandles.sysm3.pHandle != NULL) {
        tmpStatus = OMAP4430DUCATIPROC_delete (&handle->sHandles.sysm3.pHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "OMAP4430PROC_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    tmpStatus = OMAP4430DUCATIPROC_destroy (MultiProc_getId ("CORE0"));
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "OMAP4430PROC_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_1trace (curTrace, GT_LEAVE, "_Platform_destroy", status);

    /*! @retval Platform_S_SUCCESS operation was successful */
    return status;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
