/*
 *  @file       rpmsg-resmgrdrv.c
 *
 *  @brief      driver for resmgr component.
 *
 *  ============================================================================
 *
 *  Copyright (c) 2011, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  Contact information for paper mail:
 *  Texas Instruments
 *  Post Office Box 655303
 *  Dallas, Texas 75265
 *  Contact information:
 *  http://www-k.ext.ti.com/sc/technical-support/product-information-centers.htm?
 *  DCMP=TIHomeTracking&HQS=Other+OT+home_d_contact
 *  ============================================================================
 *
 */


/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/List.h>
#include <ti/syslink/utils/String.h>
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/GateSpinlock.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>

/*QNX specific header include */
#include <errno.h>
#include <unistd.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/netmgr.h>

/* Module headers */
#include <ti/ipc/MessageQCopy.h>
#include <_MessageQCopy.h>
#include <_MessageQCopyDefs.h>
#include "OsalSemaphore.h"
#include "std_qnx.h"
#include <pthread.h>

#include "rpmsg_resmgr.h"
#if defined(SYSLINK_USE_IPU_PM)
#include "ipu_pm.h"
#endif
#include <SysLinkMemUtils.h>

#define  MAX_PROCESSES          32u

/*!
 *  @brief  Remote connection object
 */
typedef struct rpmsg_resmgr_conn_object {
    UInt32                  addr;
    /*!< Address of this remote connection */
    UInt16                  procId;
    /*!< Remote procId */
    List_Handle             clients;
    /*!< List of clients on this connection/remote proc */
    UInt32                  resRefCount;
    /*!< Reference count for requested resources */
#if defined(USE_MEMMGR)
    List_Handle             tilerBufs;
    /*!< List of tiler buffers allocated on behalf of remote proc */
#endif
} rpmsg_resmgr_conn_object;


/*!
 *  @brief  rpmsg-resmgr Module state object
 */
typedef struct rpmsg_resmgr_ModuleObject_tag {
    Bool                 isSetup;
    /*!< Indicates whether the module has been already setup */
    IGateProvider_Handle gateHandle;
    /*!< Handle of gate to be used for local thread safety */
    rpmsg_resmgr_conn_object * objects [MultiProc_MAXPROCESSORS];
    /*!< List of all remote connections. */
    MessageQCopy_Handle mqHandle;
    /*!< Local mq handle associated with this module */
    UInt32 endpoint;
#if defined(USE_MEMMGR)
    /*!< Local endpoint associated with the mq handle */
    MessageQCopy_Handle mqMemHandle;
    /*!< Local mq handle associated with this module */
    UInt32 memEndpoint;
    /*!< Local memMgr endpoint associated with the mq handle */
#endif
} rpmsg_resmgr_ModuleObject;

/*!
 *  @brief  Structure of resource manager client information.
 */
typedef struct rpmsg_resmgr_client_tag {
    List_Elem          element;
    /*!< List element header */
    UInt32             addr;
    /*!< Remote addr for the client */
    List_Handle        resources;
    /*!< List of acquired resources */
} rpmsg_resmgr_client ;

/*!
 *  @brief  Structure of resource information.
 */
typedef struct rpmsg_resmgr_res_tag {
    List_Elem          element;
    /*!< List element header */
    UInt32             res_type;
    /* Type of resource */
    Char               data[];
    /* Resource-specific request info */
} rpmsg_resmgr_res ;

/*!
 *  @brief  Structure of resource information.
 */
typedef struct rpmsg_resmgr_tiler_res_tag {
    List_Elem          element;
    /*!< List element header */
    UInt32             buf;
    /* Allocated tiler address */
} rpmsg_resmgr_tiler_res ;

#if defined(SYSLINK_USE_IPU_PM)
#define IPU_PM_SELF     100
#define IPU_PM_MPU      101
#define IPU_PM_CORE     102
#endif

int rpmsg_resmgr_free(rpmsg_resmgr_conn_object * resmgr, UInt32 addr, struct rprm_request * request);

/** ============================================================================
 *  Globals
 *  ============================================================================
 */
/*!
 *  @var    rpmsg_resmgr_state
 *
 *  @brief  rpmsg-resmgr state object variable
 */
static rpmsg_resmgr_ModuleObject rpmsg_resmgr_state =
{
    .gateHandle = NULL,
    .isSetup = FALSE,
};

extern dispatch_t * syslink_dpp;

rpmsg_resmgr_res * find_resource(rpmsg_resmgr_client * client, UInt32 handle)
{
    List_Elem * elem = NULL;
    rpmsg_resmgr_res * res = NULL;

    /* Search the client list for this addr */
    List_traverse(elem, client->resources) {
        if ((UInt32)elem == handle) {
            res = (rpmsg_resmgr_res *)elem;
            break;
        }
    }

    return res;
}

rpmsg_resmgr_client * find_client(List_Handle client_list, UInt32 addr)
{
    List_Elem * elem = NULL;
    rpmsg_resmgr_client * client = NULL;

    /* Search the client list for this addr */
    List_traverse(elem, client_list) {
        if (((rpmsg_resmgr_client *)elem)->addr == addr) {
            client = (rpmsg_resmgr_client *)elem;
            break;
        }
    }

    return client;
}

int rpmsg_resmgr_disconnect(rpmsg_resmgr_conn_object * resmgr, UInt32 addr)
{
    int status = EOK;
    IArg key = NULL;
    rpmsg_resmgr_client * client = NULL;

    /* Grab the gate */
    key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
    /* Check if this addr already connected */
    client = find_client(resmgr->clients, addr);

    /* If found, remove from the list */
    if (client) {
        List_Elem * elem = NULL, * temp = NULL;
        rpmsg_resmgr_res * res = NULL;
        char msg[MessageQCopy_BUFSIZE];
        struct rprm_request * req = (struct rprm_request *)msg;
        struct rprm_free_data * fdata = (struct rprm_free_data *)req->data;

        /* Free up resources that weren't properly freed */
        List_traverse_safe(elem, temp, client->resources) {
            res = (rpmsg_resmgr_res *)elem;
            fdata->res_id = (uint32_t)res;
            rpmsg_resmgr_free(resmgr, addr, req);
        }

        /* Delete the resource list */
        List_delete(&client->resources);
        List_remove(resmgr->clients, (List_Elem *)client);
        Memory_free (NULL, client, sizeof(*client));
        status = 0;
    }
    else {
        status = -ENOTCONN;
    }

    /* Release the gate */
    IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);

    return status;
}

int rpmsg_resmgr_constraints_rel(rpmsg_resmgr_conn_object * resmgr, UInt32 addr, struct rprm_request * request)
{
    int status = EOK;
    IArg key = NULL;
    UInt32 tmp_rsrc;
    rpmsg_resmgr_client * client = NULL;
    struct rprm_constraint * cdata = (struct rprm_constraint *)(request->data);
#if defined(SYSLINK_USE_IPU_PM)
    struct ipu_pm_const_req req;
#endif

    /* Grab the gate */
    key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
    /* Check if this client is connected */
    client = find_client(resmgr->clients, addr);

    /* If client found, handle the request */
    if (client) {
        /* Find this resource in the resource list */
        rpmsg_resmgr_res * resource = find_resource(client, cdata->res_id);
        if (resource) {
            if (cdata->data.mask & RPRM_SCALE) {
                /* This is a frequency request */
                switch (resource->res_type) {
                    case RPRM_IPU:
                    case RPRM_ISS:
                    case RPRM_FDIF:
#if defined(SYSLINK_USE_IPU_PM)
                        tmp_rsrc = IPU_PM_CORE;
#endif
                        break;
                    default:
                        tmp_rsrc = resource->res_type;
                        break;
                }
#if defined(SYSLINK_USE_IPU_PM)
                req.rsrc = resource->res_type;
                req.target_rsrc = tmp_rsrc;
                req.rate = NO_FREQ_CONSTRAINT;
                status = ipu_pm_set_rate(&req);
#endif
            }
            if (cdata->data.mask & RPRM_LATENCY) {
                /* This is a latency request */
                /* TODO: handle the latency request */
            }
            if (cdata->data.mask & RPRM_BANDWIDTH) {
                /* This is a bandwidth request */
                if(resource->res_type == RPRM_IPU) {
                    GT_1trace(curTrace, GT_3CLASS, "###L3 BANDWIDTH CONSTRAINTS release(%d)", cdata->data.bandwidth);
#if defined(SYSLINK_USE_IPU_PM)
                    ipu_pm_set_bandwidth(0);
#endif
                }
            }
        }
        else {
            status = -ENOENT;
        }
    }
    else {
        status = -ENOTCONN;
    }

    /* Release the gate */
    IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);
    return status;
}

int rpmsg_resmgr_constraints_req(rpmsg_resmgr_conn_object * resmgr, UInt32 addr, struct rprm_request * request)
{
    int status = EOK;
    IArg key = NULL;
    UInt32 tmp_rsrc;
    rpmsg_resmgr_client * client = NULL;
    struct rprm_constraint * cdata = (struct rprm_constraint *)(request->data);
#if defined(SYSLINK_USE_IPU_PM)
    struct ipu_pm_const_req req;
#endif

    /* Grab the gate */
    key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
    /* Check if this client is connected */
    client = find_client(resmgr->clients, addr);

    /* If client found, handle the request */
    if (client) {
        /* Find this resource in the resource list */
        rpmsg_resmgr_res * resource = find_resource(client, cdata->res_id);
        if (resource) {
            if (cdata->data.mask & RPRM_SCALE) {
                /* This is a frequency request */
                switch (resource->res_type) {
                    case RPRM_IPU:
                    case RPRM_ISS:
                    case RPRM_FDIF:
#if defined(SYSLINK_USE_IPU_PM)
                        tmp_rsrc = IPU_PM_CORE;
#endif
                        break;
                    default:
                        tmp_rsrc = resource->res_type;
                        break;
                }
#if defined(SYSLINK_USE_IPU_PM)
                req.rsrc = resource->res_type;
                req.target_rsrc = tmp_rsrc;
                req.rate = cdata->data.frequency;
                status = ipu_pm_set_rate(&req);
#endif
            }
            if (cdata->data.mask & RPRM_LATENCY) {
                /* This is a latency request */
                /* TODO: handle the latency request */
            }
            if (cdata->data.mask & RPRM_BANDWIDTH) {
                /* This is a bandwidth request */
                if(resource->res_type == RPRM_IPU) {
                    GT_1trace(curTrace, GT_3CLASS, "###L3 BANDWIDTH CONSTRAINTS req(%d)", cdata->data.bandwidth);
#if defined(SYSLINK_USE_IPU_PM)
                    ipu_pm_set_bandwidth(cdata->data.bandwidth);
#endif
                }
            }
        }
        else {
            status = -ENOENT;
        }
    }
    else {
        status = -ENOTCONN;
    }

    /* Release the gate */
    IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);
    return status;
}



int rpmsg_resmgr_req_data(rpmsg_resmgr_conn_object * resmgr, UInt32 addr, struct rprm_request * request)
{
    int status = EOK;
    IArg key = NULL;
    rpmsg_resmgr_client * client = NULL;
    struct rprm_reqdata * rdata = (struct rprm_reqdata *)request->data;

    /* Grab the gate */
    key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
    /* Check if this client is connected */
    client = find_client(resmgr->clients, addr);

    /* If client found, alloc the resource */
    if (client) {
        rpmsg_resmgr_res * resource = find_resource(client, rdata->res_id);
        if (resource) {
            switch (rdata->type) {
                case RPRM_REQTYPE_MAXFREQ:
                {
#if defined(SYSLINK_USE_IPU_PM)
                    struct rprm_freq * fdata = (struct rprm_freq *)rdata->data;
                    status = ipu_pm_get_max_freq(resource->res_type, &fdata->value);
#else
                    status = -ENOENT;
#endif
                }
                break;
                default:
                    status = -ENOENT;
                break;
            }
        }
        else {
            status = -ENOENT;
        }
    }
    else {
        status = -ENOTCONN;
    }

    /* Release the gate */
    IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);
    return status;
}

/* Must be holding gate when calling this function */
int rpmsg_resmgr_free(rpmsg_resmgr_conn_object * resmgr, UInt32 addr, struct rprm_request * request)
{
    int status = EOK;
    struct rprm_free_data * fdata = (struct rprm_free_data *)request->data;
    rpmsg_resmgr_client * client = NULL;

    /* Check if this client is connected */
    client = find_client(resmgr->clients, addr);

    /* If client found, free the resource */
    if (client) {
        /* Find this resource in the resource list */
        rpmsg_resmgr_res * resource = find_resource(client, fdata->res_id);
        if (resource) {
            /* Remove resource from list and free memory */
            List_remove(client->resources, &resource->element);
            switch (resource->res_type) {
                case RPRM_IVAHD:
#if defined(SYSLINK_USE_IPU_PM)
                    status = ipu_pm_ivahd_disable();
#endif
                    break;
                case RPRM_IVASEQ0:
#if defined(SYSLINK_USE_IPU_PM)
                    status = ipu_pm_ivaseq0_disable();
#endif
                    break;
                case RPRM_IVASEQ1:
#if defined(SYSLINK_USE_IPU_PM)
                    status = ipu_pm_ivaseq1_disable();
#endif
                    break;
                case RPRM_L3BUS:
                    /* TODO: handle RPRM_L3BUS free request */
                    break;
                case RPRM_SL2IF:
                    /* TODO: handle SL2IF free request */
                    break;
                case RPRM_IPU:
                    /* TODO: handle IPU free request */
                    break;
                case RPRM_DSP:
                    /* TODO: handle DSP free request */
                    break;
                case RPRM_SDMA:
                {
#if defined(SYSLINK_USE_IPU_PM)
                    struct rprm_sdma * sdma = (struct rprm_sdma *)resource->data;
                    status = ipu_pm_free_sdma(sdma->num_chs, sdma->channels);
                    if (status < 0) {
                        status = -1;
                        GT_setFailureReason (curTrace,
                                            GT_4CLASS,
                                            "rpmsg_resmgr_free",
                                            status,
                                            "Failed to free DMA!");
                    }
#endif
                }
                break;
                case RPRM_CAMERA:
                    /* Nothing to do.  Always passes. */
                    break;
                case RPRM_LED:
                    /* Nothing to do.  Always passes. */
                    break;
                default:
                    break;
            }
            /* Free the resource */
            Memory_free(NULL, resource, sizeof(*resource));
            fdata->res_id = 0;
        }
        else {
            status = -ENOENT;
        }
    }
    else {
        status = -ENOTCONN;
    }

    if (status >= 0) {
        resmgr->resRefCount--;
    }

    return status;
}

int rpmsg_resmgr_alloc(rpmsg_resmgr_conn_object * resmgr, UInt32 addr, struct rprm_request * request, uint32_t res_type)
{
    int status = EOK;
    IArg key = NULL;
    rpmsg_resmgr_client * client = NULL;
#if defined(SYSLINK_USE_IPU_PM)
    struct rprm_alloc_data * adata = (struct rprm_alloc_data *)request->data;
#endif

    /* Grab the gate */
    key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
    /* Check if this client is connected */
    client = find_client(resmgr->clients, addr);

    /* If client found, alloc the resource */
    if (client) {
        /* TODO: alloc the resource */
        switch (res_type) {
            case RPRM_IVAHD:
#if defined(SYSLINK_USE_IPU_PM)
                status = ipu_pm_ivahd_enable();
                if (status < 0) {
                    status = -1;
                    ipu_pm_ivahd_disable();
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rpmsg_resmgr_alloc",
                                         status,
                                         "Failed to enable IVAHD!");
                }
#endif
            break;
            case RPRM_IVASEQ0:
#if defined(SYSLINK_USE_IPU_PM)
                status = ipu_pm_ivaseq0_enable();
#endif
            break;
            case RPRM_IVASEQ1:
#if defined(SYSLINK_USE_IPU_PM)
                status = ipu_pm_ivaseq1_enable();
#endif
            break;
            case RPRM_L3BUS:
                /* TODO: handle RPRM_L3BUS alloc request */
            break;
            case RPRM_SL2IF:
                /* TODO: handle SL2IF alloc request */
            break;
            case RPRM_IPU:
                /* TODO: handle IPU alloc request */
            break;
            case RPRM_DSP:
                /* TODO: handle DSP alloc request */
            break;
            case RPRM_SDMA:
            {
#if defined(SYSLINK_USE_IPU_PM)
                struct rprm_sdma * sdma = (struct rprm_sdma *)adata->data;
                status = ipu_pm_alloc_sdma(sdma->num_chs, sdma->channels);
                if (status < 0) {
                    status = -1;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rpmsg_resmgr_alloc",
                                         status,
                                         "Failed to alloc DMA!");
                }
#endif
            }
            break;

            case RPRM_CAMERA:
            {
#if defined(SYSLINK_USE_IPU_PM)
                struct rprm_camera * cam = (struct rprm_camera *)adata->data;
                status = ipu_pm_camera_enable(cam->mode, cam->on);
                if (status < 0) {
                    status = -1;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rpmsg_resmgr_alloc",
                                         status,
                                         "Failed to enable camera!");
                }
#endif
            }
            break;
            case RPRM_LED:
            {
#if defined(SYSLINK_USE_IPU_PM)
                struct rprm_led * led = (struct rprm_led *)adata->data;
                led->intensity = ipu_pm_led_enable(led->mode, led->intensity);
                if (led->intensity == -1) {
                    status = -1;
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "rpmsg_resmgr_alloc",
                                         status,
                                         "Failed to enable led!");
                }
#endif
            }
            break;
            default:
                status = -ENOENT;
            break;
        }
        if (status >= 0) {
            rpmsg_resmgr_res * resource = NULL;
            /* store the resource per-client for tracking */
            resource = Memory_alloc(NULL, sizeof (rpmsg_resmgr_res), 0x0, NULL);
            if (resource) {
                resource->res_type = res_type;
                List_put(client->resources, &resource->element);
            }
            else {
                status = -ENOMEM;
            }
            /* give the resource id back to the requester */
            status = (int)resource;
        }
    }
    else {
        status = -ENOTCONN;
    }

    if (status >= 0) {
        resmgr->resRefCount++;
    }
    /* Release the gate */
    IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);
    return status;
}

int rpmsg_resmgr_connect(rpmsg_resmgr_conn_object * resmgr, UInt32 addr)
{
    int status = EOK;
    IArg key = NULL;
    rpmsg_resmgr_client * client = NULL;
    List_Params listParams;

    /* Grab the gate */
    key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
    /* Check if this addr already connected */
    client = find_client(resmgr->clients, addr);

    /* Otherwise, add to the list */
    if (!client) {
        client = Memory_alloc (NULL, sizeof(*client), 0x0, NULL);
        if (client) {
            client->addr = addr;
            /* create a list to store resources used by this client */
            List_Params_init(&listParams);
            client->resources = List_create(&listParams);
            if (client->resources) {
                List_put (resmgr->clients, &(client->element));
            }
            else {
                Memory_free (NULL, client, sizeof(*client));
                client = NULL;
                status = -ENOMEM;
            }
        }
        else {
            status = -ENOMEM;
        }
    }
    else {
        status = -EISCONN;
    }
    /* Release the gate */
    IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);
    return status;
}

uint32_t rpmsg_resmgr_get_resp_len(uint32_t res_type)
{
    uint32_t len = 0;

    switch (res_type) {
        case RPRM_GPTIMER:
            len = sizeof(struct rprm_gpt);
            break;
        case RPRM_AUXCLK:
            len = sizeof(struct rprm_auxclk);
            break;
        case RPRM_REGULATOR:
            len = sizeof(struct rprm_regulator);
            break;
        case RPRM_GPIO:
            len = sizeof(struct rprm_gpio);
            break;
        case RPRM_SDMA:
            len = sizeof(struct rprm_sdma);
            break;
        case RPRM_IPU:
        case RPRM_DSP:
            len = sizeof(struct rprm_proc);
            break;
        case RPRM_I2C:
            len = sizeof(struct rprm_i2c);
            break;
        case RPRM_CAMERA:
            len = sizeof(struct rprm_camera);
            break;
        case RPRM_LED:
            len = sizeof(struct rprm_led);
            break;
        default:
            len = 0;
            break;
    }

    return len;
}

uint32_t rpmsg_resmgr_get_req_data_len(uint32_t res_type)
{
    uint32_t len = 0;

    switch (res_type) {
        case RPRM_REQTYPE_MAXFREQ:
            len = sizeof(struct rprm_freq);
            break;
        default:
            len = 0;
            break;
    }

    return len;
}

uint32_t rpmsg_resmgr_get_req_len(uint32_t acquire)
{
    uint32_t len = 0;

    switch (acquire) {
        case RPRM_REQ_ALLOC:
            len = sizeof(struct rprm_alloc_data);
            break;
        case RPRM_REQ_FREE:
            len = sizeof(struct rprm_free_data);
            break;
        case RPRM_REQ_CONSTRAINTS:
            len = sizeof(struct rprm_constraint);
            break;
        case RPRM_REQ_DATA:
            len = sizeof(struct rprm_reqdata);
            break;
        default:
            break;
    }

    return len;
}

uint32_t rpmsg_resmgr_get_alloc_data_len(uint32_t res_type)
{
    uint32_t len = 0;

    switch (res_type) {
        case RPRM_GPTIMER:
            len = sizeof(struct rprm_gpt);
            break;
       case RPRM_AUXCLK:
            len = sizeof(struct rprm_auxclk);
            break;
        case RPRM_REGULATOR:
            len = sizeof(struct rprm_regulator);
            break;
        case RPRM_GPIO:
            len = sizeof(struct rprm_gpio);
            break;
        case RPRM_SDMA:
            len = sizeof(struct rprm_sdma);
            break;
        case RPRM_IPU:
        case RPRM_DSP:
            len = sizeof(struct rprm_proc);
            break;
        case RPRM_I2C:
            len = sizeof(struct rprm_i2c);
            break;
        case RPRM_CAMERA:
            len = sizeof(struct rprm_camera);
            break;
        case RPRM_LED:
            len = sizeof(struct rprm_led);
            break;
        default:
            break;
    }

    return len;
}

/*!
 *  @brief      This function implements the callback registered with IPS. Here
 *              to pass event no. back to user function (so that it can do
 *              another level of demultiplexing of callbacks)
 *
 *  @param      procId    processor Id from which interrupt is received
 *  @param      lineId    Interrupt line ID to be used
 *  @param      eventId   eventId registered
 *  @param      arg       argument to call back
 *  @param      payload   payload received
 *
 *  @sa
 */
Void
_rpmsg_resmgr_cb (MessageQCopy_Handle handle, void * data, int len, void * priv, UInt32 src, UInt16 srcProc)
{
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int32                   status = 0;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    int err = EOK;
    int i = 0;
    rpmsg_resmgr_conn_object * resmgr_conn = NULL;
    struct rprm_request * request = (struct rprm_request *)data;
    char msg[MessageQCopy_BUFSIZE];
    struct rprm_ack * response = (struct rprm_ack *)msg;
    int resp_len = 0;
    int req_len = 0;
    IArg key = NULL;
    Int res_type = -1;
    struct rprm_alloc_data * adata = NULL;
    struct rprm_ack_data *ack_data = (struct rprm_ack_data *)response->data;
    struct rprm_reqdata * rdata = NULL;

    GT_6trace (curTrace,
               GT_ENTER,
               "_rpmsg_resmgr_cb",
               handle,
               data,
               len,
               priv,
               src,
               srcProc);

    if (len < sizeof(struct rprm_request)) {
        /* Message is not large enough */
        /* TODO: handle error */
        return;
    }

    req_len = rpmsg_resmgr_get_req_len(request->acquire);
    if (len < sizeof(struct rprm_request) + req_len) {
        /* Message is not large enough */
        /* TODO: handle error */
        err = -EINVAL;
        goto exit;
    }
    if (request->acquire == RPRM_REQ_ALLOC) {
        adata = (struct rprm_alloc_data *)request->data;
        res_type = rpmsg_resmgr_to_type(adata->res_name);
        if (res_type != -1) {
            if (res_type == -2) {
                struct rprm_proc * proc = (struct rprm_proc *)adata->data;
                if (len < sizeof(struct rprm_request) +
                        sizeof(struct rprm_proc) + req_len) {
                    /* Message is not large enough */
                    err = -EINVAL;
                    goto exit;
                }
                res_type = rpmsg_resmgr_to_type(proc->name);
            }
            req_len += rpmsg_resmgr_get_alloc_data_len(res_type);
            if (len < sizeof(struct rprm_request) + req_len) {
                /* Message is not large enough */
                err = -EINVAL;
                goto exit;
            }
        }
        else {
            err = -EINVAL;
            goto exit;
        }
    }
    else if (request->acquire == RPRM_REQ_DATA) {
        rdata = (struct rprm_reqdata *)request->data;
        res_type = rdata->type;
        req_len += rpmsg_resmgr_get_req_data_len(res_type);
        if (len < sizeof(struct rprm_request) + req_len) {
            /* Message is not large enough */
            err = -EINVAL;
            goto exit;
        }
    }

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i] != NULL) {
            if (rpmsg_resmgr_state.objects[i]->procId == srcProc)
                break;
        }
    }
    if (i == MultiProc_MAXPROCESSORS) {
        err = -EINVAL;
        goto exit;
    }
    resmgr_conn = (rpmsg_resmgr_conn_object *) rpmsg_resmgr_state.objects[i];

    switch (request->acquire) {
        case RPRM_REQ_ALLOC:
            err = rpmsg_resmgr_alloc(resmgr_conn, src, request, res_type);
            if (err > EOK) {
                resp_len = rpmsg_resmgr_get_resp_len(res_type);
                ack_data->res_id = err;
                Memory_copy(ack_data->data, adata->data, resp_len);
                resp_len += sizeof(*ack_data);
                err = EOK;
            }
            break;
        case RPRM_REQ_FREE:
            /* Grab the gate */
            key = IGateProvider_enter (rpmsg_resmgr_state.gateHandle);
            err = rpmsg_resmgr_free(resmgr_conn, src, request);
            /* Release the gate */
            IGateProvider_leave (rpmsg_resmgr_state.gateHandle, key);
            /* don't send a response in this case */
            return;
        case RPRM_REQ_CONSTRAINTS:
            resp_len = sizeof(struct rprm_constraint);
            err = rpmsg_resmgr_constraints_req(resmgr_conn, src, request);
            break;
        case RPRM_REL_CONSTRAINTS:
            err = rpmsg_resmgr_constraints_rel(resmgr_conn, src, request);
            /* don't send a response in this case */
            return;
        case RPRM_REQ_DATA:
            resp_len = rpmsg_resmgr_get_req_data_len(rdata->type);
            err = rpmsg_resmgr_req_data(resmgr_conn, src, request);
            break;
        default:
            err = -EINVAL;
            break;
    }

exit:
    /* Populate the response */
    response->acquire = request->acquire;
    response->ret = err;

    /* Send the response to the remote core */
    status = MessageQCopy_send(srcProc, MultiProc_self(), src,
                               rpmsg_resmgr_state.endpoint, response,
                               sizeof(*response) + resp_len,
                               TRUE);
    if (status < 0) {
        /* TODO: handle error */
    }

    GT_0trace (curTrace, GT_LEAVE, "_rpmsg_resmgr_cb");
}

#if defined(USE_MEMMGR)
/*
 *  MemMgr request sent to MemMgr server
 */

typedef struct {
    UInt32 reqType;
    char args[];
} MemMgr_Req;

/*
 *  MemMgr response received from MemMgr server
 */

typedef struct {
    UInt32 result;
} MemMgr_Resp;

typedef enum {
    MemMgr_Function_ALLOC,
    MemMgr_Function_FREE
} MemMgr_ReqType;

/*!
 *  @brief      This function implements the callback registered with IPC. Here
 *              to pass event no. back to user function (so that it can do
 *              another level of demultiplexing of callbacks)
 *
 *  @param      procId    processor Id from which interrupt is received
 *  @param      lineId    Interrupt line ID to be used
 *  @param      eventId   eventId registered
 *  @param      arg       argument to call back
 *  @param      payload   payload received
 *
 *  @sa
 */
Void
_rpmsg_memmgr_cb (MessageQCopy_Handle handle, void * data, int len, void * priv, UInt32 src, UInt16 srcProc)
{
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    Int32                   status = 0;
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
    int err = EOK;
    int i = 0;
    rpmsg_resmgr_conn_object * resmgr_conn = NULL;
    MemMgr_Req * request = (MemMgr_Req *)data;
    char msg[MessageQCopy_BUFSIZE];
    MemMgr_Resp * response = (MemMgr_Resp *)msg;
    List_Elem * elem = NULL;
    rpmsg_resmgr_tiler_res * res = NULL;

    GT_6trace (curTrace,
               GT_ENTER,
               "_rpmsg_memmgr_cb",
               handle,
               data,
               len,
               priv,
               src,
               srcProc);

    if (len < sizeof(MemMgr_Req)) {
        /* Message is not large enough */
        /* TODO: handle error */
        return;
    }

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i] != NULL) {
            if (rpmsg_resmgr_state.objects[i]->procId == srcProc)
                break;
        }
    }
    if (i == MultiProc_MAXPROCESSORS) {
        /* TODO: handle error */
        return;
    }
    resmgr_conn = (rpmsg_resmgr_conn_object *) rpmsg_resmgr_state.objects[i];

    switch (request->reqType) {
        case MemMgr_Function_ALLOC:
            err = SysLinkMemUtils_alloc(len, (UInt32 *)request->args);
            if (err != 0) {
                res = Memory_alloc(NULL, sizeof (rpmsg_resmgr_tiler_res), 0x0, NULL);
                if (res) {
                    res->buf = err;
                    List_put(resmgr_conn->tilerBufs, &res->element);
                }
                else {
                    err = 0;
                    SysLinkMemUtils_free(len, (UInt32 *)&err);
                }
            }
            break;
        case MemMgr_Function_FREE:
            err = SysLinkMemUtils_free(len, (UInt32 *)request->args);
            /* Search the client list for this addr */
            List_traverse(elem, resmgr_conn->tilerBufs) {
                res = (rpmsg_resmgr_tiler_res *)elem;
                if (res->buf == *(UInt32 *)(request->args)) {
                    break;
                }
            }
            if (res) {
                List_remove(resmgr_conn->tilerBufs, &res->element);
            }
            break;
        default:
            err = -EINVAL;
            break;
    }

    /* Populate the response */
    response->result = err;

    /* Send the response to the remote core */
    status = MessageQCopy_send(srcProc, MultiProc_self(), src, rpmsg_resmgr_state.memEndpoint, response, sizeof(*response), TRUE);
    if (status < 0) {
        /* TODO: handle error */
    }

    GT_0trace (curTrace, GT_LEAVE, "_rpmsg_memmgr_cb");
}
#endif

static
Void
_rpmsg_resmgr_create_conn (UInt16 procId, UInt32 endpoint)
{
    Int i = 0;
    Bool found = FALSE;
    rpmsg_resmgr_conn_object * obj = NULL;
    List_Params listParams;

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i] == NULL) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        /* found a space to save this mq handle, allocate memory */
        obj = Memory_calloc (NULL, sizeof (rpmsg_resmgr_conn_object), 0x0, NULL);
        if (obj) {
            /* store the object in the module info */
            rpmsg_resmgr_state.objects[i] = obj;

            /* store the mq info in the object */
            obj->procId = procId;
            obj->addr = endpoint;

            /* create a list to store remote proc connections */
            List_Params_init(&listParams);
            obj->clients = List_create(&listParams);
            if (obj->clients == NULL) {
                rpmsg_resmgr_state.objects[i] = NULL;
                Memory_free (NULL, obj, sizeof (rpmsg_resmgr_conn_object));
            }
            else {
                /* create a list to store remote proc connections */
                List_Params_init(&listParams);
#if defined(USE_MEMMGR)
                obj->tilerBufs = List_create(&listParams);
                if (obj->tilerBufs == NULL) {
                    List_delete(&obj->clients);
                    obj->clients = NULL;
                    rpmsg_resmgr_state.objects[i] = NULL;
                    Memory_free (NULL, obj, sizeof (rpmsg_resmgr_conn_object));
                }
#endif
            }
        }
    }
}


static
 Void
 _rpmsg_resmgr_delete_conn (rpmsg_resmgr_conn_object * obj)
{
    Int i = 0;
    Bool found = FALSE;

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i] == obj) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        if (obj) {
#if defined(USE_MEMMGR)
            if (obj->tilerBufs) {
                UInt32 buf = NULL;
                List_Elem * elem = NULL, * temp = NULL;
                /* Free up resources that weren't properly freed */
                List_traverse_safe(elem, temp, obj->tilerBufs) {
                    buf = ((rpmsg_resmgr_tiler_res *)elem)->buf;
                    SysLinkMemUtils_free(sizeof(Ptr), &buf);
                }
                List_delete(&obj->tilerBufs);
            }
#endif
            if (obj->clients) {
                /* Check if there are still registered clients and
                   disconnect them, thereby freeing the resources. */
                List_Elem * elem = NULL, * temp = NULL;
                rpmsg_resmgr_client * client = NULL;

                List_traverse_safe(elem, temp, obj->clients) {
                    client = (rpmsg_resmgr_client *)elem;
                    rpmsg_resmgr_disconnect(obj, client->addr);
                }

                List_delete(&obj->clients);
            }
            rpmsg_resmgr_state.objects[i] = NULL;
            Memory_free(NULL, obj, sizeof(rpmsg_resmgr_conn_object));
        }
    }
}

/**
 * Callback passed to MessageQCopy_registerNotify.
 *
 * This callback is called when a remote processor creates a MessageQCopy
 * handle with the same name as the local MessageQCopy handle and then
 * calls NameMap_register to notify the HOST of the handle.
 *
 * \param handle    The remote handle.
 * \param procId    The remote proc ID of the remote handle.
 * \param endpoint  The endpoint address of the remote handle.
 * \param create    The endpoint is created or deleted
 *
 * \return None.
 */

static
Void
_rpmsg_resmgr_notify_cb (MessageQCopy_Handle handle, UInt16 procId,
                         UInt32 endpoint, Bool create)
{
    int status = EOK;
    Int i = 0;
    Bool found = FALSE;
    rpmsg_resmgr_conn_object * resmgr_conn = NULL;
    Char msg[MessageQCopy_BUFSIZE];
    struct rprm_ack * response = (struct rprm_ack *)msg;

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i] != NULL) {
            if (rpmsg_resmgr_state.objects[i]->procId == procId) {
                found = TRUE;
                break;
            }
        }
    }

    if (found) {
        resmgr_conn = (rpmsg_resmgr_conn_object *) rpmsg_resmgr_state.objects[i];

        if (create)
            status = rpmsg_resmgr_connect(resmgr_conn, endpoint);
        else {
            status = rpmsg_resmgr_disconnect(resmgr_conn, endpoint);
            // don't send a response in this case
            return;
        }
    }
    else {
        status = -EINVAL;
    }

    /* Populate the response */
    response->ret = status;

    /* Send the response to the remote core */
    status = MessageQCopy_send(procId, MultiProc_self(), endpoint,
                               rpmsg_resmgr_state.endpoint, response,
                               sizeof(*response), TRUE);
    if (status < 0) {
        /* TODO: handle error */
    }
}

Bool
rpmsg_resmgr_allow_hib (UInt16 proc_id)
{
    Bool allow = FALSE;
    int i = 0;
    rpmsg_resmgr_conn_object * resmgr_conn = NULL;

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i] != NULL) {
            if (rpmsg_resmgr_state.objects[i]->procId == proc_id)
                break;
        }
    }
    if (i < MultiProc_MAXPROCESSORS) {
        resmgr_conn = (rpmsg_resmgr_conn_object *) rpmsg_resmgr_state.objects[i];

        if (!resmgr_conn->resRefCount)
            allow = TRUE;
    }

    return allow;
}

/*!
 *  @brief  Module setup function.
 *
 *  @sa     rpmsg_resmgr_destroy
 */
Int
rpmsg_resmgr_setup (Void)
{
    Int status = 0;
    UInt32 i = 0;
    Error_Block eb;

    GT_0trace (curTrace, GT_ENTER, "rpmsg_resmgr_setup");

    Error_init(&eb);

    rpmsg_resmgr_state.gateHandle = (IGateProvider_Handle)
                 GateSpinlock_create ((GateSpinlock_Handle) NULL, &eb);
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    if (rpmsg_resmgr_state.gateHandle == NULL) {
        status = -ENOMEM;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "_rpmsg_resmgr_setup",
                             status,
                             "Failed to create spinlock gate!");
    }
    else {
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */
        rpmsg_resmgr_state.mqHandle = MessageQCopy_create (
                                               MessageQCopy_ADDRANY,
                                               "rpmsg-resmgr",
                                               _rpmsg_resmgr_cb,
                                               NULL,
                                               &rpmsg_resmgr_state.endpoint);
        if (rpmsg_resmgr_state.mqHandle == NULL) {
            /*! @retval ENOMEM Failed to register MQCopy handle! */
            status = -ENOMEM;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "rpmsg_resmgr_setup",
                                 status,
                                 "Failed to create MessageQCopy handle!");
        }
        else {
            status = MessageQCopy_registerNotify(rpmsg_resmgr_state.mqHandle,
                                                 _rpmsg_resmgr_notify_cb);
            if (status >= 0) {
#if defined(USE_MEMMGR)
                rpmsg_resmgr_state.mqMemHandle = MessageQCopy_create (
                                               90,
                                               "rpmsg-memmgr",
                                               _rpmsg_memmgr_cb,
                                               NULL,
                                               &rpmsg_resmgr_state.memEndpoint);
                if (rpmsg_resmgr_state.mqMemHandle == NULL) {
                    MessageQCopy_delete (&rpmsg_resmgr_state.mqHandle);
                    /*! @retval ENOMEM Failed to register MQCopy handle! */
                    status = -ENOMEM;
                    GT_setFailureReason (curTrace,
                                       GT_4CLASS,
                                       "rpmsg_resmgr_setup",
                                       status,
                                       "Failed to create MessageQCopy handle!");
                }
                else {
#endif
                    for (i = 0; i < MultiProc_getNumProcessors(); i++) {
                        if (i != MultiProc_self())
                            _rpmsg_resmgr_create_conn(i,
                                                   rpmsg_resmgr_state.endpoint);
                    }
                    if (status < 0) {
#if defined(USE_MEMMGR)
                        MessageQCopy_delete (&rpmsg_resmgr_state.mqMemHandle);
#endif
                        MessageQCopy_delete (&rpmsg_resmgr_state.mqHandle);
                        /*! @retval ENOMEM Failed to register MQCopy handle! */
                        status = -ENOMEM;
                        GT_setFailureReason (curTrace,
                                           GT_4CLASS,
                                           "rpmsg_resmgr_setup",
                                           status,
                                           "Failed to register MQCopy handle!");
                    }
                    if (status >= 0) {
                         rpmsg_resmgr_state.isSetup = TRUE;
                    }
#if defined (USE_MEMMGR)
                }
#endif
            }
            else {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "_rpmsg_resmgr_setup",
                                     status,
                                     "Failed to register callback!");
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
    }
#endif /* if !defined(SYSLINK_BUILD_OPTIMIZE) */

    GT_0trace (curTrace, GT_LEAVE, "rpmsg_resmgr_setup");
    return status;
}


/*!
 *  @brief  Module destroy function.
 *
 *  @sa     rpmsg_resmgr_setup
 */
Void
rpmsg_resmgr_destroy (Void)
{
    int i = 0;

    GT_0trace (curTrace, GT_ENTER, "_rpmsg_resmgr_destroy");

    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        if (rpmsg_resmgr_state.objects[i])
            _rpmsg_resmgr_delete_conn(rpmsg_resmgr_state.objects[i]);
    }
#if defined(USE_MEMMGR)
    MessageQCopy_delete(&rpmsg_resmgr_state.mqMemHandle);
#endif
    MessageQCopy_delete(&rpmsg_resmgr_state.mqHandle);

    if (rpmsg_resmgr_state.gateHandle != NULL) {
        GateSpinlock_delete ((GateSpinlock_Handle *)
                                       &(rpmsg_resmgr_state.gateHandle));
    }

#if defined(SYSLINK_USE_IPU_PM)
    ipu_pm_ivahd_off();
#endif

    rpmsg_resmgr_state.isSetup = FALSE ;

    GT_0trace (curTrace, GT_LEAVE, "_rpmsgDrv_destroy");
}


/** ============================================================================
 *  Internal functions
 *  ============================================================================
 */


