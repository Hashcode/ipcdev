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
 *  ======== OffloadM3.c ========
 *  Offload some algorithms from AppM3 to SysM3 due to lack of MIPS on AppM3
 *
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/knl/Thread.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/ipc/MultiProc.h>

#define RP_MSG_OFFLOAD_BASE 0xF0F0F0F0

enum {
    OFFLOAD_CREATE_IDX  = 0,
    OFFLOAD_DELETE_IDX  = 1,
    OFFLOAD_AC_IDX      = 2,
    OFFLOAD_VS_IDX      = 3,
    OFFLOAD_MM_IDX      = 4,
    OFFLOAD_MAX_IDX     = 5
};

typedef struct {
    Void    (*fxn)(Void *);
    Void    *data;
    Int     status;
} MSP_PAYLOAD_TYPE;

typedef struct {
    Bool             created;
    Semaphore_Handle mutex;
    Task_Handle      tasks[OFFLOAD_MAX_IDX];
    Semaphore_Handle semaphores[OFFLOAD_MAX_IDX];
} OffloadM3_module;

#pragma DATA_SECTION(interm3PayloadAC, ".interm3offload")
MSP_PAYLOAD_TYPE interm3PayloadAC;
#pragma DATA_SECTION(interm3PayloadVS, ".interm3offload")
MSP_PAYLOAD_TYPE interm3PayloadVS;
#pragma DATA_SECTION(interm3PayloadMM, ".interm3offload")
MSP_PAYLOAD_TYPE interm3PayloadMM;

static OffloadM3_module module;
static UInt16 sysm3ProcId;

/*!
 * ======== OffloadM3_processSysM3Tasks ========
 *
 * Process the different Offload request messages on SysM3. Signal the
 * appropriate tasks for executing the functions to be offloaded.
 */
Int OffloadM3_processSysM3Tasks(UArg msg)
{
    UInt index = (UInt)msg - RP_MSG_OFFLOAD_BASE;
    if (index >= OFFLOAD_MAX_IDX)
        return 0;

    Semaphore_post(module.semaphores[index]);
    return 1;
}

static Void inline Mutex_enter()
{
    Semaphore_pend(module.mutex, BIOS_WAIT_FOREVER);
}

static Void inline Mutex_leave()
{
    Semaphore_post(module.mutex);
}

static Void OffloadM3_workerTask(UArg sem, UArg fxn)
{
    Semaphore_Handle semHandle = (Semaphore_Handle)sem;
    MSP_PAYLOAD_TYPE *payload = (MSP_PAYLOAD_TYPE *)fxn;

    while(1) {
        Semaphore_pend(semHandle, BIOS_WAIT_FOREVER);
        payload->fxn(payload->data);
        payload->status = 0;
    }
}

static Void OffloadM3_createTask(UArg sem, UArg arg1)
{
    Semaphore_Handle semHandle = (Semaphore_Handle)sem;
    Task_Params params;
    Int i;

    while (1) {
        Semaphore_pend(semHandle, BIOS_WAIT_FOREVER);

        Mutex_enter();
        if (!module.created) {
            for (i = OFFLOAD_AC_IDX; i < OFFLOAD_MAX_IDX; i++) {
                module.semaphores[i] = Semaphore_create(0, NULL, NULL);
            }

            Task_Params_init(&params);
            params.instance->name = "workerTaskAC";
            params.priority = xdc_runtime_knl_Thread_Priority_NORMAL;
            params.arg0 = (UArg)module.semaphores[OFFLOAD_AC_IDX];
            params.arg1 = (UArg)&interm3PayloadAC;
            module.tasks[OFFLOAD_AC_IDX] = Task_create(OffloadM3_workerTask, &params, NULL);

            Task_Params_init(&params);
            params.instance->name = "workerTaskVS";
            params.priority = xdc_runtime_knl_Thread_Priority_NORMAL;
            params.arg0 = (UArg)module.semaphores[OFFLOAD_VS_IDX];
            params.arg1 = (UArg)&interm3PayloadVS;
            module.tasks[OFFLOAD_VS_IDX] = Task_create(OffloadM3_workerTask, &params, NULL);

            Task_Params_init(&params);
            /* Create a lower priority thread */
            params.instance->name = "workerTaskMM";
            params.priority = xdc_runtime_knl_Thread_Priority_BELOW_NORMAL;
            params.arg0 = (UArg)module.semaphores[OFFLOAD_MM_IDX];
            params.arg1 = (UArg)&interm3PayloadMM;
            module.tasks[OFFLOAD_MM_IDX] = Task_create(OffloadM3_workerTask, &params, NULL);

            module.created = TRUE;
        }
        Mutex_leave();
    }
}

static Void OffloadM3_deleteTask(UArg sem, UArg arg1)
{
    Semaphore_Handle semHandle = (Semaphore_Handle)sem;
    Int i;

    while (1) {
        Semaphore_pend(semHandle, BIOS_WAIT_FOREVER);

        Mutex_enter();
        if (module.created) {
            /*
             * Since this task is higher priority than any of the subtasks,
             * they won't be in running state, so we can call Task_delete on
             * them
             */
            for (i = OFFLOAD_AC_IDX; i < OFFLOAD_MAX_IDX; i++) {
                Task_delete(&module.tasks[i]);
                Semaphore_delete(&module.semaphores[i]);
                module.tasks[i] = NULL;
                module.semaphores[i] = NULL;
            }
            module.created = FALSE;
        }
        Mutex_leave();
    }
}

/*!
 * ======== OffloadM3_init ========
 *
 * Initialize the OffloadM3 module, currently supported only on SysM3
 */
Void OffloadM3_init()
{
    Task_Params taskParams;
    Semaphore_Params mutexParams;

    sysm3ProcId = MultiProc_getId("CORE0");
    if (MultiProc_self() != sysm3ProcId)
        return;

    Semaphore_Params_init(&mutexParams);
    mutexParams.mode = Semaphore_Mode_BINARY;
    module.mutex = Semaphore_create(1, NULL, NULL);

    module.created = FALSE;
    module.semaphores[OFFLOAD_CREATE_IDX] = Semaphore_create(0, NULL, NULL);
    module.semaphores[OFFLOAD_DELETE_IDX] = Semaphore_create(0, NULL, NULL);

    Task_Params_init(&taskParams);
    taskParams.instance->name = "OffloadM3_createTask";
    taskParams.priority = xdc_runtime_knl_Thread_Priority_NORMAL;
    taskParams.arg0 = (UArg)module.semaphores[OFFLOAD_CREATE_IDX];
    module.tasks[OFFLOAD_CREATE_IDX] = Task_create(OffloadM3_createTask, &taskParams, NULL);

    Task_Params_init(&taskParams);
    taskParams.instance->name = "OffloadM3_deleteTask";
    taskParams.priority = xdc_runtime_knl_Thread_Priority_HIGHEST;
    taskParams.arg0 = (UArg)module.semaphores[OFFLOAD_DELETE_IDX];
    module.tasks[OFFLOAD_DELETE_IDX] = Task_create(OffloadM3_deleteTask, &taskParams, NULL);
}

/*!
 * ======== OffloadM3_exit ========
 *
 * Finalize the OffloadM3 module, currently supported only on SysM3
 */
Void OffloadM3_exit()
{
    if (MultiProc_self() != sysm3ProcId)
        return;

    Task_delete(&module.tasks[OFFLOAD_CREATE_IDX]);
    Task_delete(&module.tasks[OFFLOAD_DELETE_IDX]);

    Semaphore_delete(&module.semaphores[OFFLOAD_CREATE_IDX]);
    Semaphore_delete(&module.semaphores[OFFLOAD_DELETE_IDX]);
    Semaphore_delete(&module.mutex);
}
