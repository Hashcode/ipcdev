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

#if defined(SYSLINK_BUILDOS_QNX)
#include <ti/shmemallocator/SharedMemoryAllocatorUsr.h>
#endif

/*
 *  module 'Mx' functions
 */

/* compute structure */
typedef struct {
    uint32_t    coef;
    int         key;
    int         size;
    uint32_t *  inBuf;
    uint32_t *  outBuf;
} Mx_Compute;

int Mx_initialize(void);
void Mx_finalize(void);

int32_t Mx_triple(uint32_t a);
int32_t Mx_add(int32_t a, int32_t b);
int32_t Mx_compute(Mx_Compute *compute);

MmRpc_Handle Mx_rpcIpu = NULL;

/* static function indicies */
#define Mx_Fxn_triple   (0x80000000 | 1)
#define Mx_Fxn_add      (0x80000000 | 2)
#define Mx_Fxn_compute  (0x80000000 | 5)

#define Mx_OFFSET(base, member) ((uint_t)(member) - (uint_t)(base))

/*
 *  ======== main ========
 */
int main(int argc, char **argv)
{
    int status;
    int i;
    int32_t ret;
    uint32_t val;
    Mx_Compute *compute;
#if defined(SYSLINK_BUILDOS_QNX)
    shm_buf shmCompute, shmInBuf, shmOutBuf;
#endif

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

    /* allocate a compute structure in shared memory */
#if defined(SYSLINK_BUILDOS_QNX)
    SHM_alloc(sizeof(Mx_Compute), &shmCompute);
    compute = (Mx_Compute *)(shmCompute.vir_addr);
#else
    compute = NULL;
#endif

    if (compute == NULL) {
        /* temporary: memory alloc not implemented on Linux */
        goto leave;
    }

    /* initialize compute structure */
    compute->coef = 0x80400000;
    compute->key = 0xABA0;
    compute->size = 0x1000;
    compute->inBuf = NULL;
    compute->outBuf = NULL;

    /* allocate an input buffer in shared memory */
#if defined(SYSLINK_BUILDOS_QNX)
    SHM_alloc(compute->size * sizeof(uint32_t), &shmInBuf);
    compute->inBuf = (uint32_t *)(shmInBuf.vir_addr);
#else
//  compute->inBuf = ...;
#endif

    if (compute->inBuf == NULL) {
        printf("mmrpc_test: Error: inBuf == NULL\n");
        status = -1;
        goto leave;
    }

    /* fill input buffer with seed value */
    for (i = 0; i < compute->size; i++) {
        compute->inBuf[i] = 0x2010;
    }

    /* allocate an output buffer in shared memory */
#if defined(SYSLINK_BUILDOS_QNX)
    SHM_alloc(compute->size * sizeof(uint32_t), &shmOutBuf);
    compute->outBuf = (uint32_t *)(shmOutBuf.vir_addr);
#else
//  compute->outBuf = ...;
#endif

    if (compute->outBuf == NULL) {
        printf("mmrpc_test: Error: outBuf == NULL\n");
        status = -1;
        goto leave;
    }

    /* clear output buffer */
    for (i = 0; i < compute->size; i++) {
        compute->outBuf[i] = 0;
    }

    /* print some debug info */
    printf("mmrpc_test: calling Mx_compute(0x%x)\n", (unsigned int)compute);
    printf("mmrpc_test: compute->coef=0x%x\n", compute->coef);
    printf("mmrpc_test: compute->key=0x%x\n", compute->key);
    printf("mmrpc_test: compute->size=0x%x\n", compute->size);
    printf("mmrpc_test: compute->inBuf=0x%x\n", (unsigned int)compute->inBuf);
    printf("mmrpc_test: compute->outBuf=0x%x\n", (unsigned int)compute->outBuf);

    /* process the buffer */
    ret = Mx_compute(compute);

    if (ret < 0) {
        status = -1;
        printf("mmrpc_test: Error: Mx_Compute() failed\n");
        goto leave;
    }

    printf("mmrpc_test: after Mx_compute(0x%x)\n", (unsigned int)compute);
    printf("mmrpc_test: compute->coef=0x%x\n", compute->coef);
    printf("mmrpc_test: compute->key=0x%x\n", compute->key);
    printf("mmrpc_test: compute->size=0x%x\n", compute->size);
    printf("mmrpc_test: compute->inBuf=0x%x\n", (unsigned int)compute->inBuf);
    printf("mmrpc_test: compute->outBuf=0x%x\n", (unsigned int)compute->outBuf);
    printf("mmrpc_test: compute->inBuf[0]=0x%x\n",
            (unsigned int)compute->inBuf[0]);
    printf("mmrpc_test: compute->outBuf[0]=0x%x\n",
            (unsigned int)compute->outBuf[0]);

    /* check the output buffer */
    for (i = 0; i < compute->size; i++) {
        val = compute->outBuf[i] | compute->coef;
        if (compute->outBuf[i] != val) {
            status = -1;
            printf("mmrpc_test: Error: incorrect outBuf\n");
            break;
        }
    }

    /* free resources */
#if defined(SYSLINK_BUILDOS_QNX)
    SHM_release(&shmOutBuf);
    SHM_release(&shmInBuf);
    SHM_release(&shmCompute);
#else
//  ...
#endif

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
    MmRpc_Params args;

    /* create remote server insance */
    MmRpc_Params_init(&args);

    status = MmRpc_create("rpc_example", &args, &Mx_rpcIpu);

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
    if (Mx_rpcIpu != NULL) {
        MmRpc_delete(&Mx_rpcIpu);
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
    fxnCtx->params[0].type = MmRpc_ParamType_Scalar;
    fxnCtx->params[0].param.scalar.size = sizeof(int);
    fxnCtx->params[0].param.scalar.data = a;
    fxnCtx->num_xlts = 0;
    fxnCtx->xltAry = NULL;

    /* invoke the remote function call */
    status = MmRpc_call(Mx_rpcIpu, fxnCtx, &fxnRet);

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
    fxnCtx->params[0].type = MmRpc_ParamType_Scalar;
    fxnCtx->params[0].param.scalar.size = sizeof(int);
    fxnCtx->params[0].param.scalar.data = a;
    fxnCtx->params[1].type = MmRpc_ParamType_Scalar;
    fxnCtx->params[1].param.scalar.size = sizeof(int);
    fxnCtx->params[1].param.scalar.data = b;
    fxnCtx->num_xlts = 0;

    /* invoke the remote function call */
    status = MmRpc_call(Mx_rpcIpu, fxnCtx, &fxnRet);

    if (status < 0) {
        printf("mmrpc_test: Error: MmRpc_call failed\n");
        fxnRet = -1;
    }

    return(fxnRet);
}

/*
 *  ======== Mx_compute ========
 */
int32_t Mx_compute(Mx_Compute *compute)
{
    MmRpc_FxnCtx *fxnCtx;
    MmRpc_Xlt xltAry[2];
    int32_t fxnRet;
    char send_buf[512] = {0};
    int status;

    /* marshall function arguments into the send buffer */
    fxnCtx = (MmRpc_FxnCtx *)send_buf;

    fxnCtx->fxn_id = Mx_Fxn_compute;
    fxnCtx->num_params = 1;
    fxnCtx->params[0].type = MmRpc_ParamType_Ptr;
    fxnCtx->params[0].param.ptr.size = sizeof(Mx_Compute);
    fxnCtx->params[0].param.ptr.addr = (size_t)compute;
#if defined(SYSLINK_BUILDOS_QNX)
    fxnCtx->params[0].param.ptr.handle = NULL;
#else
//  fxnCtx->params[0].param.ptr.handle = ...;
#endif

    fxnCtx->num_xlts = 2;
    fxnCtx->xltAry = xltAry;

    fxnCtx->xltAry[0].index = 0;
    fxnCtx->xltAry[0].offset = MmRpc_OFFSET(compute, &compute->inBuf);
    fxnCtx->xltAry[0].base = (size_t)(compute->inBuf);
#if defined(SYSLINK_BUILDOS_QNX)
    fxnCtx->xltAry[0].handle = NULL;
#else
//  fxnCtx->xltAry[0].handle = ...;
#endif

    fxnCtx->xltAry[1].index = 0;
    fxnCtx->xltAry[1].offset = MmRpc_OFFSET(compute, &compute->outBuf);
    fxnCtx->xltAry[1].base = (size_t)(compute->outBuf);
#if defined(SYSLINK_BUILDOS_QNX)
    fxnCtx->xltAry[1].handle = NULL;
#else
//  fxnCtx->xltAry[1].handle = ...;
#endif

    /* invoke the remote function call */
    status = MmRpc_call(Mx_rpcIpu, fxnCtx, &fxnRet);

    if (status < 0) {
        printf("mmrpc_test: Error: MmRpc_call failed\n");
        fxnRet = -1;
    }

    return(fxnRet);
}
