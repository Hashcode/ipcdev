/**
 *  @file   SysLink.h
 *
 *  @brief      This module contains common definitions, types, structures and
 *              functions used by SysLink.
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



#ifndef _SysLink_H_
#define _SysLink_H_


#if defined (__cplusplus)
extern "C" {
#endif

/* =============================================================================
 * macros & defines
 * =============================================================================
 */

/* The bulk of this file is wrapped in this doxygen INTERNAL as it's not intended
 * to be used by end users.  Most of this is implementation details that users
 * shouldn't see or bind to.
 */

/** @cond INTERNAL */

/**
 *  @const  IPC_BUFFER_ALIGN
 *
 *  @desc   Macro to align a number.
 *          x: The number to be aligned
 *          y: The value that the number should be aligned to.
 */
#define IPC_BUFFER_ALIGN(x, y) (UInt32)((UInt32)((x + y - 1) / y) * y)


/**
 *  @brief   Maximum number of mem entries for each core for one platform.
 */
#define SYSLINK_MAX_MEMENTRIES  32

/**
 *  @brief   Max name length.
 */
#define SYSLINK_MAX_NAMELENGTH  32

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

typedef UInt32 SysLink_MapMask;

/**
 *  @brief  Kernel Virtual address on master processor
 */
#define SysLink_MASTERKNLVIRT   (SysLink_MapMask)(1 << 0)

/**
 *  @brief  User Virtual address on master processor
 */
#define SysLink_MASTERUSRVIRT   (SysLink_MapMask)(1 << 1)

/**
 *  @brief  Virtual address on slave processor
 */
#define SysLink_SLAVEVIRT       (SysLink_MapMask)(1 << 2)

/**
 *  @brief  Enumeration of Client notifyMgr notification types.
 */
typedef enum SysLink_NotifyType_tag {
    SysLink_NOTIFICATION_NONE   = 0,
    /*!< No notification required*/
    SysLink_NOTIFICATION_ALWAYS = 1,
    /*!< Notify whenever the other client sends data/frees up space.*/
    SysLink_NOTIFICATION_ONCE   = 2,
    /*!< Notify when the other side sends data/frees up buffer. Once the
     *   notification is done, the notification is disabled until it is
     *   enabled again.
     */
    SysLink_NOTIFICATION_HDWRFIFO_ALWAYS = 3,
    /*!< Notify whenever the other side sends data/frees up space.
     *   This notification is never disabled.
     */
    SysLink_NOTIFICATION_HDWRFIFO_ONCE   = 4
    /*!< Notify when the other side sends data/frees up buffer. Once the
     *   notification is done, the notification is disabled until it is enabled
     *   again. The notification is enabled once the watermark is crossed and
     *   does not require buffer to get full/empty.
     */
} SysLink_NotifyType;

/**
 *  @brief  Structure for memEntry.
 */
typedef struct SysLink_MemEntry_tag {
    UInt32 slaveVirtAddr;    /**< Virtual address */
    UInt32 masterPhysAddr;   /**< Physical address */
    UInt32 size;             /**< Size of the entry */
    SysLink_MapMask mapMask; /**< Used for entries for which map is TRUE */
    Bool   map;              /**< Flag indicating whether this region should
                              *   be mapped to the slave MMU.
                              */
    Bool   isCached;         /**< Flag indicating whether the cache is enabled
                              *   for this region
                              */
    Bool   isValid;          /**< Specifies if the memEntry is valid */
} SysLink_MemEntry;

/**
 *  @brief  Structure for memEntry block for one core.
 */
typedef struct SysLink_MemEntry_Block_tag {
    Char             procName[SYSLINK_MAX_NAMELENGTH];
    /*!< Processor name for which entries are being defined*/
    UInt32           numEntries;
    /*!< Max memEntries for one core*/
    SysLink_MemEntry memEntries [SYSLINK_MAX_MEMENTRIES];
    /*!< Entire memory map (p->v) for one code*/
} SysLink_MemEntry_Block;


/**
 *  @brief  Structure for memEntry block for one core.
 */
typedef struct SysLink_MemoryMap_tag {
    UInt16                   numBlocks;
    /*!< Number of memory blocks in the memory map */
    SysLink_MemEntry_Block * memBlocks;
    /*!< Poninter to entire system memory map */
} SysLink_MemoryMap;

/** @endcond */

/**
 *  @brief      Config params override strings.
 *
 *  This string is a list of semi-colon-delimited "assignments" that can be set
 *  by users prior to the initial call to SysLink_setup() to affect system
 *  behavior.
 *
 *  Example assignments include:
 *    - "ProcMgr.proc[DSP].mmuEnable=FALSE;"
 *    - "ProcMgr.proc[VPSS-M3].mmuEnable=TRUE;"
 *    - "SharedRegion.entry[1].cacheEnable=FALSE;"
 *    - "SharedRegion.entry[3].cacheEnable=FALSE;"
 *
 *  @remarks    Note that many users don't explicitly set this string and
 *              rebuild their app, but rather leverage the @c SL_PARAMS
 *              environment variable to set this string's value.
 *
 *  @remarks    In many systems, slaveloader (or similar) is used to load
 *              and start the slaves, and is therefore the initial app
 *              in the system.  In those systems, it's important to set
 *              this variable (or @c SL_PARAMS) prior to running slaveloader.
 *
 *  @remarks    A common mistake is to forget to terminate the string with
 *              a trailing semi-colon.
 */
extern String SysLink_params;


/* =============================================================================
 * APIs
 * =============================================================================
 */
/**
 *  @brief      Function to initialize SysLink.
 *
 *              This function must be called in every user process before making
 *              calls to any other SysLink APIs.
 *
 *  @sa         SysLink_destroy()
 */
Void SysLink_setup (Void);

/**
 *  @brief      Function to finalize SysLink.
 *
 *              This function must be called in every user process at the end
 *              after all usage of SysLink in that process is complete.
 *
 *  @sa         SysLink_setup()
 */
Void SysLink_destroy (Void);


#if defined (__cplusplus)
}
#endif


#endif /*_SysLink_H_*/
