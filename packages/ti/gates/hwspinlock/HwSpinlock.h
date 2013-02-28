/*
 * Copyright (c) 2011-2013, Texas Instruments Incorporated
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

#ifndef ti_gates_HwSpinlock__include
#define ti_gates_HwSpinlock__include

#if defined (__cplusplus)
extern "C" {
#endif

extern UInt32 ti_gates_HwSpinlock_sharedState[];
extern const UInt32 ti_gates_HwSpinlock_numLocks;

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */

/*!
 *  @def    HwSpinlock_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define HwSpinlock_S_SUCCESS            0

/*!
 *  @def    HwSpinlock_NO_SPINLOCK
 *  @brief  Not hwspinlock Id associated to the obj.
 */
#define HwSpinlock_NO_SPINLOCK         -1

/*!
 *  @def    HwSpinlock_E_FAIL
 *  @brief  Generic failure.
 */
#define HwSpinlock_E_FAIL              -2

/*!
 *  @def    HwSpinlock_E_TIMEOUT
 *  @brief  Operation timed out.
 */
#define HwSpinlock_E_TIMEOUT           -3

/*!
 *  @def    HwSpinlock_STILL_IN_USE
 *  @brief  handle is still in use..
 */
#define HwSpinlock_STILL_IN_USE        -4

/*!
 *  @def    HwSpinlock_WAIT_FOREVER
 *  @brief  Use as a NOT timeout option.
 */
#define HwSpinlock_WAIT_FOREVER        0xFFFFFFFF


/* =============================================================================
 *  Structures & Enums
 * =============================================================================
 */

/*!
 *  @brief  HwSpinlock state
 */
typedef enum {
    HwSpinlock_STATE_FREE,
    HwSpinlock_STATE_TAKEN
} HwSpinlock_State;

/*!
 *  @brief  A set of local context protection levels
 *          - HwSpinlock_PreemptGate_HWI  -> GateHwi: disables interrupts
 *          - HwSpinlock_PreemptGate_SWI  -> GateSwi: disables Swi's (software interrupts)
 *          - HwSpinlock_PreemptGate_TASK -> GateTask: disables Task's preemption
 */
typedef enum HwSpinlock_PreemptGate {
    HwSpinlock_PreemptGate_NONE   = 0,  /* Use no local protection */
    HwSpinlock_PreemptGate_HWI    = 1,  /* Use the INTERRUPT local protection level */
    HwSpinlock_PreemptGate_SWI    = 2,  /* Use the SWI local protection level */
    HwSpinlock_PreemptGate_TASK   = 3   /* Use the TASK local protection level */
} HwSpinlock_PreemptGate;

typedef Void * (*HwSpinlock_hookFxn)(Void *);

/*!
 *  @brief  HwSpinlock_Params
 */
typedef struct HwSpinlock_Params {
    Int                   id;
    HwSpinlock_hookFxn    reqFxn;
    HwSpinlock_hookFxn    relFxn;
    Void                  *arg1;
    Void                  *arg2;
} HwSpinlock_Params;

/*!
 *  @brief  HwSpinlock_Key
 */
typedef struct HwSpinlock_Key {
    IArg key;
    Bool valid;
} HwSpinlock_Key;

/*!
 *  @brief  HwSpinlock_Handle type
 */
typedef struct HwSpinlock_Object *HwSpinlock_Handle;


/* =============================================================================
 *  HwSpinlock Interface Functions
 * =============================================================================
 */

/*!
 *  @brief      HwSpinlock_Params_init
 *
 *  @param[in]  HwSpinlock_Params
 *
 *  @return     Void
 */
Void HwSpinlock_Params_init(HwSpinlock_Params *params);

/*!
 *  @brief      Create spinlock
 *
 *  @param[in]  resourceId, baseAddr
 *
 *  @return     HwSpinlock_Handle
 */
HwSpinlock_Handle HwSpinlock_create(HwSpinlock_Params *params);

/*!
*  @brief      HwSpinlock_delete
*
*  @param[in]  HwSpinlock_Handle
*
*  @return     Status
*/
Int HwSpinlock_delete(HwSpinlock_Handle handle);

/*!
 *  @brief      Enter/Acquire the HwSpinlock
 *
 *  @param[in]  timeout, local protecttion option
 *
 *  @return     Status
 */
Int HwSpinlock_enter(HwSpinlock_Handle handle, HwSpinlock_PreemptGate pType,
                     UInt timeout, HwSpinlock_Key *key);

/*!
 *  @brief      Leave the HwSpinlock
 *
 *  @param[in]  HwSpinlock handle
 *
 *  @return     Void
 */
Void HwSpinlock_leave(HwSpinlock_Handle handle, HwSpinlock_Key *key);

/*!
 *  @brief      HwSpinlock_getId
 *
 *  @param[in]  HwSpinlock handle
 *
 *  @return     Spinlock Id
 */
Int HwSpinlock_getId(HwSpinlock_Handle handle);

/*!
 *  @brief      HwSpinlock_getState
 *
 *  @param[in]  HwSpinlock handle
 *
 *  @return     Current State
 */
HwSpinlock_State HwSpinlock_getState(HwSpinlock_Handle handle);

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ti_gates_HwSpinlock__include */
