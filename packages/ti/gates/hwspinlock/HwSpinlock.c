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
/*
 *  ======== HwSpinlock.c ========
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/IGateProvider.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Gate.h>

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

#include <ti/gates/hwspinlock/HwSpinlock.h>
#include "_HwSpinlock.h"


static HwSpinlock_Module_State HwSpinlock_module = { 0 };

/* Exposed to the Host side to reset hwspinlock if needed */
Bits32 ti_gates_HwSpinlock_sharedState
                            [(HwSpinlock_NUMLOCKS/sizeof(Bits32))] = { 0 };
const UInt32 ti_gates_HwSpinlock_numLocks = HwSpinlock_NUMLOCKS;


/* Helper functions to set and reset status of a lock */
inline Void _HwSpinlock_set(Int hwlockId)
{
    ti_gates_HwSpinlock_sharedState[hwlockId >> 5] |= (1 << (hwlockId % 32));
}

inline Void _HwSpinlock_clr(Int hwlockId)
{
    ti_gates_HwSpinlock_sharedState[hwlockId >> 5] &= ~(1 << (hwlockId % 32));
}

/*
 *************************************************************************
 *                       Instance functions
 *************************************************************************
 */

/*
 *  ======== HwSpinlock_Params_init ========
 */
Void HwSpinlock_Params_init(HwSpinlock_Params *params)
{
    params->id = HwSpinlock_NO_SPINLOCK;
    params->reqFxn= NULL;
    params->relFxn= NULL;
    params->arg1 = NULL;
    params->arg2 = NULL;
}

/*
 *  ======== HwSpinlock_create ========
 */
#define FXNN "HwSpinlock_create"
HwSpinlock_Handle HwSpinlock_create(HwSpinlock_Params *params)
{
    HwSpinlock_Handle handle;
    Int i;
    IArg key;

    /* Call request hook fxn, return (NULL handle) if the hook fxn fails */
    if (params->reqFxn) {
        if (params->reqFxn(params)) {
            return NULL;
        }
    }

    /* Check for a valid hwspinlock id */
    if ((params->id < 0) || (params->id >= HwSpinlock_NUMLOCKS)) {
        Log_error0(FXNN": HwSpinlock Id is not valid");
        return NULL;
    }

    /* Return handle if already created */
    key = Gate_enterSystem();
    handle = HwSpinlock_module.locks[params->id];
    if (handle) {
        handle->refCnt++;
        Gate_leaveSystem(key);
        return handle;
    }

    /* Allocate handle */
    handle = Memory_alloc(NULL, sizeof(*handle), 0, NULL);
    if (!handle) {
        Log_error0(FXNN": Unable to allocate handle");
        Gate_leaveSystem(key);
        return NULL;
    }

    /* Create the preemption gates */
    for (i = 0; i < HwSpinlock_NUMPREEMPTGATES; i++) {
        handle->preemptGates[i] = (IGateProvider_Handle)
                                    HwSpinlock_GateFxns[i].create(NULL, NULL);
    }
    /* Create the local gate */
    handle->mutex = GateMutexPri_create(NULL, NULL);

    /* Store private info in the handle */
    handle->params = *params;
    handle->baseAddr = (volatile UInt32 *)HwSpinlock_BASEADDR;
    handle->refCnt = 1;
    handle->state = HwSpinlock_STATE_FREE;

    /* Store the created handle */
    HwSpinlock_module.locks[params->id] = handle;
    Gate_leaveSystem(key);

    return handle;
}
#undef FXNN

/*
 *  ======== HwSpinlock_delete ========
 */
#define FXNN "HwSpinlock_delete"
Int HwSpinlock_delete(HwSpinlock_Handle handle)
{
    Int i;
    IArg key;

    Assert_isTrue(handle, NULL);
    key = Gate_enterSystem();

    /* Decrement reference counter */
    if (handle->refCnt) {
        handle->refCnt--;
    }

    if (!handle->refCnt) {
        /* Return error if trying to delete before unlock */
        if (handle->state == HwSpinlock_STATE_TAKEN) {
            Log_error0(FXNN": Error: HwSpinlock still in use");
            Gate_leaveSystem(key);
            return HwSpinlock_STILL_IN_USE;
        }
        /* Call release hook fxn if exists */
        if (handle->params.relFxn) {
            handle->params.relFxn(&handle->params);
         }

        /* Delete and reset the preemption gates */
        for (i = 0; i < HwSpinlock_NUMPREEMPTGATES; i++) {
            HwSpinlock_GateFxns[i].delete(&handle->preemptGates[i]);
            handle->preemptGates[i] = NULL;
        }
        /* Delete and reset the local gate */
        GateMutexPri_delete(&handle->mutex);
        handle->mutex = NULL;

        /* Remove the handle from the module */
        HwSpinlock_module.locks[handle->params.id] = NULL;

        /* Release the allocated memory for the handle */
        Memory_free(NULL, handle, sizeof(*handle));
        handle = NULL;
    }
    Gate_leaveSystem(key);

    return HwSpinlock_S_SUCCESS;
}
#undef FXNN

/*
 *  ======== HwSpinlock_enter ========
 */
Int HwSpinlock_enter(HwSpinlock_Handle handle, HwSpinlock_PreemptGate pType,
                     UInt timeout, HwSpinlock_Key *hkey)
{
    UInt start, elapsed;

    Assert_isTrue(handle, NULL);

    /* Store Start TimeStamp */
    if (timeout != HwSpinlock_WAIT_FOREVER) {
        start = Clock_getTicks();
    }

    while(1) {
        /* Enter the localGate protection */
        handle->mkey = GateMutexPri_enter(handle->mutex);

        /* Try to get the hwspinlock */
        if (handle->baseAddr[handle->params.id] == 0) {
            handle->state = HwSpinlock_STATE_TAKEN;
            /* Disable preemption of pType and store the type in handle */
            hkey->key = IGateProvider_enter(handle->preemptGates[pType]);
            hkey->valid = TRUE;
            handle->pType = pType;
            _HwSpinlock_set(handle->params.id);
            return (HwSpinlock_S_SUCCESS);
        }

        /* If is already taken undo localGate protection */
        GateMutexPri_leave(handle->mutex, handle->mkey);

        /* Check if timeout has elapsed */
        if (timeout != HwSpinlock_WAIT_FOREVER) {
            elapsed = Clock_getTicks() - start;
            if (elapsed > timeout) {
                /* Mark key as invalid */
                hkey->valid = FALSE;
                return (HwSpinlock_E_TIMEOUT);
            }
        }
    }
}

/*
 *  ======== HwSpinlock_leave ========
 */
Void HwSpinlock_leave(HwSpinlock_Handle handle, HwSpinlock_Key *hkey)
{
    Assert_isTrue(handle, NULL);

    /* Only unlock hwspinlock if it was really acquired and isa valid key */
    if (handle->state == HwSpinlock_STATE_TAKEN && hkey->valid) {
       /* Leave the spinlock */
        handle->baseAddr[handle->params.id] = 0;
        handle->state = HwSpinlock_STATE_FREE;
        _HwSpinlock_clr(handle->params.id);

        /* Restore Preemption of pType */
        IGateProvider_leave(handle->preemptGates[handle->pType], hkey->key);
        hkey->valid = FALSE;

        /* Leave the mutex */
        GateMutexPri_leave(handle->mutex, handle->mkey);
    }
}

/*
 *  ======== HwSpinlock_getId ========
 */
Int HwSpinlock_getId(HwSpinlock_Handle handle)
{
    Assert_isTrue(handle, NULL);
    return (handle->params.id);
}

/*
 *  ======== HwSpinlock_getState ========
 */
HwSpinlock_State HwSpinlock_getState(HwSpinlock_Handle handle)
{
    Assert_isTrue(handle, NULL);
    return (handle->state);
}
