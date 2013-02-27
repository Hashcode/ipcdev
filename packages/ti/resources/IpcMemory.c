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
 *  ======== IpcMemory.c ========
 *
 */

#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>

#include <ti/resources/rsc_types.h>
#include "package/internal/IpcMemory.xdc.h"


/*
 *  ======== IpcMemory_getEntry ========
 */
IpcMemory_MemEntry *IpcMemory_getEntry(UInt index)
{
    UInt32 offset;
    UInt32 *type;
    IpcMemory_MemEntry *entry = NULL;
    IpcMemory_RscTable *table = (IpcMemory_RscTable *)
                                            (IpcMemory_module->pTable);

    if (index >= table->num) {
        return (NULL);
    }

    offset = (UInt32)((Char *)table + table->offset[index]);
    type = (UInt32 *)offset;
    if (*type == TYPE_CARVEOUT || *type == TYPE_DEVMEM) {
        entry = (IpcMemory_MemEntry *) ((Char *)offset);
    }

    return (entry);
}

/*
 *************************************************************************
 *                      Module wide functions
 *************************************************************************
 */

/*
 *  ======== IpcMemory_Module_startup ========
 */
Int IpcMemory_Module_startup(Int phase)
{
    IpcMemory_init();
    return (Startup_DONE);
}

/*
 *  ======== IpcMemory_virtToPhys ========
 */
Int IpcMemory_virtToPhys(UInt32 va, UInt32 *pa)
{
    UInt32 i;
    UInt32 offset;
    IpcMemory_MemEntry *entry;

    *pa = NULL;

    for (i = 0; i < module->pTable->num; i++) {
        entry = IpcMemory_getEntry(i);
        if (entry && va >= entry->da && va < (entry->da + entry->len)) {
                offset = va - entry->da;
                *pa = entry->pa + offset;
                return (IpcMemory_S_SUCCESS);
        }
    }

    return (IpcMemory_E_NOTFOUND);
}

/*
 *  ======== IpcMemory_physToVirt ========
 */
Int IpcMemory_physToVirt(UInt32 pa, UInt32 *va)
{
    UInt32 i;
    UInt32 offset;
    IpcMemory_MemEntry *entry;

    *va = NULL;

    for (i = 0; i < module->pTable->num; i++) {
        entry = IpcMemory_getEntry(i);
        if (entry && pa >= entry->pa && pa < (entry->pa + entry->len)) {
                offset = pa - entry->pa;
                *va = entry->da + offset;
                return (IpcMemory_S_SUCCESS);
        }
    }

    return (IpcMemory_E_NOTFOUND);
}
