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

/*
 *  ======== MmRpc.c ========
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "MmRpc.h"

/*
 *  ======== MmRpc_Object ========
 */
typedef struct {
    int         fd;             /* device file descriptor */
} MmRpc_Object;

/*
 *  ======== MmRpc_Params_init ========
 */
void MmRpc_Params_init(MmRpc_Params *params)
{
    params->reserved = 0;
}

/*
 *  ======== MmRpc_create ========
 */
MmRpc_Handle MmRpc_create(const char *proc, const char *service,
        const MmRpc_Params *params)
{
    int                 status = MmRpc_S_SUCCESS;
    MmRpc_Object *      obj;

    printf("MmRpc_create: -->\n");

    /* allocate the instance object */
    obj = (MmRpc_Object *)calloc(1, sizeof(MmRpc_Object));

    if (obj == NULL) {
        status = MmRpc_E_FAIL;
        goto leave;
    }

    /* open the driver */
    obj->fd = open("/dev/rpc_example", O_RDWR);

    if (obj->fd < 0) {
        printf("MmRpc_create: Error: open failed\n");
        status = MmRpc_E_FAIL;
        goto leave;
    }

leave:
    if (status < 0) {
        if ((obj != NULL) && (obj->fd >= 0)) {
            close(obj->fd);
        }
        if (obj != NULL) {
            free(obj);
        }
    }

    printf("MmRpc_create: <--\n");
    return((MmRpc_Handle)obj);
}

/*
 *  ======== MmRpc_delete ========
 */
int MmRpc_delete(MmRpc_Handle *handlePtr)
{
    int status = MmRpc_S_SUCCESS;
    MmRpc_Object *obj;

    printf("MmRpc_delete: -->\n");
    obj = (MmRpc_Object *)(*handlePtr);

    /* close the device */
    if ((obj != NULL) && (obj->fd >= 0)) {
        close(obj->fd);
    }

    /* free the instance object */
    free((void *)(*handlePtr));
    *handlePtr = NULL;

    printf("MmRpc_delete: <--\n");
    return(status);
}
