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
/** ============================================================================
 *  @file       _HwSpinlock.h
 *  ============================================================================
 */

#ifndef ti_gates__HwSpinlock__include
#define ti_gates__HwSpinlock__include

#if defined (__cplusplus)
extern "C" {
#endif

#include <xdc/runtime/GateNull.h>
#include <ti/sysbios/gates/GateMutexPri.h>
#include <ti/sysbios/gates/GateHwi.h>
#include <ti/sysbios/gates/GateSwi.h>
#include <ti/sysbios/gates/GateTask.h>


/* =============================================================================
 *  Structures & Definitions
 * =============================================================================
 */

typedef Void * (*HwSpinlock_createFxn)(IGateProvider_Params *, Error_Block *);
typedef Void * (*HwSpinlock_deleteFxn)(IGateProvider_Handle *);

/*!
 *  @brief  HwSpinlock_PreemptGateFxn
 */
typedef struct {
    HwSpinlock_createFxn create;
    HwSpinlock_deleteFxn delete;
} HwSpinlock_PreemptGateFxn;

/*!
 *  @brief HwSpinlock GateFxns
 */
HwSpinlock_PreemptGateFxn HwSpinlock_GateFxns[] = {
    {(HwSpinlock_createFxn)GateNull_create,
                                        (HwSpinlock_deleteFxn)GateNull_delete},
    {(HwSpinlock_createFxn)GateHwi_create,
                                        (HwSpinlock_deleteFxn)GateHwi_delete},
    {(HwSpinlock_createFxn)GateSwi_create,
                                        (HwSpinlock_deleteFxn)GateSwi_delete},
    {(HwSpinlock_createFxn)GateTask_create,
                                        (HwSpinlock_deleteFxn)GateTask_delete},
};

#ifdef DSPC674
#define HwSpinlock_NUMLOCKS         128
#define HwSpinlock_BASEADDR         0x480CA800
#else
#define HwSpinlock_NUMLOCKS         32
#ifdef DSP
#define HwSpinlock_BASEADDR         0x4A0F6800
#else
#define HwSpinlock_BASEADDR         0xAA0F6800
#endif
#endif
#define HwSpinlock_NUMPREEMPTGATES  (sizeof(HwSpinlock_GateFxns) / \
                                        sizeof(HwSpinlock_PreemptGateFxn))


/*!
 *  @brief  HwSpinlock_Module_State type
 */
typedef struct {
    HwSpinlock_Handle        locks[HwSpinlock_NUMLOCKS];
} HwSpinlock_Module_State;

/*!
 *  @brief  HwSpinlock_Object type
 */
struct HwSpinlock_Object {
    HwSpinlock_Params        params;
    volatile UInt32          *baseAddr;
    Int                      lockNum;   /* The lock number being used */
    HwSpinlock_PreemptGate   pType;
    IArg                     mkey;
    IGateProvider_Handle     preemptGates[HwSpinlock_NUMPREEMPTGATES];
    GateMutexPri_Handle      mutex;
    Int                      refCnt;
    HwSpinlock_State         state;
};

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ti_gates__HwSpinlock__include */
