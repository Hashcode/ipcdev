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
 *  ======== rpc_task.c ========
 *
 *  Example of setting up a 'RPC' service with the ServiceMgr, allowing clients
 *  to instantiate the example RPC instance.
 *
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Diags.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/ipc/MultiProc.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <ti/grcm/RcmTypes.h>
#include <ti/grcm/RcmServer.h>

#include <ti/srvmgr/ServiceMgr.h>
#include <ti/srvmgr/omaprpc/OmapRpc.h>
#include <ti/srvmgr/rpmsg_omx.h>
#include <ti/srvmgr/omx_packet.h>

#include <ti/sysbios/hal/Cache.h>

/* Turn on/off printf's */
#define CHATTER 1

#define RPC_MGR_PORT    59

/* Legacy function to allow Linux side rpmsg sample tests to work: */
extern void start_rpc_task();

/*
 * OMX packet expected to have its data payload start with a payload of
 * this value. Need to export this properly in a meaningful header file on
 * both HLOS and RTOS sides
 */
typedef enum {
    RPC_OMX_MAP_INFO_NONE       = 0,
    RPC_OMX_MAP_INFO_ONE_BUF    = 1,
    RPC_OMX_MAP_INFO_TWO_BUF    = 2,
    RPC_OMX_MAP_INFO_THREE_BUF  = 3,
    RPC_OMX_MAP_INFO_MAX        = 0x7FFFFFFF
} map_info_type;

/*
 *  ======== fxnTriple used by omx_benchmark test app ========
 */
typedef struct {
    Int a;
} FxnTripleArgs;

typedef struct {
    Int a;
    Int b;
} FxnAddArgs;

typedef struct {
    Int a;
    Int b;
    Int c;
} FxnAdd3Args;

typedef struct {
    Int num;
    Int *array;
} FxnAddXArgs;

#define H264_DECODER_NAME   "H264_decoder"

#define OMX_VIDEO_THREAD_PRIORITY    5

typedef UInt32 OMX_HANDLETYPE;

static Int32 RPC_SKEL_Init(Void *, UInt32 size, UInt32 *data);
static Int32 RPC_SKEL_Init2(UInt32 size, UInt32 *data);
static Int32 RPC_SKEL_SetParameter(UInt32 size, UInt32 *data);
static Int32 RPC_SKEL_GetParameter(UInt32 size, UInt32 *data);
static Int32 fxnTriple(UInt32 size, UInt32 *data);
static Int32 fxnAdd(UInt32 size, UInt32 *data);
static Int32 fxnAdd3(UInt32 size, UInt32 *data);
static Int32 fxnAddX(UInt32 size, UInt32 *data);

#if 0
/* RcmServer static function table */
static RcmServer_FxnDesc RPCServerFxnAry[] = {
    {"RPC_SKEL_Init", NULL}, /* filled in dynamically */
    {"RPC_SKEL_SetParameter", RPC_SKEL_SetParameter},
    {"RPC_SKEL_GetParameter", RPC_SKEL_GetParameter},
    {"fxnTriple", fxnTriple },
};

#define RPCServerFxnAryLen (sizeof RPCServerFxnAry / sizeof RPCServerFxnAry[0])

static const RcmServer_FxnDescAry RPCServer_fxnTab = {
    RPCServerFxnAryLen,
    RPCServerFxnAry
};
#endif

#define RPC_SVR_NUM_FXNS 5
OmapRpc_FuncDeclaration RPCServerFxns[RPC_SVR_NUM_FXNS] =
{
    { RPC_SKEL_Init2,
        { "RPC_SKEL_Init2", 3,
            {
                {OmapRpc_Direction_Out, OmapRpc_Param_S32, 1}, // return
                {OmapRpc_Direction_In, OmapRpc_Param_U32, 1},
                {OmapRpc_Direction_In, OmapRpc_PtrType(OmapRpc_Param_U32), 1}
            }
        }
    },
    { fxnTriple,
        { "fxnTriple", 2,
            {
                {OmapRpc_Direction_Out, OmapRpc_Param_S32, 1}, // return
                {OmapRpc_Direction_In, OmapRpc_Param_U32, 1}
            },
        },
    },
    { fxnAdd,
        { "fxnAdd", 3,
            {
                {OmapRpc_Direction_Out, OmapRpc_Param_S32, 1}, // return
                {OmapRpc_Direction_In, OmapRpc_Param_S32, 1},
                {OmapRpc_Direction_In, OmapRpc_Param_S32, 1}
            }
        }
    },
    { fxnAdd3,
        { "fxnAdd3", 2,
            {
                {OmapRpc_Direction_Out, OmapRpc_Param_S32, 1}, // return
                {OmapRpc_Direction_In, OmapRpc_PtrType(OmapRpc_Param_U32), 1}
            }
        }
    },
    { fxnAddX,
        { "fxnAddX", 2,
            {
                {OmapRpc_Direction_Out, OmapRpc_Param_S32, 1}, // return
                {OmapRpc_Direction_In, OmapRpc_PtrType(OmapRpc_Param_U32), 1}
            }
        }
    }
};

static Int32 RPC_SKEL_SetParameter(UInt32 size, UInt32 *data)
{
#if CHATTER
    System_printf("RPC_SKEL_SetParameter: Called\n");
#endif

    return(0);
}

static Int32 RPC_SKEL_GetParameter(UInt32 size, UInt32 *data)
{
#if CHATTER
    System_printf("RPC_SKEL_GetParameter: Called\n");
#endif

    return(0);
}

Void RPC_SKEL_SrvDelNotification()
{
    System_printf("RPC_SKEL_SrvDelNotification: Nothing to cleanup\n");
}

#define CALLBACK_DATA      "OMX_Callback"
#define PAYLOAD_SIZE       sizeof(CALLBACK_DATA)
#define CALLBACK_DATA_SIZE (HDRSIZE + OMXPACKETSIZE + PAYLOAD_SIZE)

static Int32 RPC_SKEL_Init(Void *srvc, UInt32 size, UInt32 *data)
{
    char              cComponentName[128] = {0};
    OMX_HANDLETYPE    hComp;
    Char              cb_data[HDRSIZE + OMXPACKETSIZE + PAYLOAD_SIZE] =  {0};

    /*
     * Note: Currently, rpmsg_omx linux driver expects an omx_msg_hdr in front
     * of the omx_packet data, so we allow space for this:
     */
    struct omx_msg_hdr * hdr = (struct omx_msg_hdr *)cb_data;
    struct omx_packet  * packet = (struct omx_packet *)hdr->data;


    //Marshalled:[>offset(cParameterName)|>pAppData|>offset(RcmServerName)|>pid|
    //>--cComponentName--|>--CallingCorercmServerName--|
    //<hComp]

    strcpy(cComponentName, (char *)data + sizeof(map_info_type));

#if CHATTER
    System_printf("RPC_SKEL_GetHandle: Component Name received: %s\n",
                  cComponentName);
#endif

    /* Simulate sending an async OMX callback message, passing an omx_packet
     * structure.
     */
    packet->msg_id  = 99;   // Set to indicate callback instance, buffer id, etc.
    packet->fxn_idx = 5;    // Set to indicate callback fxn
    packet->data_size = PAYLOAD_SIZE;
    strcpy((char *)packet->data, CALLBACK_DATA);

#if CHATTER
    System_printf("RPC_SKEL_GetHandle: Sending callback message id: %d, "
                  "fxn_id: %d, data: %s\n",
                  packet->msg_id, packet->fxn_idx, packet->data);
#endif
    ServiceMgr_send(srvc, cb_data, CALLBACK_DATA_SIZE);

    /* Call OMX_Get_Handle() and return handle for future calls. */
    //eCompReturn = OMX_GetHandle(&hComp, (OMX_STRING)&cComponentName[0], pAppData,&rpcCallBackInfo);
    hComp = 0x5C0FFEE5;
    data[0] = hComp;

#if CHATTER
    System_printf("RPC_SKEL_GetHandle: returning hComp: 0x%x\n", hComp);
#endif

    return(0);
}

static Int32 RPC_SKEL_Init2(UInt32 size, UInt32 *data)
{
    System_printf("RPC_SKEL_Init2: size = 0x%x data = 0x%x\n", size,
                                                            (UInt32)data);
    return 0;
}

/*
 *  ======== fxnTriple ========
 */
Int32 fxnTriple(UInt32 size, UInt32 *data)
{
    struct OmapRpc_Parameter *payload = (struct OmapRpc_Parameter *)data;
    Int a;

    a = (Int)payload[0].data;

#if CHATTER
    System_printf("fxnTriple: a=%d\n", a);
#endif

    return(a * 3);
}

/*
 *  ======== fxnAdd ========
 */
Int32 fxnAdd(UInt32 size, UInt32 *data)
{
    struct OmapRpc_Parameter *payload = (struct OmapRpc_Parameter *)data;
    Int a, b;

    a = (Int)payload[0].data;
    b = (Int)payload[1].data;

#if CHATTER
    System_printf("fxnAdd: a=%d, b=%d\n", a, b);
#endif

    return(a + b);
}

/*
 *  ======== fxnAdd3 ========
 */
Int32 fxnAdd3(UInt32 size, UInt32 *data)
{
    struct OmapRpc_Parameter *payload = (struct OmapRpc_Parameter *)data;
    FxnAdd3Args *args;
    Int a, b, c;

    args = (FxnAdd3Args *)payload[0].data;

    Cache_inv (args, sizeof(FxnAdd3Args), Cache_Type_ALL, TRUE);

    a = args->a;
    b = args->b;
    c = args->c;

#if CHATTER
    System_printf("fxnAdd3: a=%d, b=%d, c=%d\n", a, b, c);
#endif

    return(a + b + c);
}

/*
 *  ======== fxnAddX ========
 */
Int32 fxnAddX(UInt32 size, UInt32 *data)
{
    struct OmapRpc_Parameter *payload = (struct OmapRpc_Parameter *)data;
    FxnAddXArgs *args;
    Int num, i, sum = 0;
    Int *array;

    args = (FxnAddXArgs *)payload[0].data;

    Cache_inv (args, sizeof(FxnAddXArgs), Cache_Type_ALL, TRUE);

    num = args->num;
    array = args->array;

    Cache_inv (array, sizeof(Int) * num, Cache_Type_ALL, TRUE);

#if CHATTER
    System_printf("fxnAddX: ");
#endif
    for (i = 0; i < num; i++) {
#if CHATTER
        System_printf(" a[%d]=%d,", i, array[i]);
#endif
        sum += array[i];
    }

#if CHATTER
    System_printf(" sum=%d\n", sum);
#endif

    return(sum);
}

Void start_rpc_task()
{
    /* Init service manager */
    System_printf("%s initializing OMAPRPC based service manager endpoint\n",
                    MultiProc_getName(MultiProc_self()));

    OmapRpc_createChannel("rpc_example", MultiProc_getId("HOST"),
                          RPC_MGR_PORT, RPC_SVR_NUM_FXNS, RPCServerFxns,
                          (OmapRpc_SrvDelNotifyFxn)RPC_SKEL_SrvDelNotification);
}
