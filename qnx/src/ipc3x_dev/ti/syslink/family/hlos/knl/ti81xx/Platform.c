/*
 *  @file   Platform.c
 *
 *  @brief      Implementation of Platform initialization logic.
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



/* Standard header files */
#include <ti/syslink/Std.h>

/* Utilities & Osal headers */
#include <ti/syslink/utils/Gate.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/Trace.h>
#include <ti/ipc/MultiProc.h>
#include <ti/syslink/utils/OsalPrint.h>
#include <ti/syslink/inc/knl/OsalThread.h>
#include <ti/syslink/utils/String.h>
#include <ti/syslink/utils/Cfg.h>

/* SysLink device specific headers */
#include <Dm8168IpcInt.h>
#include <Dm8168DspPwr.h>
#include <Dm8168DspProc.h>
#include <Dm8168M3VideoProc.h>
#include <Dm8168M3DssProc.h>
#include <Dm8168DucatiPwr.h>

/* Module level headers */
#include <_MessageQCopy.h>
#include <ti/ipc/MessageQCopy.h>
#include <_MessageQCopyDefs.h>

#include <ti/syslink/ProcMgr.h>
#include <_ProcMgr.h>
#include <ti/syslink/inc/knl/Platform.h>
#if defined(SYSLINK_LOADER_COFF)
#include <CoffLoader.h>
#endif
#include <ElfLoader.h>

#include <ti/ipc/Ipc.h>
#include <_Ipc.h>
#include <IpcKnl.h>
#include <GateHWSpinlock.h>

#if defined (__cplusplus)
extern "C" {
#endif

/* this macro is only used on Linux builds */
#ifndef EXPORT_SYMBOL
#define EXPORT_SYMBOL(s)
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

#define IPC_DSP_MEM_VRING0      0x9EF00000
#define IPC_DSP_MEM_VRING1      0x9EF04000

/* This tag is used as an identifier by Ipc_readConfig
 * to get different modules' configuration on slave
 */
#define SLAVE_CONFIG_TAG            0xDADA0000

#define HWSPINLOCK_BASE             0x480CA000
#define HWSPINLOCK_SIZE             0x1000
#define HWSPINLOCK_OFFSET           0x800


/* Defines used for timeout value for start/stopCallback */
#define TIMEOUT_LOOPCNT             1000
#define TIMEOUT_SLEEPTIME           10
#define ATTACH_LOOPCNT              5000000
#define ATTACH_SLEEPTIME            1

/** ============================================================================
 *  Application specific configuration, please change these value according to
 *  your application's need.
 *  ============================================================================
 */

 /* This structure is used to get different modules' configuration on host
 * to match it with that of Slaves'
 */
typedef struct Platform_ModuleConfig {
    UInt16 sharedRegionNumEntries;
}Platform_ModuleConfig;

/*!
 *  @brief  Structure defining config parameters for overall System.
 */
typedef struct Platform_Config {
    MultiProc_Config                multiProcConfig;
    /*!< Multiproc config parameter */

    MessageQCopy_Config             MQCopyConfig;
    /*!< MessageQCopy config parameter */


    ProcMgr_Config                  procMgrConfig;
    /*!< Processor manager config parameter */

#if defined(SYSLINK_LOADER_COFF)
    CoffLoader_Config               coffLoaderConfig;
    /*!< Coff loader config parameter */
#endif

    ElfLoader_Config                elfLoaderConfig;
    /*!< Elf loader config parameter */

    GateHWSpinlock_Config           gateHWSpinlockConfig;
    /*!< GateHWSpinlock config parameter */
} Platform_Config;


/* Struct embedded into slave binary */
typedef struct Platform_SlaveConfig {
    UInt32  cacheLineSize;
    UInt32  brOffset;
} Platform_SlaveConfig;

/* Shared region configuration */
typedef struct Platform_SlaveSRConfig {
    UInt32 entryBase;
    UInt32 entryLen;
    UInt32 ownerProcId;
    UInt32 id;
    UInt32 createHeap;
    UInt32 cacheLineSize;
} Platform_SlaveSRConfig;

/*! @brief structure for platform instance */
typedef struct Platform_Object {
    /*!< Flag to indicate platform initialization status */
    ProcMgr_Handle                pmHandle;
    /*!< Handle to the ProcMgr instance used */
    union {
        struct {
            DM8168DSPPROC_Handle pHandle;
            /*!< Handle to the Processor instance used */
            DM8168DSPPWR_Handle  pwrHandle;
            /*!< Handle to the PwrMgr instance used */
            ElfLoader_Handle    ldrHandle;
            /*!< Handle to the Loader instance used */
        } dsp;
        struct {
            DM8168VIDEOM3PROC_Handle pHandle;
            /*!< Handle to the Processor instance used */
            DM8168DUCATIPWR_Handle  pwrHandle;
            /*!< Handle to the PwrMgr instance used */
            ElfLoader_Handle        ldrHandle;
            /*!< Handle to the Loader instance used */
        } m3video;
        struct {
            DM8168VPSSM3PROC_Handle pHandle;
            /*!< Handle to the Processor instance used */
            DM8168DUCATIPWR_Handle  pwrHandle;
            /*!< Handle to the PwrMgr instance used */
            ElfLoader_Handle        ldrHandle;
            /*!< Handle to the Loader instance used */
        } m3vpss;
    } sHandles;
    /*!< Slave specific handles */
    Platform_SlaveConfig          slaveCfg;
    /*!< Slave embedded config */
    Platform_SlaveSRConfig *      slaveSRCfg;
    /*!< Shared region details from slave */

    UInt8               *bslaveAdditionalReg;
    /*!< To keep track of  additional shared regions configured in the slave */
} Platform_Object, *Platform_Handle;


/*! @brief structure for platform instance */
typedef struct Platform_Module_State {
    Bool              multiProcInitFlag;
    /*!< MultiProc Initialize flag */
    Bool              messageQCopyInitFlag;
    /*!< MessageQCopy Initialize flag */
    Bool              procMgrInitFlag;
    /*!< Processor manager Initialize flag */
#if defined(SYSLINK_LOADER_COFF)
    Bool              coffLoaderInitFlag;
    /*!< Coff loader Initialize flag */
#endif
    Bool              elfLoaderInitFlag;
    /*!< Elf loader Initialize flag */
    Bool              ipcIntInitFlag;
    /*!< IpcInt Initialize flag */
    Bool              platformInitFlag;
    /*!< Flag to indicate platform initialization status */
    Platform_ModuleConfig hostModuleConfig;
    /*!< Configuration of various Modules' parameters on host */
    Bool              gateHWSpinlockInitFlag;
    /*!< GateHWSpinlock Initialize flag */
    Ptr               gateHWSpinlockVAddr;
    /*!< GateHWSpinlock Virtual Address */
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

extern String ProcMgr_sysLinkCfgParams;

String Syslink_Override_Params = "ProcMgr.proc[DSP].mmuEnable=FALSE;"
                                 "ProcMgr.proc[DSP].carveoutAddr=0x9FB00000;"
                                 "ProcMgr.proc[DSP].carveoutSize=0x500000;"
                                 "ProcMgr.proc[DSP].carveoutAddr1=0x9DB00000;"
                                 "ProcMgr.proc[DSP].carveoutSize1=0x1000000;"
                                 "ProcMgr.proc[DSP].carveoutAddr2=0x9EB00000;"
                                 "ProcMgr.proc[DSP].carveoutSize2=0x1000000;";

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
 *  @brief      Function to get tyhe default values for confiurations.
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
        ElfLoader_getConfig (&config->elfLoaderConfig);

        /* Get the HWSpinlock default config */
        GateHWSpinlock_getConfig (&config->gateHWSpinlockConfig);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "Platform_getConfig");
}

/*!
 *  @brief      Function to override the default configuration values.
 *
 *  @param      config   Configuration values.
 */
Int32
Platform_overrideConfig (Platform_Config * config, Ipc_Config * cfg)
{
    Int32  status = Platform_S_SUCCESS;
    char *pSL_PARAMS;

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

        /* assign config->params - overriding with SL_PARAMS env var, if set */
        pSL_PARAMS = getenv("SL_PARAMS");
        if (pSL_PARAMS != NULL) {
            GT_2trace(curTrace, GT_1CLASS, "Overriding SysLink_params \"%s\""
                    " with SL_PARAMS \"%s\"\n", Syslink_Override_Params, pSL_PARAMS);
            cfg->params = Memory_alloc(NULL, String_len(pSL_PARAMS), 0, NULL);
            if (cfg->params) {
                String_cpy(cfg->params, pSL_PARAMS);
            }
        }
        else {
            cfg->params = Memory_alloc(NULL,
                                       String_len(Syslink_Override_Params) + 1, 0,
                                       NULL);
            if (cfg->params) {
                String_cpy(cfg->params, Syslink_Override_Params);
            }
        }

        _ProcMgr_saveParams(cfg->params, String_len(cfg->params));

#if !defined (SYSLINK_VARIANT_TI813X)
        /* Override the gatepeterson default config */
        config->multiProcConfig.numProcessors = 4;
        config->multiProcConfig.id            = 0;
        String_cpy (config->multiProcConfig.nameList [0],
                    "HOST");
        String_cpy (config->multiProcConfig.nameList [1],
                    "VPSS-M3");
        String_cpy (config->multiProcConfig.nameList [2],
                    "VIDEO-M3");
        String_cpy (config->multiProcConfig.nameList [3],
                    "DSP");
#else
        config->multiProcConfig.numProcessors = 3;
        config->multiProcConfig.id            = 0;
        String_cpy (config->multiProcConfig.nameList [0],
                    "HOST");
        String_cpy (config->multiProcConfig.nameList [1],
                    "VPSS-M3");
        String_cpy (config->multiProcConfig.nameList [2],
                    "VIDEO-M3");
#endif  /* #if !defined (SYSLINK_VARIANT_TI813X) */

        /* Override the MESSAGEQCOPY default config */
        config->MQCopyConfig.intId[1] = 77;
        config->MQCopyConfig.intId[2] = 77;

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
    Int32             status = Platform_S_SUCCESS;
    Platform_Config   _config;
    Platform_Config * config;
    Dm8168IpcInt_Config dm8168cfg;
    Memory_MapInfo    minfo;

    Platform_getConfig (&_config);
    config = &_config;

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
        /* Do the IPC interrupts setup for the full platform (cfg is not used) */
        Dm8168IpcInt_setup (&dm8168cfg);
        Platform_module->ipcIntInitFlag = TRUE;
    }


/* Intialize MESSAGEQCOPY */
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
            Platform_module->messageQCopyInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

/* Intialize Elf loader */
    if (status >= 0) {
        status = ElfLoader_setup (&config->elfLoaderConfig);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_setup",
                                 status,
                                 "ElfLoader_setup failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->elfLoaderInitFlag = TRUE;
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
    if (status >= 0) {
        minfo.src  = HWSPINLOCK_BASE;
        minfo.size = HWSPINLOCK_SIZE;
        minfo.isCached = FALSE;
        status = Memory_map (&minfo);
        if (status < 0) {
           GT_setFailureReason (curTrace,
                                GT_4CLASS,
                                "Platform_setup",
                                status,
                                "Memory_map failed!");
        }
        else {
            Platform_module->gateHWSpinlockVAddr = (Ptr)minfo.dst;
            config->gateHWSpinlockConfig.numLocks = 128;
            config->gateHWSpinlockConfig.baseAddr = minfo.dst  + HWSPINLOCK_OFFSET;
            status = GateHWSpinlock_setup (&config->gateHWSpinlockConfig);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "Platform_setup",
                                     status,
                                     "GateHWSpinlock_setup failed!");
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                Platform_module->gateHWSpinlockInitFlag = TRUE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
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
    Memory_UnmapInfo minfo;

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

    /* Finalize elf loader */
    if (Platform_module->elfLoaderInitFlag == TRUE) {
        status = ElfLoader_destroy ();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "ElfLoader_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->elfLoaderInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Finalize MESSAGEQCOPY */
    if (Platform_module->messageQCopyInitFlag == TRUE) {
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
            Platform_module->messageQCopyInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }


    if (Platform_module->ipcIntInitFlag == TRUE) {
        Dm8168IpcInt_destroy ();
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

    if (Platform_module->gateHWSpinlockInitFlag == TRUE) {
        status = GateHWSpinlock_destroy();
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "GateHWSpinlock_destroy failed!");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Platform_module->gateHWSpinlockInitFlag = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    if (Platform_module->gateHWSpinlockVAddr) {
        minfo.addr = (UInt32)Platform_module->gateHWSpinlockVAddr;
        minfo.size = HWSPINLOCK_SIZE;
        minfo.isCached = FALSE;
        status = Memory_unmap(&minfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Platform_destroy",
                                 status,
                                 "Memory_unmap failed!");
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        Platform_module->gateHWSpinlockVAddr = NULL;
    }


    GT_1trace (curTrace, GT_LEAVE, "Platform_destroy", status);

    /*! @retval Platform_S_SUCCESS Operation successful */
    return status;
}

/*
 * union _Platform_setup_Local exists so that we don't waste stack or
 * alloc'ed memory on storage for things that exist for just a few
 * statements of the function _Platform_setup().  The *PROC_Params
 * elements are large and variably sized, depending on the macro
 * ProcMgr_MAX_MEMORY_REGIONS.
 */
typedef union _Platform_setup_Local {
    ProcMgr_Params          params;
    DM8168DSPPROC_Config     dspProcConfig;
    DM8168VIDEOM3PROC_Config videoProcConfig;
    DM8168VPSSM3PROC_Config   vpssProcConfig;
    DM8168DSPPWR_Config      dspPwrConfig;
    DM8168DUCATIPWR_Config  videoPwrConfig;
    DM8168DUCATIPWR_Config    vpssPwrConfig;
    DM8168DSPPROC_Params     dspProcParams;
    DM8168VIDEOM3PROC_Params videoProcParams;
    DM8168VPSSM3PROC_Params   vpssProcParams;
    DM8168DSPPWR_Params      dspPwrParams;
    DM8168DUCATIPWR_Params  videoPwrParams;
    DM8168DUCATIPWR_Params    vpssPwrParams;
    ElfLoader_Params        elfLoaderParams;
#if defined(SYSLINK_LOADER_COFF)
    CoffLoader_Params       coffLoaderParams;
#endif
} _Platform_setup_Local;


/*!
 *  @brief      Function to setup platform.
 *              TBD: logic would change completely in the final system.
 */
Int32
_Platform_setup (Ipc_Config * cfg)
{
    Int32                   status = Platform_S_SUCCESS;
    _Platform_setup_Local   *lv;
    UInt16                  procId;
    Platform_Handle         handle;

    GT_0trace (curTrace, GT_ENTER, "_Platform_setup");

    lv = Memory_alloc(NULL, sizeof(_Platform_setup_Local), 0, NULL);
    if (lv == NULL) {
        status = Platform_E_FAIL;
        GT_setFailureReason (curTrace,
                                GT_4CLASS,
                                "_Platform_setup",
                                status,
                                "Memory_alloc failed");
        goto ret;
    }

    /* Get MultiProc ID by name. */
    procId = MultiProc_getId ("VIDEO-M3");

    handle = &Platform_objects [procId];
    DM8168VIDEOM3PROC_getConfig (&lv->videoProcConfig);
    status = DM8168VIDEOM3PROC_setup (&lv->videoProcConfig);
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_setup",
                             status,
                             "DM8168VIDEOM3PROC_setup failed!");
    }
    else {
        DM8168DUCATIPWR_getConfig (&lv->videoPwrConfig);
        status = DM8168DUCATIPWR_setup (&lv->videoPwrConfig);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "DM8168DUCATIPWR_setup failed!");
        }
    }

    if (status >= 0) {
        /* Create an instance of the Processor object for
         * DM8168VIDEOM3 */
        DM8168VIDEOM3PROC_Params_init (NULL, &lv->videoProcParams);
        handle->sHandles.m3video.pHandle = DM8168VIDEOM3PROC_create (
                                                    procId,
                                                    &lv->videoProcParams);

        /* Create an instance of the ELF Loader object */
        ElfLoader_Params_init (NULL, &lv->elfLoaderParams);
        handle->sHandles.m3video.ldrHandle = ElfLoader_create (procId,
                                                    &lv->elfLoaderParams);

        /* Create an instance of the PwrMgr object for DM8168VIDEOM3 */
        DM8168DUCATIPWR_Params_init (&lv->videoPwrParams);
        handle->sHandles.m3video.pwrHandle = DM8168DUCATIPWR_create (
                                                     procId,
                                                     &lv->videoPwrParams);

        if (handle->sHandles.m3video.pHandle == NULL) {
            status = Platform_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "DM8168VIDEOM3PROC_create failed!");
        }
        else if (handle->sHandles.m3video.ldrHandle ==  NULL) {
            status = Platform_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "Failed to create loader instance!");
        }
        else if (handle->sHandles.m3video.pwrHandle ==  NULL) {
            status = Platform_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "DM8168DUCATIPWR_create failed!");
        }
        else {
            /* Initialize parameters */
            ProcMgr_Params_init (NULL, &lv->params);
            lv->params.procHandle = handle->sHandles.m3video.pHandle;
            lv->params.loaderHandle = handle->sHandles.m3video.ldrHandle;
            lv->params.pwrHandle = handle->sHandles.m3video.pwrHandle;
            String_cpy (lv->params.rstVectorSectionName,
                        RESETVECTOR_SYMBOL);
            handle->pmHandle = ProcMgr_create (procId, &lv->params);
            if (handle->pmHandle == NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "ProcMgr_create failed!");
            }
        }
    }

    if (status >= 0) {
        /* Get MultiProc ID by name. */
        procId = MultiProc_getId ("VPSS-M3");

        handle = &Platform_objects [procId];
        DM8168VPSSM3PROC_getConfig (&lv->vpssProcConfig);
        status = DM8168VPSSM3PROC_setup (&lv->vpssProcConfig);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "DM8168VPSSM3PROC_setup failed!");
        }
        else {
            DM8168DUCATIPWR_getConfig (&lv->vpssPwrConfig);
            status = DM8168DUCATIPWR_setup (&lv->vpssPwrConfig);
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "DM8168DUCATIPWR_setup failed!");
            }
        }

        if (status >= 0) {
            /* Create an instance of the Processor object for
             * DM8168VPSSM3 */
            DM8168VPSSM3PROC_Params_init (NULL, &lv->vpssProcParams);
            handle->sHandles.m3vpss.pHandle = DM8168VPSSM3PROC_create (procId,
                                                         &lv->vpssProcParams);

            /* Create an instance of the ELF Loader object */
            ElfLoader_Params_init (NULL, &lv->elfLoaderParams);
            handle->sHandles.m3vpss.ldrHandle = ElfLoader_create (procId,
                                                        &lv->elfLoaderParams);

            /* Create an instance of the PwrMgr object for DM8168VPSSM3 */
            DM8168DUCATIPWR_Params_init (&lv->vpssPwrParams);
            handle->sHandles.m3vpss.pwrHandle = DM8168DUCATIPWR_create (
                                                           procId,
                                                           &lv->vpssPwrParams);

            if (handle->sHandles.m3vpss.pHandle == NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "DM8168VPSSM3PROC_create failed!");
            }
            else if (handle->sHandles.m3vpss.ldrHandle ==  NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "Failed to create loader instance!");
            }
            else if (handle->sHandles.m3vpss.pwrHandle ==  NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "DM8168DUCATIPWR_create failed!");
            }
            else {
                /* Initialize parameters */
                ProcMgr_Params_init (NULL, &lv->params);
                lv->params.procHandle = handle->sHandles.m3vpss.pHandle;
                lv->params.loaderHandle = handle->sHandles.m3vpss.ldrHandle;
                lv->params.pwrHandle = handle->sHandles.m3vpss.pwrHandle;
                String_cpy (lv->params.rstVectorSectionName,
                            RESETVECTOR_SYMBOL);
                handle->pmHandle = ProcMgr_create (procId, &lv->params);
                if (handle->pmHandle == NULL) {
                    status = Platform_E_FAIL;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "_Platform_setup",
                                         status,
                                         "ProcMgr_create failed!");
                }
            }
        }
    }

#if !defined (SYSLINK_VARIANT_TI813X)
    if (status >= 0) {
        /* Get MultiProc ID by name. */
        procId = MultiProc_getId ("DSP");

        handle = &Platform_objects [procId];
        DM8168DSPPROC_getConfig (&lv->dspProcConfig);
        status = DM8168DSPPROC_setup (&lv->dspProcConfig);
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_setup",
                                 status,
                                 "DM8168DSPPROC_setup failed!");
        }
        else {
            DM8168DSPPWR_getConfig (&lv->dspPwrConfig);
            status = DM8168DSPPWR_setup (&lv->dspPwrConfig);
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "DM8168DSPPWR_setup failed!");
            }
        }

        if (status >= 0) {
            /* Create an instance of the Processor object for
             * DM8168DSP */
            DM8168DSPPROC_Params_init (NULL, &lv->dspProcParams);
            handle->sHandles.dsp.pHandle = DM8168DSPPROC_create (procId,
                                                              &lv->dspProcParams);

            /* Create an instance of the ELF Loader object */
            ElfLoader_Params_init (NULL, &lv->elfLoaderParams);
            handle->sHandles.dsp.ldrHandle =
                                           ElfLoader_create (procId,
                                                             &lv->elfLoaderParams);
            /* Create an instance of the PwrMgr object for DM8168DSP */
            DM8168DSPPWR_Params_init (&lv->dspPwrParams);
            handle->sHandles.dsp.pwrHandle = DM8168DSPPWR_create (procId,
                                                    &lv->dspPwrParams);

            if (handle->sHandles.dsp.pHandle == NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "DM8168DSPPROC_create failed!");
            }
            else if (handle->sHandles.dsp.ldrHandle ==  NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "Failed to create loader instance!");
            }
            else if (handle->sHandles.dsp.pwrHandle ==  NULL) {
                status = Platform_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_Platform_setup",
                                     status,
                                     "DM8168DSPPWR_create failed!");
            }
            else {
                /* Initialize parameters */
                ProcMgr_Params_init (NULL, &lv->params);
                lv->params.procHandle = handle->sHandles.dsp.pHandle;
                lv->params.loaderHandle = handle->sHandles.dsp.ldrHandle;
                lv->params.pwrHandle = handle->sHandles.dsp.pwrHandle;
                String_cpy (lv->params.rstVectorSectionName,
                            RESETVECTOR_SYMBOL);
                handle->pmHandle = ProcMgr_create (procId, &lv->params);
                if (handle->pmHandle == NULL) {
                    status = Platform_E_FAIL;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "_Platform_setup",
                                         status,
                                         "ProcMgr_create failed!");
                }
            }
        }
    }
#endif /* #if !defined (SYSLINK_VARIANT_TI813X) */

    Memory_free(NULL, lv, sizeof(_Platform_setup_Local));

ret:
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

    GT_0trace (curTrace, GT_ENTER, "_Platform_destroy");

#if !defined (SYSLINK_VARIANT_TI813X)
    /* ------------------------- DSP cleanup -------------------------------- */
    handle = &Platform_objects [MultiProc_getId ("DSP")];
    if (handle->pmHandle != NULL) {
        status = ProcMgr_delete (&handle->pmHandle);
        GT_assert (curTrace, (status >= 0));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "ProcMgr_delete failed!");
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* Delete the Processor, Loader and PwrMgr instances */
    if (handle->sHandles.dsp.pwrHandle != NULL) {
        tmpStatus = DM8168DSPPWR_delete (&handle->sHandles.dsp.pwrHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "DM8168DSPPWR_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    if (handle->sHandles.dsp.ldrHandle != NULL) {
        tmpStatus = ElfLoader_delete (&handle->sHandles.dsp.ldrHandle);
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

    if (handle->sHandles.dsp.pHandle != NULL) {
        tmpStatus = DM8168DSPPROC_delete (&handle->sHandles.dsp.pHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "DM8168DSPPROC_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    tmpStatus = DM8168DSPPWR_destroy ();
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "DM8168DSPPWR_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    tmpStatus = DM8168DSPPROC_destroy ();
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "DM8168DSPPROC_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }
#endif /* #if !defined (SYSLINK_VARIANT_TI813X) */
    /* ------------------------- VIDEOM3 cleanup ------------------------------ */
    handle = &Platform_objects [MultiProc_getId ("VIDEO-M3")];
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
    if (handle->sHandles.m3video.pwrHandle != NULL) {
        tmpStatus = DM8168DUCATIPWR_delete (&handle->sHandles.m3video.pwrHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "DM8168DUCATIPWR_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    if (handle->sHandles.m3video.ldrHandle != NULL) {
        tmpStatus = ElfLoader_delete (&handle->sHandles.m3video.ldrHandle);
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

    if (handle->sHandles.m3video.pHandle != NULL) {
        tmpStatus = DM8168VIDEOM3PROC_delete (&handle->sHandles.m3video.pHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "DM8168VIDEOM3PROC_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    tmpStatus = DM8168DUCATIPWR_destroy ();
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "DM8168DUCATIPWR_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    tmpStatus = DM8168VIDEOM3PROC_destroy ();
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "DM8168VIDEOM3PROC_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    /* ------------------------- VPSSM3 cleanup -------------------------------- */
    handle = &Platform_objects [MultiProc_getId ("VPSS-M3")];
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
    if (handle->sHandles.m3vpss.pwrHandle != NULL) {
        tmpStatus = DM8168DUCATIPWR_delete (&handle->sHandles.m3vpss.pwrHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "DM8168DUCATIPWR_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    if (handle->sHandles.m3vpss.ldrHandle != NULL) {
        tmpStatus = ElfLoader_delete (&handle->sHandles.m3vpss.ldrHandle);
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

    if (handle->sHandles.m3vpss.pHandle != NULL) {
        tmpStatus = DM8168VPSSM3PROC_delete (&handle->sHandles.m3vpss.pHandle);
        GT_assert (curTrace, (tmpStatus >= 0));
        if ((status >= 0) && (tmpStatus < 0)) {
            status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_Platform_destroy",
                                 status,
                                 "DM8168VPSSM3PROC_delete failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    tmpStatus = DM8168DUCATIPWR_destroy ();
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "DM8168DUCATIPWR_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    tmpStatus = DM8168VPSSM3PROC_destroy ();
    GT_assert (curTrace, (tmpStatus >= 0));
    if ((status >= 0) && (tmpStatus < 0)) {
        status = tmpStatus;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_Platform_destroy",
                             status,
                             "DM8168VPSSM3PROC_destroy failed!");
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_1trace (curTrace, GT_LEAVE, "_Platform_destroy", status);

    /*! @retval Platform_S_SUCCESS operation was successful */
    return status;
}


#if defined (__cplusplus)
}
#endif
