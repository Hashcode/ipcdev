/*
 * Copyright (c) 2013, Texas Instruments Incorporated
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
 *  ======== mmrpc_test.c ========
 */
#include <stdio.h>

#include <ti/ipc/mm/MmRpc.h>


/* module 'Mx' functions */
int Mx_initialize(void);
void Mx_finalize(void);

int32_t Mx_triple(uint32_t a);
int32_t Mx_add(int32_t a, int32_t b);

MmRpc_Handle Mx_rpcDsp = NULL;

/* static function indicies */
#define Mx_Fxn_triple   (0x80000000 | 1)
#define Mx_Fxn_add      (0x80000000 | 2)


/*
 *  ======== main ========
 */
int main(int argc, char **argv)
{
    int status;
    int32_t ret;

    printf("mmrpc_test: --> main\n");

    /* initialize Mx module (setup rpc connection) */
    status = Mx_initialize();

    if (status < 0) {
        goto leave;
    }

    /* invoke Mx functions */
    ret = Mx_triple(11);
    printf("mmrpc_test: Mx_triple(11), ret=%d\n", ret);

    if (ret < 0) {
        status = -1;
        goto leave;
    }

    ret = Mx_triple(111);
    printf("mmrpc_test: Mx_triple(111), ret=%d\n", ret);

    if (ret < 0) {
        status = -1;
        goto leave;
    }

    ret = Mx_add(44, 66);
    printf("mmrpc_test: Mx_add(44, 66), ret=%d\n", ret);

    if (ret < 0) {
        status = -1;
        goto leave;
    }

leave:
    /* finalize Mx module (destroy rpc connection) */
    Mx_finalize();

    if (status < 0) {
        printf("mmrpc_test: FAILED\n");
    }
    else {
        printf("mmrpc_test: PASSED\n");
    }
    return(0);
}

/*
 *  ======== Mx_initialize ========
 */
int Mx_initialize(void)
{
    int status;
    MmRpc_Params params;

    /* create remote server insance */
    MmRpc_Params_init(&params);

    status = MmRpc_create("DSP", "FooServer", &params, &Mx_rpcDsp);

    if (status < 0) {
        printf("mmrpc_test: Error: MmRpc_create failed\n");
        status = -1;
    }
    else {
        status = 0;
    }

    return(status);
}

/*
 *  ======== Mx_finalize ========
 */
void Mx_finalize(void)
{
    /* delete remote server instance */
    if (Mx_rpcDsp != NULL) {
        MmRpc_delete(&Mx_rpcDsp);
    }
}

/*
 *  ======== Mx_triple ========
 */
int32_t Mx_triple(uint32_t a)
{
    MmRpc_FxnCtx *fxnCtx;
    int32_t fxnRet;
    char send_buf[512] = {0};
    int status;

    /* marshall function arguments into the send buffer */
    fxnCtx = (MmRpc_FxnCtx *)send_buf;

    fxnCtx->fxn_id = Mx_Fxn_triple;
    fxnCtx->num_params = 1;
    fxnCtx->params[0].type = MmRpc_ParamType_Atomic;
    fxnCtx->params[0].param.atomic.size = sizeof(int);
    fxnCtx->params[0].param.atomic.data = a;
    fxnCtx->num_translations = 0;

    /* invoke the remote function call */
    status = MmRpc_call(Mx_rpcDsp, fxnCtx, &fxnRet);

    if (status < 0) {
        printf("mmrpc_test: Error: MmRpc_call failed\n");
        fxnRet = -1;
    }

    return(fxnRet);
}

/*
 *  ======== Mx_add ========
 */
int32_t Mx_add(int32_t a, int32_t b)
{
    MmRpc_FxnCtx *fxnCtx;
    int32_t fxnRet;
    char send_buf[512] = {0};
    int status;

    /* marshall function arguments into the send buffer */
    fxnCtx = (MmRpc_FxnCtx *)send_buf;

    fxnCtx->fxn_id = Mx_Fxn_add;
    fxnCtx->num_params = 2;
    fxnCtx->params[0].type = MmRpc_ParamType_Atomic;
    fxnCtx->params[0].param.atomic.size = sizeof(int);
    fxnCtx->params[0].param.atomic.data = a;
    fxnCtx->params[1].type = MmRpc_ParamType_Atomic;
    fxnCtx->params[1].param.atomic.size = sizeof(int);
    fxnCtx->params[1].param.atomic.data = b;
    fxnCtx->num_translations = 0;

    /* invoke the remote function call */
    status = MmRpc_call(Mx_rpcDsp, fxnCtx, &fxnRet);

    if (status < 0) {
        printf("mmrpc_test: Error: MmRpc_call failed\n");
        fxnRet = -1;
    }

    return(fxnRet);
}
