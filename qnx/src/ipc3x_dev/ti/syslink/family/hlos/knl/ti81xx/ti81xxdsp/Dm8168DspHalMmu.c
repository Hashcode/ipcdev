/*
 *  @file   Dm8168DspHalMmu.c
 *
 *  @brief      Hardware abstraction for Memory Management Unit module.
 *
 *              This module is responsible for handling slave MMU-related
 *              hardware- specific operations.
 *              The implementation is specific to DM8168DSP.
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

/* OSAL and utils headers */
#include <ti/syslink/utils/List.h>
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/utils/OsalPrint.h>
#include <ti/syslink/utils/Memory.h>
#include <Bitops.h>

/* Module level headers */
#include <_ProcDefs.h>
#include <Processor.h>

/* Hardware Abstraction Layer headers */
#include <Dm8168DspHal.h>
#include <Dm8168DspHalMmu.h>
#include <Dm8168DspPhyShmem.h>


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 * @brief Defines default mixedSize i.e same types of pages in one segment
 */
#define MMU_RAM_DEFAULT         0
/*!
 * @brief returns page number based on physical address
 */
#define ti_syslink_phys_to_page(phys)      pfn_to_page((phys) >> PAGE_SHIFT)

/*!
 * @brief helper macros
 */
#define SLAVEVIRTADDR(x)  ((x)->addr [ProcMgr_AddrType_SlaveVirt])
#define SLAVEPHYSADDR(x)  ((x)->addr [ProcMgr_AddrType_SlavePhys])
#define MASTERPHYSADDR(x) ((x)->addr [ProcMgr_AddrType_MasterPhys])

#define MMUPAGE_ALIGN(size, psz)  (((size) + psz - 1) & ~(psz -1))

/* =============================================================================
 *  Forward declarations of internal functions
 * =============================================================================
 */
#if defined(SYSLINK_BUILDOS_LINUX)
/* Enables the MMU for GEM Module. */
Int _DM8168DSP_halMmuEnable (DM8168DSP_HalObject * halObject,
                            UInt32               numMemEntries,
                            ProcMgr_AddrInfo *   memTable);

/* Disables the MMU for GEM Module. */
Int _DM8168DSP_halMmuDisable (DM8168DSP_HalObject * halObject);

/* Add entry in TWL. */
Int
_DM8168DSP_halMmuAddEntry (DM8168DSP_HalObject       * halObject,
                          DM8168DSP_HalMmuEntryInfo * entry);
/* Add static entries in TWL. */
Int
_DM8168DSP_halMmuAddStaticEntries (DM8168DSP_HalObject * halObject,
                                  UInt32               numMemEntries,
                                  ProcMgr_AddrInfo *   memTable);

/* Delete entry from TLB. */
Int
_DM8168DSP_halMmuDeleteEntry (DM8168DSP_HalObject       * halObject,
                             DM8168DSP_HalMmuEntryInfo * entry);
/* Set entry in to TLB. */
Int
_DM8168DSP_halMmuPteSet (DM8168DSP_HalObject       * halObject,
                        DM8168DSP_HalMmuEntryInfo * setPteInfo);
/* Print page dump */
Int
_DM8168DSP_badPageDump(UInt32 phyAddr, struct page *pg);

/* IOMMU Exported function */
extern
void
iopgtable_lookup_entry (struct iommu *obj, u32 da, u32 **ppgd, u32 **ppte);
#endif /* #if defined(SYSLINK_BUILDOS_LINUX) */

/* =============================================================================
 * APIs called by DM8168DSPPROC module
 * =============================================================================
 */
/*!
 *  @brief      Function to control MMU operations for this slave device.
 *
 *  @param      halObj  Pointer to the HAL object
 *  @param      cmd     MMU control command
 *  @param      arg     Arguments specific to the MMU control command
 *
 *  @sa
 */
Int
DM8168DSP_halMmuCtrl (Ptr halObj, Processor_MmuCtrlCmd cmd, Ptr args)
{
	Int                  status     = PROCESSOR_SUCCESS;
#if defined(SYSLINK_BUILDOS_LINUX)
    DM8168DSP_HalObject * halObject  = NULL;
#endif /* #if defined(SYSLINK_BUILDOS_LINUX) */
    GT_3trace (curTrace, GT_ENTER, "DM8168DSP_halMmuCtrl", halObj, cmd, args);
#if defined(SYSLINK_BUILDOS_LINUX)
    GT_assert (curTrace, (halObj != NULL));

    halObject = (DM8168DSP_HalObject *) halObj ;

    switch (cmd) {
        case Processor_MmuCtrlCmd_Enable:
        {
            DM8168DSP_HalMmuCtrlArgs_Enable * enableArgs;
            enableArgs = (DM8168DSP_HalMmuCtrlArgs_Enable *) args;
            halObject = (DM8168DSP_HalObject *) halObj;
            status = _DM8168DSP_halMmuEnable (halObject,
                                             enableArgs->numMemEntries,
                                             enableArgs->memEntries);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                /*! @retval PROCESSOR_E_FAIL Failed to configure DSP MMU. */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSP_halMmuCtrl",
                                     status,
                                     "Failed to configure DSP MMU"
                                     "at _DM8168DSP_halMmuEnable");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
        break;

        case Processor_MmuCtrlCmd_Disable:
        {
            /* args are not used. */
            halObject = (DM8168DSP_HalObject *) halObj;
            status = _DM8168DSP_halMmuDisable (halObject);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                /*! @retval PROCESSOR_E_FAIL Failed to disable DSP MMU. */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSP_halMmuCtrl",
                                     status,
                                     "Failed to disable DSP MMU");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
        break ;

        case Processor_MmuCtrlCmd_AddEntry:
        {
            DM8168DSP_HalMmuCtrlArgs_AddEntry * addEntry;
            addEntry = (DM8168DSP_HalMmuCtrlArgs_AddEntry *) args;

            halObject = (DM8168DSP_HalObject *) halObj;
            /* Add the entry in TLB for new request */
            status = _DM8168DSP_halMmuAddEntry (halObject,addEntry) ;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                /*! @retval PROCESSOR_E_FAIL Failed to dynamically add DSP MMU
                 *                           entry. */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSP_halMmuCtrl",
                                     status,
                                     "Failed to dynamically add DSP MMU entry");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
        break;

        case Processor_MmuCtrlCmd_DeleteEntry:
        {
            DM8168DSP_HalMmuCtrlArgs_DeleteEntry * deleteEntry;
            deleteEntry = (DM8168DSP_HalMmuCtrlArgs_DeleteEntry *) args;

            halObject = (DM8168DSP_HalObject *) halObj;
            /* Add the entry in TLB for new request */
            status = _DM8168DSP_halMmuDeleteEntry (halObject,deleteEntry);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                /*! @retval PROCESSOR_E_FAIL Failed to dynamically delete DSP
                 *                           MMU entry  */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSP_halMmuCtrl",
                                     status,
                                     "Failed to dynamically add DSP MMU entry");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
        break;

        default:
        {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSP_halMmuCtrl",
                                 status,
                                 "Unsupported MMU ctrl cmd specified");
        }
        break;
    }
#endif /* #if defined(SYSLINK_BUILDOS_LINUX) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168DSP_halMmuCtrl",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}

#if defined(SYSLINK_BUILDOS_LINUX)
/* =============================================================================
 * Internal functions
 * =============================================================================
 */
/*!
 *  @brief      Enables and configures the DSP MMU as per provided memory map.
 *
 *  @param      halObject       Pointer to the HAL object
 *  @param      numMemEntries   Number of memory entries in memTable
 *  @param      memTable        Table of memory entries to be configured
 *
 *  @sa
 */
Int
_DM8168DSP_halMmuAddStaticEntries (DM8168DSP_HalObject * halObject,
                                  UInt32               numMemEntries,
                                  ProcMgr_AddrInfo *   memTable)
{
    Int                           status    = PROCESSOR_SUCCESS;
    DM8168DSP_HalMmuEntryInfo      staticEntry;
    UInt32                        i;

    GT_3trace (curTrace, GT_ENTER, "_DM8168DSP_halMmuAddStaticEntries",
               halObject, numMemEntries, memTable);

    GT_assert (curTrace, (halObject != NULL));
    /* It is possible that numMemEntries may be 0, if user does not want to
     * configure any default regions.
     * memTable may also be NULL.
     */

    for (i = 0 ; i < numMemEntries && (status >= 0) ; i++) {
        if (SLAVEVIRTADDR (&memTable [i]) >= MMU_GLOBAL_MEMORY) {
            /* Configure the TLB */
            if (memTable [i].size != 0) {
                staticEntry.slaveVirtAddr =
                                         SLAVEVIRTADDR (&memTable [i]);
                staticEntry.size          = memTable[i].size;
                staticEntry.masterPhyAddr =
                                         MASTERPHYSADDR (&memTable [i]);
                /*TBD : elementSize, endianism, mixedSized are hard
                 *      coded now, must be configurable later*/
                staticEntry.elementSize   = MMU_RAM_ELSZ_16;
                staticEntry.endianism     = LITTLE_ENDIAN;
                staticEntry.mixedSize     = MMU_TLBES;
                status = _DM8168DSP_halMmuAddEntry (halObject,
                                                   &staticEntry);
                if (status < 0) {
                    /*! @retval PROCESSOR_E_FAIL Failed to add MMU entry. */
                    status = PROCESSOR_E_FAIL;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "_DM8168DSP_halMmuAddStaticEntries",
                                         status,
                                         "Failed to add MMU entry!");
                }
            }
        }
    }
    GT_1trace (curTrace, GT_LEAVE, "_DM8168DSP_halMmuAddStaticEntries", status);

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status ;
}


/*!
 *  @brief      Enables and configures the DSP MMU as per provided memory map.
 *
 *  @param      halObject       Pointer to the HAL object
 *  @param      numMemEntries   Number of memory entries in memTable
 *  @param      memTable        Table of memory entries to be configured
 *
 *  @sa
 */
Int
_DM8168DSP_halMmuEnable (DM8168DSP_HalObject * halObject,
                        UInt32               numMemEntries,
                        ProcMgr_AddrInfo *   memTable)
{
    Int                           status    = PROCESSOR_SUCCESS;
    DM8168DSP_HalMmuObject *       mmuObj;

    GT_3trace (curTrace, GT_ENTER, "_DM8168DSP_halMmuEnable",
               halObject, numMemEntries, memTable);

    GT_assert (curTrace, (halObject != NULL));
    /* It is possible that numMemEntries may be 0, if user does not want to
     * configure any default regions.
     * memTable may also be NULL.
     */
    mmuObj = &(halObject->mmuObj);
    /* Check if dspMmuHandler is alreaday available if yes put it back and get
     * new one */
    if(mmuObj->dspMmuHandler)
    {
        iommu_put(mmuObj->dspMmuHandler);
        mmuObj->dspMmuHandler = NULL;
    }
    mmuObj->dspMmuHandler = iommu_get("sys");

#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (IS_ERR(mmuObj->dspMmuHandler)) {
        /*! @retval PROCESSOR_E_FAIL Failed at iommu_get. */
        status = PROCESSOR_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_DM8168DSP_halMmuEnable",
                             status,
                             "iommu_get failed!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

        if ((status >= 0) && (numMemEntries != 0)) {
            /* Only statically created entries are being added here. */
            status = _DM8168DSP_halMmuAddStaticEntries(halObject,
                                                      numMemEntries,
                                                      memTable);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                /*! @retval PROCESSOR_E_FAIL Failed at
                 *                         _DM8168DSP_halMmuAddStaticEntries. */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_DM8168DSP_halMmuEnable",
                                     status,
                                   "_DM8168DSP_halMmuAddStaticEntries failed !");
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_1trace (curTrace, GT_LEAVE, "_DM8168DSP_halMmuEnable", status);

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status ;
}


/*!
 *  @brief      Disables the DSP MMU.
 *
 *  @param      halObject       Pointer to the HAL object
 *
 *  @sa
 */
Int
_DM8168DSP_halMmuDisable (DM8168DSP_HalObject * halObject)
{
    Int                        status    = PROCESSOR_SUCCESS;
    DM8168DSP_HalMmuObject *    mmuObj;

    GT_1trace (curTrace, GT_ENTER, "_DM8168DSP_halMmuDisable", halObject);

    GT_assert (curTrace, (halObject != NULL));
    mmuObj = &(halObject->mmuObj);

    /*TBD : Delete all added static entries in TWL*/

    /* Disable iommu. */
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if(mmuObj->dspMmuHandler == NULL){
        /*! @retval PROCESSOR_E_FAIL mmuObj->clkHandle is NULL ! */
        status = PROCESSOR_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_DM8168DSP_halMmuDisable",
                             status,
                             "dspMmuHandler is NULL !");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        iommu_put(mmuObj->dspMmuHandler);
        mmuObj->dspMmuHandler = NULL;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    GT_1trace (curTrace, GT_LEAVE, "_DM8168DSP_halMmuDisable", status);

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status;
}


/*!
 *  @brief      This function adds an MMU entry for the specfied address and
 *              size.
 *
 *  @param      halObject   Pointer to the HAL object
 *  @param      entry       entry to be added
 *
 *  @sa
 */
Int
_DM8168DSP_halMmuAddEntry (DM8168DSP_HalObject       * halObject,
                          DM8168DSP_HalMmuEntryInfo * entry)
{
    Int                         status = PROCESSOR_SUCCESS;
    UInt32  *                   ppgd = NULL;
    UInt32  *                   ppte = NULL;
    DM8168DSP_HalMmuEntryInfo    te;
    DM8168DSP_HalMmuEntryInfo    currentEntry;
    Int32                       currentEntrySize;

    GT_2trace (curTrace, GT_ENTER, "_DM8168DSP_halMmuAddEntry",
               halObject, entry);

    GT_assert (curTrace, (halObject != NULL));
    GT_assert (curTrace, (entry     != NULL));

    /* Add the entry (or entries) */
    Memory_copy(&currentEntry,
                entry,
                sizeof(DM8168DSP_HalMmuEntryInfo));

    /* Align the addresses to page size */
    currentEntry.size += (currentEntry.slaveVirtAddr & (PAGE_SIZE_4KB -1));
    currentEntry.slaveVirtAddr &= ~(PAGE_SIZE_4KB-1);
    currentEntry.masterPhyAddr &= ~(PAGE_SIZE_4KB-1);

    /* Align the size as well */
    currentEntry.size = MMUPAGE_ALIGN(currentEntry.size, PAGE_SIZE_4KB);
    currentEntrySize = currentEntry.size;

    /* To find the max. page size with which both PA & VA are
     * aligned */
    while ((currentEntrySize != 0) &&
           (status >= 0) &&
           (currentEntry.slaveVirtAddr > MMU_GLOBAL_MEMORY)) {
        if (currentEntrySize >= PAGE_SIZE_16MB
            && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_16MB - 1))) {
            currentEntry.size = PAGE_SIZE_16MB;
        }
        else if (currentEntrySize >= PAGE_SIZE_1MB
                 && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_1MB - 1))) {
            currentEntry.size = PAGE_SIZE_1MB;
        }
        else if (currentEntrySize >= PAGE_SIZE_64KB
                 && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_64KB - 1))){
            currentEntry.size = PAGE_SIZE_64KB;
        }
        else  if (currentEntrySize >= PAGE_SIZE_4KB
                 && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_4KB - 1))){
            currentEntry.size = PAGE_SIZE_4KB;
        }
        else {
            Osal_printf ("Configuration error: "
                         " MMU entries must be aligned to their"
                         "page size(4KB,"
                         " 64KB, 1MB, or 16MB).\n");
            Osal_printf ("Since the addresses are not aligned buffer"
                         "of size: %x at address: %x cannot be  "
                         "TLB entries\n",
                       currentEntrySize, currentEntry.slaveVirtAddr);
            /*! @retval DM8168DSPPROC_E_MMUCONFIG Memory region is not
                                                 aligned to page size */
            status = DM8168DSPPROC_E_MMUCONFIG;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "_DM8168DSP_halMmuAddEntry",
                               status,
                          "Memory region is not aligned to page size!");
            break ;
        }

        /* DO NOT put this check under SYSLINK_BUILD_OPTIMIZE */
        if (status >= 0) {
            /* Check every page if exists */
            iopgtable_lookup_entry (halObject->mmuObj.dspMmuHandler,
                                    currentEntry.slaveVirtAddr,
                                    (u32 **) &ppgd,
                                    (u32 **) &ppte);

            /* Lookup if the entry exists */
            if (!ppgd || !ppte) {
                /* Entry doesnot exists, insert this page */
                te.size = currentEntry.size;
                te.slaveVirtAddr = currentEntry.slaveVirtAddr;
                te.masterPhyAddr = currentEntry.masterPhyAddr;
                te.elementSize   = currentEntry.elementSize;
                te.endianism     = currentEntry.endianism;
                te.mixedSize     = currentEntry.mixedSize;
                status = _DM8168DSP_halMmuPteSet (halObject, &te);
            }
            else if (ppgd && ppte) {
                if (currentEntry.masterPhyAddr != (*ppte & IOPAGE_MASK)) {
                    /* Entry doesnot exists, insert this page */
                    te.size = currentEntry.size;
                    te.slaveVirtAddr = currentEntry.slaveVirtAddr;
                    te.masterPhyAddr = currentEntry.masterPhyAddr;
                    te.elementSize   = currentEntry.elementSize;
                    te.endianism     = currentEntry.endianism;
                    te.mixedSize     = currentEntry.mixedSize;
                    status = _DM8168DSP_halMmuPteSet (halObject, &te);
                }
            }

#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason (
                                curTrace,
                                GT_4CLASS,
                                "_DM8168DSP_halMmuAddEntry",
                                status,
                                "Failed to in _DM8168DSP_halMmuPteSet!");
                break;
            }
            else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                currentEntrySize           -= currentEntry.size;
                currentEntry.masterPhyAddr += currentEntry.size;
                currentEntry.slaveVirtAddr += currentEntry.size;
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        }
    }

    GT_1trace (curTrace, GT_LEAVE, "_DM8168DSP_halMmuAddEntry", status);

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status;
}


/*!
 *  @brief      Prints the page dump in case of error.
 *
 *  @param      phyAddr   Physical address
 *  @param      pg        Mapped page
 *
 *  @sa
 */
Int
_DM8168DSP_badPageDump(UInt32 phyAddr, struct page *pg)
{
    Int  status = PROCESSOR_SUCCESS;

    GT_2trace (curTrace, GT_ENTER, "_DM8168DSP_badPageDump",
               phyAddr, pg);

    GT_assert (curTrace, (pg != NULL));

    Osal_printf("Bad page state in process \n"
                "page:%p flags:0x%0*lx mapping:%p mapcount:%d count:%d\n"
                "Backtrace:\n",
                pg, (Int)(2*sizeof(UInt32)),
                (UInt32)pg->flags, pg->mapping,
                page_mapcount(pg), page_count(pg));

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status;
}


/*!
 *  @brief      This function deletes an MMU entry for the specfied address and
 *              size.
 *
 *  @param      halObject   Pointer to the HAL object
 *  @param      slaveVirtAddr DSP virtual address of the memory region
 *  @param      size        Size of the memory region to be configured
 *  @param      isDynamic   Is the MMU entry being dynamically added?
 *
 *  @sa
 */
Int
_DM8168DSP_halMmuDeleteEntry (DM8168DSP_HalObject       * halObject,
                             DM8168DSP_HalMmuEntryInfo * entry)
{
    Int                         status      = PROCESSOR_SUCCESS;
    UInt32 *                    iopgd       = NULL;
    UInt32                      currentEntrySize;
    DM8168DSP_HalMmuEntryInfo    currentEntry;
    DM8168DSP_HalMmuObject *     mmuObj;
    UInt32                      clearBytes = 0;

    GT_2trace (curTrace, GT_ENTER, "_DM8168DSP_halMmuDeleteEntry",
               halObject, entry);

    GT_assert (curTrace, (halObject            != NULL));
    GT_assert (curTrace, (entry                != NULL));
    GT_assert (curTrace, (entry->size          != 0));

    mmuObj = &(halObject->mmuObj);

    /* Add the entry (or entries) */
    Memory_copy(&currentEntry,
                entry,
                sizeof(DM8168DSP_HalMmuEntryInfo));

    /* Align the addresses to page size */
    currentEntry.size += (currentEntry.slaveVirtAddr & (PAGE_SIZE_4KB -1));
    currentEntry.slaveVirtAddr &= ~(PAGE_SIZE_4KB-1);

    /* Align the size as well */
    currentEntry.size = MMUPAGE_ALIGN(currentEntry.size, PAGE_SIZE_4KB);
    currentEntrySize = currentEntry.size;

    /* To find the max. page size with which both PA & VA are
     * aligned */
    while ((currentEntrySize != 0)
            && (status >= 0)
            && (currentEntry.slaveVirtAddr > MMU_GLOBAL_MEMORY)) {
        if (currentEntrySize >= PAGE_SIZE_16MB
            && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_16MB - 1))) {
            currentEntry.size = PAGE_SIZE_16MB;
        }
        else if (currentEntrySize >= PAGE_SIZE_1MB
                 && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_1MB - 1))) {
            currentEntry.size = PAGE_SIZE_1MB;
        }
        else if (currentEntrySize >= PAGE_SIZE_64KB
                 && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_64KB - 1))){
            currentEntry.size = PAGE_SIZE_64KB;
        }
        else  if (currentEntrySize >= PAGE_SIZE_4KB
                 && !(currentEntry.slaveVirtAddr & (PAGE_SIZE_4KB - 1))){
            currentEntry.size = PAGE_SIZE_4KB;
        }
        else {
            Osal_printf ("Configuration error: "
                         " MMU entries must be aligned to their"
                         "page size(4KB,"
                         " 64KB, 1MB, or 16MB).\n");
            Osal_printf ("Since the addresses are not aligned buffer"
                         "of size: %x at address: %x cannot be  "
                         "TLB entries\n",
                       currentEntrySize, currentEntry.slaveVirtAddr);
            /*! @retval DM8168DSPPROC_E_MMUCONFIG Memory region is not
                                                 aligned to page size */
            status = DM8168DSPPROC_E_MMUCONFIG;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "_DM8168DSP_halMmuAddEntry",
                               status,
                          "Memory region is not aligned to page size!");
            break ;
        }
        /* DO NOT put this check under SYSLINK_BUILD_OPTIMIZE */
        if (status >= 0) {
            /* Check every page if exists */
            iopgd = iopgd_offset(mmuObj->dspMmuHandler,
                                 currentEntry.slaveVirtAddr);

            if (*iopgd) {
                /* Clear the requested page entry */
                clearBytes = iopgtable_clear_entry(mmuObj->dspMmuHandler,
                                      currentEntry.slaveVirtAddr);
            }

            currentEntry.slaveVirtAddr += currentEntry.size;
            currentEntrySize           -= currentEntry.size;
        }
    }

    GT_1trace (curTrace, GT_LEAVE, "_DM8168DSP_halMmuDeleteEntry", status);

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status;
}


/*!
 *  @brief      Updates entries in table.
 *
 *  @param      refData Argument provided to the ISR registration function
 *
 *  @sa
 */
Int
_DM8168DSP_halMmuPteSet (DM8168DSP_HalObject *      halObject,
                        DM8168DSP_HalMmuEntryInfo* setPteInfo)
{
    DM8168DSP_HalMmuObject *     mmuObj;
    struct iotlb_entry tlb_entry;
    Int    status = PROCESSOR_SUCCESS;

    GT_assert (curTrace, (halObject  != NULL));
    GT_assert (curTrace, (setPteInfo != NULL));

    mmuObj = &(halObject->mmuObj);
    GT_assert(curTrace, (mmuObj != NULL));

    switch (setPteInfo->size) {
        case PAGE_SIZE_16MB:
             tlb_entry.pgsz = MMU_CAM_PGSZ_16M;
             break;
        case PAGE_SIZE_1MB:
             tlb_entry.pgsz = MMU_CAM_PGSZ_1M;
             break;
        case PAGE_SIZE_64KB:
             tlb_entry.pgsz = MMU_CAM_PGSZ_64K;
             break;
        case PAGE_SIZE_4KB:
             tlb_entry.pgsz = MMU_CAM_PGSZ_4K;
             break;
        default :
            status = PROCESSOR_E_INVALIDARG;
            /*! @retval PROCESSOR_E_INVALIDARG Invalid Page size passed!. */
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "_DM8168DSP_halMmuPteSet",
                                 status,
                                 "Invalid Page size passed!");
    }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (status >= 0) {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        tlb_entry.prsvd  = MMU_CAM_PRESERVE;
        tlb_entry.valid  = MMU_CAM_VALID;
        /*TBD : elsz parameter has to be configured*/
        switch (setPteInfo->elementSize) {
            case ELEM_SIZE_8BIT:
                 tlb_entry.elsz = MMU_RAM_ELSZ_8;
                 break;
            case ELEM_SIZE_16BIT:
                 tlb_entry.elsz = MMU_RAM_ELSZ_16;
                 break;
            case ELEM_SIZE_32BIT:
                 tlb_entry.elsz = MMU_RAM_ELSZ_32;
                 break;
            case ELEM_SIZE_64BIT:
                 tlb_entry.elsz = 0x3; /* No translation */
                 break;
            default :
                status = PROCESSOR_E_INVALIDARG;
                /*! @retval PROCESSOR_E_INVALIDARG Invalid elementSize passed!*/
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_DM8168DSP_halMmuPteSet",
                                     status,
                                     "Invalid elementSize passed!");
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
        if (status >= 0) {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
            /*TBD : endian parameter has to configured*/
            switch (setPteInfo->endianism) {
                case LITTLE_ENDIAN:
                        tlb_entry.endian = MMU_RAM_ENDIAN_LITTLE;
                        break;
                case BIG_ENDIAN:
                        tlb_entry.endian = MMU_RAM_ENDIAN_BIG;
                        break;
                default :
                    status = PROCESSOR_E_INVALIDARG;
                    /*! @retval PROCESSOR_E_INVALIDARG Invalid endianism
                     *                                 passed!. */
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "_DM8168DSP_halMmuPteSet",
                                         status,
                                         "Invalid endianism passed!");
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status >= 0) {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
                /*TBD : mixed parameter has to configured*/
                switch (setPteInfo->mixedSize) {
                    case MMU_TLBES:
                            tlb_entry.mixed = MMU_RAM_DEFAULT;
                            break;
                    case MMU_CPUES:
                            tlb_entry.mixed = MMU_RAM_MIXED;
                            break;
                    default :
                        status = PROCESSOR_E_INVALIDARG;
                        /*! @retval PROCESSOR_E_INVALIDARG Invalid
                         *                                 mixed size passed!*/
                        GT_setFailureReason (curTrace,
                                             GT_4CLASS,
                                             "_DM8168DSP_halMmuPteSet",
                                             status,
                                             "Invalid mixed size passed!");
                }

                tlb_entry.da = setPteInfo->slaveVirtAddr;
                tlb_entry.pa = setPteInfo->masterPhyAddr;

                if (iopgtable_store_entry(mmuObj->dspMmuHandler, &tlb_entry)){
                    status = PROCESSOR_E_STOREENTERY;
                    GT_setFailureReason (curTrace,
                            GT_4CLASS,
                            "_DM8168DSP_halMmuPteSet",
                            status,
                            "iopgtable_store_entry failed!");
                }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            }
        }
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

//    GT_1trace (curTrace, GT_LEAVE, "_DM8168DSP_halMmuPteSet", status);

    /*! @retval PROCESSOR_SUCCESS Operation completed successfully. */
    return status;
}
#endif /* #if defined(SYSLINK_BUILDOS_LINUX) */


#if defined (__cplusplus)
}
#endif
