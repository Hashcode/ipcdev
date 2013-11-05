/*
 * Copyright (c) 2012-2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 *  @file       ti/ipc/MultiProc.h
 *
 *  @brief      Processor ID Manager
 *
 *  Many IPC modules require the ability to uniquely specify and identify
 *  processors in a multi-processor environment. The MultiProc module
 *  centralizes processor id management into one module.  Since this
 *  configuration is almost always universally required, most IPC applications
 *  require supplying configuration of this module.
 *
 *  Each processor in the MultiProc module may be uniquely identified by
 *  either a name string or an integer ranging from 0 to NUMPROCESSORS - 1.
 *
 *  At runtime, the MultiProc_getId() call returns the MultiProc id for any
 *  processor given its name.
 *
 *  The MultiProc header should be included in an application as follows:
 *  @code
 *  #include <ti/ipc/MultiProc.h>
 *  @endcode
 */

#ifndef ti_ipc_MultiProc__include
#define ti_ipc_MultiProc__include

#if defined (__cplusplus)
extern "C" {
#endif

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */

/*!
 *  @brief  The resource is still in use
 */
#define MultiProc_S_BUSY                 (2)

/*!
 *  @brief  The module has been already setup
 */
#define MultiProc_S_ALREADYSETUP         (1)

/*!
 *  @brief  Operation is successful.
 */
#define MultiProc_S_SUCCESS              (0)

/*!
 *  @brief  Generic failure.
 */
#define MultiProc_E_FAIL                (-1)

/*!
 *  @brief  Argument passed to function is invalid.
 */
#define MultiProc_E_INVALIDARG          (-2)

/*!
 *  @brief  Operation resulted in memory failure.
 */
#define MultiProc_E_MEMORY              (-3)

/*!
 *  @brief  The specified entity already exists.
 */
#define MultiProc_E_ALREADYEXISTS       (-4)

/*!
 *  @brief  Unable to find the specified entity.
 */
#define MultiProc_E_NOTFOUND            (-5)

/*!
 *  @brief  Operation timed out.
 */
#define MultiProc_E_TIMEOUT             (-6)

/*!
 *  @brief  Module is not initialized.
 */
#define MultiProc_E_INVALIDSTATE        (-7)

/*!
 *  @brief  A failure occurred in an OS-specific call
 */
#define MultiProc_E_OSFAILURE           (-8)

/*!
 *  @brief  Specified resource is not available
 */
#define MultiProc_E_RESOURCE            (-9)

/*!
 *  @brief  Operation was interrupted. Please restart the operation
 */
#define MultiProc_E_RESTART             (-10)

/* =============================================================================
 *  Macros
 * =============================================================================
 */

/*!
 *  @brief  Invalid processor id.
 */
#define MultiProc_INVALIDID             (0xFFFF)

/* =============================================================================
 *  MultiProc Module-wide Functions
 * =============================================================================
 */

 /*!
 *  @brief      Gets the base MultiProc id of the cluster
 *
 *  Retrieves the base MultiProc id for the cluster of processors.
 *
 *  @return     MultiProc id for base of cluster
 *
 *  @sa         MultiProc_getClusterId
 */
UInt16 MultiProc_getBaseIdOfCluster(Void);

/*!
 *  @brief      Gets the MultiProc id
 *
 *  Retrieves the MultiProc id for the processor with corresponding MultiProc
 *  name. #MultiProc_INVALIDID is returned if the name was not found.
 *
 *  @param      name  Name of the processor.
 *
 *  @return     MultiProc id
 *
 *  @sa         MultiProc_getName
 */
UInt16 MultiProc_getId(String name);

/*!
 *  @brief      Gets the name of a processor
 *
 *  @param      id  MultiProc id.
 *
 *  @return     Name of the processor
 *
 *  The returned string should never be modified.
 *
 *  @sa         MultiProc_getId
 */
String MultiProc_getName(UInt16 id);

/*!
 *  @brief      Gets the number of processors
 *
 *  @return     Number of processors configured with MultiProc
 */
UInt16 MultiProc_getNumProcessors(Void);

/*!
 *  @brief      Gets the number of processors in the cluster
 *
 *  @return     Number of processors in cluster
 */
UInt16 MultiProc_getNumProcsInCluster(Void);

/*!
 *  @brief      Gets executing processor's MultiProc id
 *
 *  @return     Executing processor's id
 *
 *  @sa         MultiProc_getId
 */
UInt16 MultiProc_self(Void);

/*!
 *  @brief      Sets executing processor's MultiProc id
 *
 *  @param      id  MultiProc id
 *
 *  @return     MultiProc status:
 *              - #MultiProc_S_SUCCESS: MultiProc id successfully set
 *              - #MultiProc_E_FAIL:    MultiProc id cannot be set at this time
 */
Int MultiProc_setLocalId(UInt16 id);

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ti_ipc_MultiProc__include */
