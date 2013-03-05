/*
 *  @file   Dm8168IpcInt.c
 *
 *  @brief      DM8168 interrupt handling code.
 *              Defines necessary functions for Interrupt Handling.
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



/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL headers */
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/utils/List.h>
#include <Bitops.h>

/* OSAL and utils headers */
#include <OsalIsr.h>
#include <_MultiProc.h>
#include <ti/ipc/MultiProc.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/Gate.h>
#include <ti/syslink/utils/GateMutex.h>

/* Hardware Abstraction Layer */
#include <_ArchIpcInt.h>
#include <_Dm8168IpcInt.h>
#include <Dm8168IpcInt.h>
#include <errno.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */

/*!
 *  @def    DM8168_DM8168_NUMPROCS
 *  @brief  Number of processors supported on this platform
 */
#define DM8168_NUMPROCS 4
/*!
 *  @def    DM8168_DM8168_INDEX_DSP
 *  @brief  Dsp index.
 */
#define DM8168_INDEX_DSP 0
/*!
 *  @def    DM8168_INDEX_VIDEOM3
 *  @brief  M3Video index.
 */
#define DM8168_INDEX_VIDEOM3 1
/*!
 *  @def    DM8168_INDEX_VPSSM3
 *  @brief  M3Dss index.
 */
#define DM8168_INDEX_VPSSM3 2
/*!
 *  @def    DM8168_INDEX_HOST
 *  @brief  HOST index.
 */
#define DM8168_INDEX_HOST 3

/* Macro to make a correct module magic number with refCount */
#define DM8168IPCINT_MAKE_MAGICSTAMP(x) \
                                    ((DM8168IPCINT_MODULEID << 12u) | (x))

/*!
 *  @def    REG
 *  @brief  Regsiter access method.
 */
#define REG(x)          *((volatile UInt32 *) (x))
#define REG32(x)        (*(volatile UInt32 *) (x))

/* Register access method. */
#define REG16(A)        (*(volatile UInt16 *) (A))

/*!
 *  @def    AINTC_BASE_ADDR
 *  @brief  configuraion address.
 */
#define AINTC_BASE_ADDR                 0x48200000

/*!
 *  @def    AINTC_BASE_SIZE
 *  @brief  size to be ioremapped.
 */
#define AINTC_BASE_SIZE                 0x1000

/* Mailbox management values */
/*!
 *  @def    DM8168_MAILBOX_BASE
 *  @brief  configuraion address.
 */
#define MAILBOX_BASE                     0x480C8000

/*!
 *  @def    MAILBOX_SIZE
 *  @brief  size to be ioremapped.
 */
#define MAILBOX_SIZE                     0x1000

/*!
 *  @def    MAILBOX_SYSCONFIG_OFFSET
 *  @brief  Offset from the Mailbox base address.
 */
#define MAILBOX_SYSCONFIG_OFFSET      0x10

/*!
 *  @def    MAILBOX_MAXNUM
 *  @brief  maximum number of mailbox.
 */
#define MAILBOX_MAXNUM                   0x8

/*!
 *  @def    MAILBOX_MESSAGE_m_OFFSET
 *  @brief  mailbox message address Offset from the Mailbox base
 *          address. m = 0 to 7 => offset = 0x40 + 0x4*m
 */
#define MAILBOX_MESSAGE_m_OFFSET(m)        (0x40 + (m<<2))

/*!
 *  @def    MAILBOX_MESSAGE_0_OFFSET
 *  @brief  Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_0_OFFSET         0x40

/*!
 *  @def    MAILBOX_MESSAGE_1_OFFSET
 *  @brief  Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_1_OFFSET        0x44

/*!
 *  @def    MAILBOX_MESSAGE_2_OFFSET
 *  @brief  Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_2_OFFSET         0x48

/*!
 *  @def    MAILBOX_MESSAGE_3_OFFSET
 *  @brief  Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_3_OFFSET         0x4C

/*!
 *  @def    MAILBOX_MESSAGE_4_OFFSET
 *  @brief  mailbox message 4 address Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_4_OFFSET        0x50

/*!
 *  @def    MAILBOX_MESSAGE_5_OFFSET
 *  @brief  mailbox message 5 address Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_5_OFFSET        0x54

/*!
 *  @def    MAILBOX_MESSAGE_6_OFFSET
 *  @brief  mailbox message 6 address Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_6_OFFSET        0x58

/*!
 *  @def    DM8168_MAILBOX_BASE_OFFSET
 *  @brief  mailbox message 7 address Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_7_OFFSET        0x5C

/*!
 *  @def    DM8168_MAILBOX_BASE_OFFSET
 *  @brief  mailbox message 8 address Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_8_OFFSET        0x60

/*!
 *  @def    MAILBOX_MESSAGE_9_OFFSET
 *  @brief  mailbox message 9 address Offset from the Mailbox base address.
 */
#define MAILBOX_MESSAGE_9_OFFSET        0x64
/*!
 *  @def    MAILBOX_MSGSTATUS_m_OFFSET
 *  @brief  mailbox message status address Offset from the Mailbox base
 *          address. m = 0 to 7 => offset = 0x40 + 0x4*m
 */
#define MAILBOX_MSGSTATUS_m_OFFSET(m)        (0xC0 + (m<<2))

/*!
 *  @def    MAILBOX_MSGSTATUS_0_OFFSET
 *  @brief  mailbox message 0 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_0_OFFSET      0xC0

/*!
 *  @def    MAILBOX_MSGSTATUS_1_OFFSET
 *  @brief  mailbox message 1 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_1_OFFSET      0xC4

/*!
 *  @def    MAILBOX_MSGSTATUS_2_OFFSET
 *  @brief  mailbox message 2 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_2_OFFSET      0xC8

/*!
 *  @def    MAILBOX_MSGSTATUS_3_OFFSET
 *  @brief  mailbox message 3 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_3_OFFSET      0xCC

/*!
 *  @def    MAILBOX_MSGSTATUS_4_OFFSET
 *  @brief  mailbox message 4 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_4_OFFSET      0xD0

/*!
 *  @def    MAILBOX_MSGSTATUS_5_OFFSET
 *  @brief  mailbox message 5 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_5_OFFSET      0xD4

/*!
 *  @def    MAILBOX_MSGSTATUS_6_OFFSET
 *  @brief  mailbox message 6 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_6_OFFSET      0xD8

/*!
 *  @def    MAILBOX_MSGSTATUS_7_OFFSET
 *  @brief  mailbox message 7 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_7_OFFSET      0xDC
/*!
 *  @def    MAILBOX_MSGSTATUS_8_OFFSET
 *  @brief  mailbox message 8 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_8_OFFSET      0xE0
/*!
 *  @def    MAILBOX_MSGSTATUS_9_OFFSET
 *  @brief  mailbox message 9 status address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_MSGSTATUS_9_OFFSET      0xE4
/*!
 *  @def    MAILBOX_IRQSTATUS_CLEAR_OFFSET
 *  @brief  mailbox IRQSTATUS clear address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_IRQSTATUS_CLEAR_OFFSET  0x104

/*!
 *  @def    MAILBOX_IRQENABLE_SET_OFFSET
 *  @brief  mailbox IRQ enable set address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_IRQENABLE_SET_OFFSET    0x108
/*!
 *  @def    MAILBOX_IRQENABLE_CLR_OFFSET
 *  @brief  mailbox IRQ enable clear address Offset from the Mailbox base
 *          address.
 */
#define MAILBOX_IRQENABLE_CLR_OFFSET    0x10C


/*!
 *  @def    MAILBOX_NUMBER_0
 *  @brief  mailbox number 0 used by HOST ARM to receive interrupts from DSP.
 */
#define MAILBOX_NUMBER_0                 0
/*!
 *  @def    MAILBOX_NUMBER_1
 *  @brief  mailbox number 1 used by HOST to receive interrupts from CORE0.
 */
#define MAILBOX_NUMBER_1                 1
/*!
 *  @def    MAILBOX_NUMBER_2
 *  @brief  mailbox number 2 used by HOST to receive interrupts from DSP.
 */
#define MAILBOX_NUMBER_2                 2
/*!
 *  @def    MAILBOX_NUMBER_3
 *  @brief  mailbox number 3 used by DSP to receive interrupts from HOST.
 */
#define MAILBOX_NUMBER_3                 3
/*!
 *  @def    MAILBOX_NUMBER_4
 *  @brief  mailbox number 4 used by CORE1 to receive interrupts from CORE0/DSP.
 */
#define MAILBOX_NUMBER_4                 4

/*!
 *  @def    MAILBOX_NUMBER_6
 *  @brief  mailbox number 6 used by HOST ARM to receive interrupts from VIDEOM3
 */
#define MAILBOX_NUMBER_6                 6
/*!
 *  @def    MAILBOX_NUMBER_8
 *  @brief  mailbox number 8 used by HOST ARM to receive interrupts from VPSSM3.
 */
#define MAILBOX_NUMBER_8                 8

/* Macro used when saving the mailbox context */
#define DM8168_MAILBOX_IRQENABLE(u)    (0x108 + 0x10 * (u))

/* Msg elem used to store messages from the remote proc */
typedef struct Dm8168IpcInt_MsgListElem_tag {
    List_Elem elem;
    UInt32 msg;
    struct Dm8168IpcInt_MsgListElem * next;
    struct Dm8168IpcInt_MsgListElem * prev;
} Dm8168IpcInt_MsgListElem;

/*!
 *  @brief  Device specific object
 *          It can be populated as per device need and it is used internally in
 *          the device specific implementation only.
 */
typedef struct Dm8168IpcInt_Object_tag {
    Atomic                 isrRefCount;
    /*!< ISR Reference count */
    Atomic                 asserted;
    /*!< Indicates receipt of interrupt from particular processor */
    UInt32                 recvIntId;
    /*!<recevive interrupt id */
    ArchIpcInt_CallbackFxn fxn;
    /*!< Callbck function to be registered for particular instance of driver*/
    Ptr                    fxnArgs;
    /*!< Argument to the call back function */
} Dm8168IpcInt_Object;


/*!
 *  @brief  Device specific object
 *          It can be populated as per device need and it is used internally in
 *          the device specific implementation only.
 */
typedef struct Dm8168IpcInt_ModuleObject_tag {
    Atomic             isrRefCount;
    /*!< ISR Reference count */
    OsalIsr_Handle     isrHandle;
    /*!< Handle to the OsalIsr object */
    UInt16             procIds [DM8168_NUMPROCS];
    /*!< Processors supported */
    UInt16             maxProcessors;
    /*!< Maximum number of processors supported by this platform*/
    Dm8168IpcInt_Object isrObjects [MultiProc_MAXPROCESSORS];
    /*!< Array of Isr objects */
    List_Handle isrLists [MultiProc_MAXPROCESSORS];
    /*!< Array of Isr lists */
    UInt32         archCoreCmBase;
    /*!< configuration mgmt base */
    UInt32         mailboxBase;
    /*!< mail box configuration mgmt base */
    UInt32         intId;
    /*!< interrupt id for this proc */
} Dm8168IpcInt_ModuleObject;



/* =============================================================================
 * Forward declarations of internal functions.
 * =============================================================================
 */
/* This function implements the interrupt service routine for the interrupt
 * received from the remote processor.
 */
static Bool _Dm8168IpcInt_isr (Ptr ref);

/*!
 *  @brief  Forward declaration of check and clear function
 */
static Bool _Dm8168IpcInt_checkAndClearFunc (Ptr arg);


/* =============================================================================
 *  Globals
 * =============================================================================
 */
/*!
 *  @brief  State object for Dm8168IpcInt
 */
Dm8168IpcInt_ModuleObject Dm8168IpcInt_state;

/*!
 *  @brief  Function table for OMAP3530
 */
ArchIpcInt_FxnTable Dm8168IpcInt_fxnTable = {
    Dm8168IpcInt_interruptRegister,
    Dm8168IpcInt_interruptUnregister,
    Dm8168IpcInt_interruptEnable,
    Dm8168IpcInt_interruptDisable,
    Dm8168IpcInt_waitClearInterrupt,
    Dm8168IpcInt_sendInterrupt,
    Dm8168IpcInt_clearInterrupt,
};

int mailbox_context[MAILBOX_SIZE];

/* =============================================================================
 *  APIs
 * =============================================================================
 */

/*!
 *  @brief      Function to initialize the Dm8168IpcInt module.
 *
 *  @param      cfg  Configuration for setup
 *
 *  @sa         Dm8168IpcInt_destroy
 */
Void
Dm8168IpcInt_setup (Dm8168IpcInt_Config * cfg)
{
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int            status = DM8168IPCINT_SUCCESS;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    Int i = 0;
    Memory_MapInfo mapInfo;
    List_Params listParams;

    GT_1trace (curTrace, GT_ENTER, "Dm8168IpcInt_setup", cfg);

    GT_assert (curTrace, (cfg != NULL));

    /* The setup will be called only once, either from SysMgr or from
     * archipcdm8168 module. Hence it does not need to be atomic.
     */
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (cfg == NULL) {
        GT_setFailureReason (curTrace,
                        GT_4CLASS,
                        "Dm8168IpcInt_setup",
                        DM8168IPCINT_E_FAIL,
                        "config for driver specific setup can not be NULL");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        /* Map general control base */
        mapInfo.src      = AINTC_BASE_ADDR;
        mapInfo.size     = AINTC_BASE_SIZE;
        mapInfo.isCached = FALSE;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        status =
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        Memory_map (&mapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status < 0) {
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Dm8168IpcInt_setup",
                                 status,
                                 "Failure in Memory_map for general ctrl base");
            Dm8168IpcInt_state.archCoreCmBase = 0;
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            Dm8168IpcInt_state.archCoreCmBase = mapInfo.dst;
            /* Map mailboxBase */
            mapInfo.src      = MAILBOX_BASE;
            mapInfo.size     = MAILBOX_SIZE;
            mapInfo.isCached = FALSE;
 #if !defined(SYSLINK_BUILD_OPTIMIZE)
            status =
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                Memory_map (&mapInfo);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "Dm8168IpcInt_setup",
                                     status,
                                     "Failure in Memory_map for mailboxBase");
                Dm8168IpcInt_state.mailboxBase = 0;
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                Dm8168IpcInt_state.mailboxBase = mapInfo.dst;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            }
        }
        if (status >= 0) {
            /*Registering dm8168 platform with ArchIpcInt*/
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            ArchIpcInt_object.fxnTable = &Dm8168IpcInt_fxnTable;
            ArchIpcInt_object.obj      = &Dm8168IpcInt_state;

            for (i = 0; i < MultiProc_getNumProcessors(); i++ ) {
                Atomic_set (&(Dm8168IpcInt_state.isrObjects [i].asserted), 1);
                List_Params_init(&listParams);
                Dm8168IpcInt_state.isrLists [i] = List_create(&listParams);
                if (Dm8168IpcInt_state.isrLists [i] == NULL) {
                    status = DM8168IPCINT_E_MEMORY;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "Dm8168IpcInt_setup",
                                         status,
                                         "Failure in List_create");
                    for (i = i - 1; i >= 0; i--) {
                        List_delete(&Dm8168IpcInt_state.isrLists [i]);
                    }
                    break;
                }
            }

            /* Calling MultiProc APIs here in setup save time in ISR and makes
             * it small and fast with less overhead.  This can be done
             * regardless of status.
             */
            Dm8168IpcInt_state.procIds [DM8168_INDEX_DSP] =
                                                        MultiProc_getId ("DSP");
            Dm8168IpcInt_state.procIds [DM8168_INDEX_VIDEOM3] =
                                                        MultiProc_getId ("VIDEO-M3");
            Dm8168IpcInt_state.procIds [DM8168_INDEX_VPSSM3] =
                                                    MultiProc_getId ("VPSS-M3");
            Dm8168IpcInt_state.maxProcessors = MultiProc_getNumProcessors();

            if (status >= 0) {
                ArchIpcInt_object.isSetup  = TRUE;
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "Dm8168IpcInt_setup");
}


/*!
 *  @brief      Function to finalize the Dm8168IpcInt module
 *
 *  @sa         Dm8168IpcInt_setup
 */
Void
Dm8168IpcInt_destroy (Void)
{
    Memory_UnmapInfo unmapInfo;
    UInt32 i = 0;

    GT_0trace (curTrace, GT_ENTER, "Dm8168IpcInt_destroy");

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));

    ArchIpcInt_object.isSetup  = FALSE;
    ArchIpcInt_object.obj      = NULL;
    ArchIpcInt_object.fxnTable = NULL;

    for (i = 0; i < MultiProc_getNumProcessors(); i++ ) {
        if (Dm8168IpcInt_state.isrLists [i]) {
            List_delete(&Dm8168IpcInt_state.isrLists [i]);
        }
    }

    if (Dm8168IpcInt_state.archCoreCmBase != (UInt32) NULL) {
        unmapInfo.addr = Dm8168IpcInt_state.archCoreCmBase;
        unmapInfo.size = AINTC_BASE_SIZE;
        unmapInfo.isCached = FALSE;
        Memory_unmap (&unmapInfo);
        Dm8168IpcInt_state.archCoreCmBase = (UInt32) NULL;
    }

    if (Dm8168IpcInt_state.mailboxBase != (UInt32) NULL) {
        unmapInfo.addr = Dm8168IpcInt_state.mailboxBase;
        unmapInfo.size = MAILBOX_SIZE;
        unmapInfo.isCached = FALSE;
        Memory_unmap (&unmapInfo);
        Dm8168IpcInt_state.mailboxBase = (UInt32) NULL;
    }

    GT_0trace (curTrace, GT_ENTER, "Dm8168IpcInt_destroy");
}


/*!
 *  @brief      Function to register the interrupt.
 *
 *  @param      procId  destination procId.
 *  @param      intId   interrupt id.
 *  @param      fxn     callback function to be called on receiving interrupt.
 *  @param      fxnArgs arguments to the callback function.
 *
 *  @sa         Dm8168IpcInt_interruptEnable
 */

Int32
Dm8168IpcInt_interruptRegister  (UInt16                     procId,
                                UInt32                     intId,
                                ArchIpcInt_CallbackFxn     fxn,
                                Ptr                        fxnArgs)
{
    Int32 status = DM8168IPCINT_SUCCESS;
    OsalIsr_Params isrParams;

    GT_4trace (curTrace,
               GT_ENTER,
               "Dm8168IpcInt_interruptRegister",
               procId,
               intId,
               fxn,
               fxnArgs);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));
    GT_assert(curTrace, (procId < MultiProc_MAXPROCESSORS));
    GT_assert(curTrace, (fxn != NULL));


    /* This sets the refCount variable is not initialized, upper 16 bits is
     * written with module Id to ensure correctness of refCount variable.
     */
    Atomic_cmpmask_and_set (
                            &Dm8168IpcInt_state.isrObjects [procId].isrRefCount,
                            DM8168IPCINT_MAKE_MAGICSTAMP(0),
                            DM8168IPCINT_MAKE_MAGICSTAMP(0));

    /* This is a normal use-case, so should not be inside
     * SYSLINK_BUILD_OPTIMIZE.
     */
        if (Atomic_inc_return (
                            &Dm8168IpcInt_state.isrObjects [procId].isrRefCount)
            != DM8168IPCINT_MAKE_MAGICSTAMP(1u)) {
        /*! @retval DM8168IPCINT_S_ALREADYREGISTERED ISR already registered!
         */
            status = DM8168IPCINT_S_ALREADYREGISTERED;
        GT_0trace (curTrace,
                   GT_2CLASS,
                   "ISR already registered!");
    }
    else {
            Dm8168IpcInt_state.isrObjects [procId].fxn       = fxn;
            Dm8168IpcInt_state.isrObjects [procId].fxnArgs   = fxnArgs;
            Dm8168IpcInt_state.isrObjects [procId].recvIntId = intId;
        /* Enable hardware interrupt. */
            Dm8168IpcInt_interruptEnable (procId, intId);
    }

    Atomic_cmpmask_and_set (&Dm8168IpcInt_state.isrRefCount,
                            DM8168IPCINT_MAKE_MAGICSTAMP(0),
                            DM8168IPCINT_MAKE_MAGICSTAMP(0));

    /* This is a normal use-case, so should not be inside
     * SYSLINK_BUILD_OPTIMIZE.
     */
    if (   Atomic_inc_return (&Dm8168IpcInt_state.isrRefCount)
        != DM8168IPCINT_MAKE_MAGICSTAMP(1u)) {
        /*! @retval DM8168IPCINT_S_ALREADYREGISTERED Generic ISR already set!
         */
        status = DM8168IPCINT_S_ALREADYREGISTERED;
        GT_0trace (curTrace,
                   GT_2CLASS,
                   "Generic ISR already set !");
    }
    else {
        isrParams.sharedInt        = FALSE;
        isrParams.checkAndClearFxn = &_Dm8168IpcInt_checkAndClearFunc;
        isrParams.fxnArgs          = NULL;
        isrParams.intId            = intId;

        Dm8168IpcInt_state.isrHandle = OsalIsr_create (&_Dm8168IpcInt_isr,
                                                      NULL,
                                                      &isrParams);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (Dm8168IpcInt_state.isrHandle == NULL) {
            /*! @retval DM8168IPCINT_E_FAIL OsalIsr_create failed */
            status = DM8168IPCINT_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "Dm8168IpcInt_interruptRegister",
                                 status,
                                 "OsalIsr_create failed");
        }
        else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            status = OsalIsr_install (Dm8168IpcInt_state.isrHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "Dm8168IpcInt_interruptRegister",
                                     status,
                                     "OsalIsr_install failed");
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                Dm8168IpcInt_state.intId = intId;

                /* Clear the messages in the CORE0->HOST mailbox */
                while (REG32(Dm8168IpcInt_state.mailboxBase + \
                             MAILBOX_MSGSTATUS_m_OFFSET(1))) {
                    Dm8168IpcInt_clearInterrupt(1);
                }
                /* Clear the messages in the HOST->CORE0 mailbox */
                while (REG32(Dm8168IpcInt_state.mailboxBase + \
                             MAILBOX_MSGSTATUS_m_OFFSET(0))) {
                    Dm8168IpcInt_clearInterrupt(0);
                }
                /* Clear the messages in the DSP->HOST mailbox */
                while (REG32(Dm8168IpcInt_state.mailboxBase + \
                             MAILBOX_MSGSTATUS_m_OFFSET(2))) {
                    Dm8168IpcInt_clearInterrupt(2);
                }
                /* Clear the messages in the HOST->DSP mailbox */
                while (REG32(Dm8168IpcInt_state.mailboxBase + \
                             MAILBOX_MSGSTATUS_m_OFFSET(3))) {
                    Dm8168IpcInt_clearInterrupt(3);
                }
                /* The below seems to be needed for OMAP5 Virtio for
                 * slaying/restarting syslink properly
                 */
                /* Disables interrupts from HOST->CORE0 */
                SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + \
                            MAILBOX_IRQENABLE_CLR_OFFSET + 0x20),
                        ((MAILBOX_NUMBER_0) << 1));
                /* Disables interrupts from HOST->DSP */
                SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + \
                            MAILBOX_IRQENABLE_CLR_OFFSET + 0x10),
                        ((MAILBOX_NUMBER_3) << 1));

                /* Set mailbox to smart-idle */
                REG(Dm8168IpcInt_state.mailboxBase + MAILBOX_SYSCONFIG_OFFSET) = 0x8;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            }
        }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    }

    GT_1trace (curTrace, GT_LEAVE, "Dm8168IpcInt_interruptRegister", status);

    /*! @retval DM8168IPCINT_SUCCESS Interrupt successfully registered */
    return status;
}

/*!
 *  @brief      Function to Save context.
 *
 *  @param      procId  The procId associated with the mailbox context being
 *                      saved.
 *
 *  @sa         Dm8168IpcInt_mbxRestoreCtxt
 */

Int32
Dm8168IpcInt_mboxSaveCtxt (UInt16 procId)
{
    Int32 status = DM8168IPCINT_SUCCESS;
    UInt32 i = 0;

    if (Dm8168IpcInt_state.mailboxBase == NULL) {
        status = DM8168IPCINT_E_MEMORY;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_mboxSaveCtxt",
                             status,
                             "Unable to map the Mailbox memory in SaveCtxt");
    }
    else {
        for (i = 0; i < 4; i++) {
            mailbox_context[i] = REG32(Dm8168IpcInt_state.mailboxBase + \
                                       DM8168_MAILBOX_IRQENABLE(i));
        }
        Dm8168IpcInt_interruptDisable(procId, Dm8168IpcInt_state.intId);
    }
    return status;
}

/*!
 *  @brief      Function to Restore context.
 *
 *  @param      procId  The procId associated with the mailbox context being
 *                      restored.
 *
 *  @sa         Dm8168IpcInt_mbxSaveCtxt
 */

Int32
Dm8168IpcInt_mboxRestoreCtxt (UInt16 procId)
{
    Int32 status = DM8168IPCINT_SUCCESS;
    UInt32 i = 0;

    if (Dm8168IpcInt_state.mailboxBase == NULL) {
        status = DM8168IPCINT_E_MEMORY;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_mboxRestoreCtxt",
                             status,
                             "Unable to map the Mailbox memory in RestoreCtxt");
    }
    else {
        /* Set to Smart Idle mode*/
        REG(Dm8168IpcInt_state.mailboxBase + MAILBOX_SYSCONFIG_OFFSET) = 0x8;

        for (i = 0; i < 4; i++) {
            REG32(Dm8168IpcInt_state.mailboxBase + \
                               DM8168_MAILBOX_IRQENABLE(i)) = mailbox_context[i];
        }

        Dm8168IpcInt_interruptEnable(procId, Dm8168IpcInt_state.intId);
    }

    return status;
}


/*!
 *  @brief      Function to unregister interrupt.
 *
 *  @param      procId  destination procId
 *
 *  @sa         Dm8168IpcInt_interruptRegister
 */
Int32
Dm8168IpcInt_interruptUnregister  (UInt16 procId)
{
    Int32 status = DM8168IPCINT_SUCCESS;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int32 tmpStatus = DM8168IPCINT_SUCCESS;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace,GT_ENTER,"Dm8168IpcInt_interruptUnregister", procId);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));
    GT_assert(curTrace, (procId < MultiProc_MAXPROCESSORS));

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (   Atomic_cmpmask_and_lt (
                            &Dm8168IpcInt_state.isrObjects [procId].isrRefCount,
                            DM8168IPCINT_MAKE_MAGICSTAMP(0),
                            DM8168IPCINT_MAKE_MAGICSTAMP(1))
        == TRUE) {
        /*! @retval DM8168IPCINT_E_INVALIDSTATE ISR was not registered */
        status = DM8168IPCINT_E_INVALIDSTATE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_interruptUnregister",
                             status,
                             "ISR was not registered!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        /* This is a normal use-case, so should not be inside
         * SYSLINK_BUILD_OPTIMIZE.
         */
        if (Atomic_dec_return(&Dm8168IpcInt_state.isrObjects[procId].isrRefCount)
            == DM8168IPCINT_MAKE_MAGICSTAMP(0)) {
            /* Disable hardware interrupt. */
            Dm8168IpcInt_interruptDisable (procId,
                              Dm8168IpcInt_state.isrObjects [procId].recvIntId);

            Dm8168IpcInt_state.isrObjects [procId].fxn       = NULL;
            Dm8168IpcInt_state.isrObjects [procId].fxnArgs   = NULL;
            Dm8168IpcInt_state.isrObjects [procId].recvIntId = -1u;
        }

        if (   Atomic_dec_return (&Dm8168IpcInt_state.isrRefCount)
            == DM8168IPCINT_MAKE_MAGICSTAMP(0)) {
            status = OsalIsr_uninstall (Dm8168IpcInt_state.isrHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "Dm8168IpcInt_interruptUnregister",
                                     status,
                                     "OsalIsr_uninstall failed");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            /* Clear the messages in the CORE0->HOST mailbox */
            while (REG32(Dm8168IpcInt_state.mailboxBase + \
                         MAILBOX_MSGSTATUS_m_OFFSET(1))) {
                Dm8168IpcInt_clearInterrupt(1);
            }
            /* Clear the messages in the HOST->CORE0 mailbox */
            while (REG32(Dm8168IpcInt_state.mailboxBase + \
                         MAILBOX_MSGSTATUS_m_OFFSET(0))) {
                Dm8168IpcInt_clearInterrupt(0);
            }
            /* Clear the messages in the DSP->HOST mailbox */
            while (REG32(Dm8168IpcInt_state.mailboxBase + \
                         MAILBOX_MSGSTATUS_m_OFFSET(2))) {
                Dm8168IpcInt_clearInterrupt(2);
            }
            /* Clear the messages in the HOST->DSP mailbox */
            while (REG32(Dm8168IpcInt_state.mailboxBase + \
                         MAILBOX_MSGSTATUS_m_OFFSET(3))) {
                Dm8168IpcInt_clearInterrupt(3);
            }
            /* The below seems to be needed for OMAP5 Virtio for
             * slaying/restarting syslink properly
             */
            /* Disables interrupts from HOST->CORE0 */
            SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + \
                        MAILBOX_IRQENABLE_CLR_OFFSET + 0x20),
                    ((MAILBOX_NUMBER_0) << 1));
            /* Disables interrupts from HOST->DSP */
            SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + \
                        MAILBOX_IRQENABLE_CLR_OFFSET + 0x10),
                    ((MAILBOX_NUMBER_3) << 1));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            tmpStatus =
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                OsalIsr_delete (&(Dm8168IpcInt_state.isrHandle));
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if ((status >= 0) && (tmpStatus < 0)) {
                status = tmpStatus;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "Dm8168IpcInt_interruptUnregister",
                                     status,
                                     "OsalIsr_delete failed");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "Dm8168IpcInt_interruptUnregister",
               status);

    /*! @retval DM8168IPCINT_SUCCESS Interrupt successfully unregistered */
    return status;
}


/*!
 *  @brief      Function to enable the specified interrupt
 *
 *  @param      procId  Remote processor ID
 *  @param      intId   interrupt id
 *
 *  @sa         Dm8168IpcInt_interruptDisable
 */
Void
Dm8168IpcInt_interruptEnable (UInt16 procId, UInt32 intId)
{
    GT_2trace (curTrace, GT_ENTER, "Dm8168IpcInt_interruptEnable",
               procId, intId);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));
    GT_assert (curTrace, (procId < MultiProc_MAXPROCESSORS));

    if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_DSP]) {
        /*
         * Mailbox 2 is used by HOST for receiving interrupts from DSP
         * Mailbox 3 is used by DSP  for receiving interrupts from HOST
         *
         */
        SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + MAILBOX_IRQENABLE_SET_OFFSET),
                ( (MAILBOX_NUMBER_2) << 1));
    }
    else if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VIDEOM3]) {
        /*
         * Do nothing.  Interrupt is handled through CORE0 interrupt.
         */
    }
    else if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VPSSM3]) {
        /*
         * Mailbox 1 is used by HOST  for receiving interrupts from VPSSM3
         * Mailbox 0 is used by VPSSM3 for receiving interrupts from HOST
         *
         */
        SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + MAILBOX_IRQENABLE_SET_OFFSET),
                ( (MAILBOX_NUMBER_1) << 1));
    }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    else {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_interruptEnable",
                             DM8168IPCINT_E_FAIL,
                             "Invalid procId specified");
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "Dm8168IpcInt_interruptEnable");
}


/*!
 *  @brief      Function to disable the specified interrupt
 *
 *  @param      procId  Remote processor ID
 *  @param      intId   interrupt id
 *
 *  @sa         Dm8168IpcInt_interruptEnable
 */
Void
Dm8168IpcInt_interruptDisable (UInt16 procId, UInt32 intId)
{
    GT_2trace (curTrace, GT_ENTER, "Dm8168IpcInt_interruptDisable",
               procId, intId);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));
    GT_assert (curTrace, (procId < MultiProc_MAXPROCESSORS));

    if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_DSP]) {
        /*
         * Mailbox 2 is used by HOST for receiving interrupts from DSP
         * Mailbox 3 is used by DSP  for receiving interrupts from HOST
         *
         */
        SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + MAILBOX_IRQENABLE_CLR_OFFSET),
                ( (MAILBOX_NUMBER_2) << 1));
    }
    else if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VIDEOM3]) {
        /*
         * Do nothing.  Interrupt is handled through VPSSM3 interrupt.
         */
    }
    else if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VPSSM3]) {
        /*
         * Mailbox 1 is used by HOST  for receiving interrupts from VPSSM3
         * Mailbox 0 is used by VPSSM3  for receiving interrupts from HOST
         *
         */
        SET_BIT(REG(Dm8168IpcInt_state.mailboxBase + MAILBOX_IRQENABLE_CLR_OFFSET),
                ( (MAILBOX_NUMBER_1) << 1));
    }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    else {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_interruptDisable",
                             DM8168IPCINT_E_FAIL,
                             "Invalid procId specified");
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "Dm8168IpcInt_interruptDisable");
}


/*!
 *  @brief      Function to wait for interrupt to be cleared.
 *
 *  @param      procId  Remote processor ID
 *  @param      intId   interrupt id
 *
 *  @sa         Dm8168IpcInt_sendInterrupt
 */
Int32
Dm8168IpcInt_waitClearInterrupt (UInt16 procId, UInt32 intId)
{
    Int32 status = DM8168IPCINT_SUCCESS;

    GT_2trace (curTrace,GT_ENTER,"Dm8168IpcInt_waitClearInterrupt",
               procId, intId);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));
    GT_assert (curTrace, (procId < MultiProc_MAXPROCESSORS));

    if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_DSP]) {
        /* Wait for DSP to clear the previous interrupt */
        while( (  REG32((  Dm8168IpcInt_state.mailboxBase
                        + MAILBOX_MSGSTATUS_3_OFFSET))
                & 0x3F ));
    }
    else if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VIDEOM3]) {
        /* Wait for VIDEOM3 to clear the previous interrupt */
        while( (  REG32((Dm8168IpcInt_state.mailboxBase
                      + MAILBOX_MSGSTATUS_0_OFFSET))
                & 0x3F ));
    }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    else {
        /*! @retval DM8168IPCINT_E_FAIL Invalid procId specified */
        status = DM8168IPCINT_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_waitClearInterrupt",
                             status,
                             "Invalid procId specified");
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace,GT_LEAVE,"Dm8168IpcInt_waitClearInterrupt", status);

    /*! @retval DM8168IPCINT_SUCCESS Wait for interrupt clearing successfully
                completed. */
    return status ;
}


/*!
 *  @brief      Function to send a specified interrupt to the DSP.
 *
 *  @param      procId  Remote processor ID
 *  @param      intId   interrupt id
 *  @param      value   Value to be sent with the interrupt
 *
 *  @sa         Dm8168IpcInt_waitClearInterrupt
 */
Int32
Dm8168IpcInt_sendInterrupt (UInt16 procId, UInt32 intId,  UInt32 value)
{
    Int32 status = DM8168IPCINT_SUCCESS;

    GT_3trace (curTrace, GT_ENTER, "Dm8168IpcInt_sendInterrupt",
               procId, intId, value);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));
    GT_assert (curTrace, (procId < MultiProc_MAXPROCESSORS));

    if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_DSP]) {
        /*
         * Mailbox 2 is used by HOST for receiving interrupts from DSP
         * Mailbox 3 is used by DSP  for receiving interrupts from HOST
         *
         */
        REG32(Dm8168IpcInt_state.mailboxBase + MAILBOX_MESSAGE_3_OFFSET) = value;
    } else if (procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VIDEOM3]||
        procId == Dm8168IpcInt_state.procIds [DM8168_INDEX_VPSSM3]) {
        /*
         * Mailbox 1 is used by HOST   for receiving interrupts from CORE0
         * Mailbox 0 is used by CORE0  for receiving interrupts from DSP
         *
         */
        REG32(Dm8168IpcInt_state.mailboxBase + MAILBOX_MESSAGE_0_OFFSET) = value;
    }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    else {
        /*! @retval DM8168IPCINT_E_FAIL Invalid procId specified */
        status = DM8168IPCINT_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_sendInterrupt",
                             status,
                             "Invalid procId specified");
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "Dm8168IpcInt_sendInterrupt",status);

    /*! @retval DM8168IPCINT_SUCCESS Interrupt successfully sent */
    return status;
}


/*!
 *  @brief      Function to clear the specified interrupt received from the
 *              remote core.
 *
 *  @param      procId  Remote processor ID
 *  @param      intId   interrupt id
 *
 *  @sa         Dm8168IpcInt_sendInterrupt
 */
UInt32
Dm8168IpcInt_clearInterrupt (UInt16 mboxNum)
{
    UInt32 retVal = 0;

    GT_1trace (curTrace,GT_ENTER,"Dm8168IpcInt_clearInterrupt", mboxNum);

    GT_assert (curTrace,(ArchIpcInt_object.isSetup == TRUE));

    if (mboxNum < MAILBOX_MAXNUM) {
        /* Read the register to get the entry from the mailbox FIFO */
        retVal = REG32(Dm8168IpcInt_state.mailboxBase
                       + MAILBOX_MESSAGE_m_OFFSET(mboxNum));

        /* Clear the IRQ status.
         * If there are more in the mailbox FIFO, it will re-assert.
         */
        SET_BIT(REG((Dm8168IpcInt_state.mailboxBase
                    + MAILBOX_IRQSTATUS_CLEAR_OFFSET)),
                    (mboxNum<<1));
    }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    else {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Dm8168IpcInt_clearInterrupt",
                             DM8168IPCINT_E_FAIL,
                             "Invalid mailbox number specified");
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "Dm8168IpcInt_clearInterrupt");

    /*! @retval Value Value received with the interrupt. */
    return retVal;
}


/*
 * Instead of constantly allocating and freeing the msg structures
 * we just cache a few of them, and recycle them instead.
 */
#define CACHE_NUM 20
static Dm8168IpcInt_MsgListElem *msg_cache;
static int num_msg = 0;

static Dm8168IpcInt_MsgListElem *get_msg()
{
    Dm8168IpcInt_MsgListElem *msg;
    IArg key = NULL;

    key = Gate_enterSystem();
    msg = msg_cache;
    if (msg != NULL) {
        msg_cache = (Dm8168IpcInt_MsgListElem *)msg_cache->next;
        num_msg--;
        Gate_leaveSystem(key);
    } else {
        Gate_leaveSystem(key);
        msg = Memory_alloc(NULL, sizeof(Dm8168IpcInt_MsgListElem), 0, NULL);
    }
    return(msg);
}

static void put_msg(Dm8168IpcInt_MsgListElem * msg)
{
    IArg key = NULL;
    key = Gate_enterSystem();
    if (num_msg >= CACHE_NUM) {
        Gate_leaveSystem(key);
        Memory_free(NULL, msg, sizeof(*msg));
    } else {
        msg->next = (struct Dm8168IpcInt_MsgListElem *)msg_cache;
        msg_cache = msg;
        num_msg++;
        Gate_leaveSystem(key);
    }
    return;
}


/*!
 *  @brief      Function to check and clear the remote proc interrupt
 *
 *  @param      arg     Optional argument to the function.
 *
 *  @sa         _Dm8168IpcInt_isr
 */
static
Bool
_Dm8168IpcInt_checkAndClearFunc (Ptr arg)
{
    UInt16 procId;
    UInt32 msg;
    Dm8168IpcInt_MsgListElem * elem = NULL;

    if( REG32(  Dm8168IpcInt_state.mailboxBase
              + MAILBOX_MSGSTATUS_1_OFFSET) != 0 ){
        msg = Dm8168IpcInt_clearInterrupt (1);
        procId = Dm8168IpcInt_state.procIds [DM8168_INDEX_VIDEOM3];
        /* This is a message from CORE0, put the message in CORE0's list */
        elem = get_msg();
        if (elem) {
            elem->msg = msg;
            List_put(Dm8168IpcInt_state.isrLists[procId], (List_Elem *)elem);
        }
    }
    if( REG32(  Dm8168IpcInt_state.mailboxBase
              + MAILBOX_MSGSTATUS_2_OFFSET) != 0 ){
        msg = Dm8168IpcInt_clearInterrupt (2);
        procId = Dm8168IpcInt_state.procIds [DM8168_INDEX_DSP];
        /* This is a message from DSP, put the message in DSP's list */
        elem = get_msg();
        if (elem) {
            elem->msg = msg;
            List_put(Dm8168IpcInt_state.isrLists[procId], (List_Elem *)elem);
        }
    }

    /* This is not a shared interrupt, so interrupt has always occurred */
    /*! @retval TRUE Interrupt has occurred. */
    return (TRUE);
}


/*!
 *  @brief      Interrupt Service Routine for Dm8168IpcInt module
 *
 *  @param      arg     Optional argument to the function.
 *
 *  @sa         _Dm8168IpcInt_checkAndClearFunc
 */
static
Bool
_Dm8168IpcInt_isr (Ptr ref)
{
    UInt16 i = 0;
    Dm8168IpcInt_MsgListElem * elem = NULL;
    GT_1trace (curTrace, GT_ENTER, "_Dm8168IpcInt_isr", ref);

    for (i = 0 ; i < Dm8168IpcInt_state.maxProcessors ; i++) {
        if ((elem = List_get(Dm8168IpcInt_state.isrLists [i])) != NULL) {
            /*Calling the particular ISR */
            GT_assert(curTrace,(Dm8168IpcInt_state.isrObjects [i].fxn != NULL));
            if (Dm8168IpcInt_state.isrObjects [i].fxn != NULL) {
                Dm8168IpcInt_state.isrObjects [i].fxn (elem->msg,
                                    Dm8168IpcInt_state.isrObjects [i].fxnArgs);
            }
            put_msg(elem);
        }
    }

    GT_1trace (curTrace, GT_LEAVE, "_Dm8168IpcInt_isr", TRUE);

    /*! @retval TRUE Interrupt has been handled. */
    return (TRUE);
}

#if defined (__cplusplus)
}
#endif
