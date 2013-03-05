/*
 * Remote processor messaging
 *
 * Copyright (c) 2011-2013 Texas Instruments. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name Texas Instruments nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_RPMSG_RESMGR_H
#define _LINUX_RPMSG_RESMGR_H

#include <stdint.h>

#define __packed __attribute__ ((packed))

#define MAX_NUM_SDMA_CHANNELS   16

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(*(x)))

/* Names should match the IpcResource_Type definitions */
static char * rpmsg_resmgr_names[] = {
    "omap-gptimer",
    "iva",
    "iva_seq0",
    "iva_seq1",
    "l3bus",
    "omap-iss",
    "omap-fdif",
    "omap-sl2if",
    "omap-auxclk",
    "regulator",
    "gpio",
    "omap-sdma",
    "ipu_c0",
    "dsp_c0",
    "i2c",
    "camera",
    "led"
};

static inline int32_t rpmsg_resmgr_to_type(char * name)
{
    int i = 0;
    /* check for NULL-terminated string */
    for (i = 0; i < 16; i++) {
        if (name[i] == '\0')
            break;
    }
    if (i == 16) return -1;
    for (i = 0; i < ARRAY_SIZE(rpmsg_resmgr_names); i++) {
        if (!strcmp(rpmsg_resmgr_names[i], name))
            return i;
    }
    if (!strcmp("rproc", name)) {
        return -2;
    }
    return -1;
}

enum {
    RPRM_GPTIMER    = 0,
    RPRM_IVAHD  = 1,
    RPRM_IVASEQ0    = 2,
    RPRM_IVASEQ1    = 3,
    RPRM_L3BUS  = 4,
    RPRM_ISS    = 5,
    RPRM_FDIF   = 6,
    RPRM_SL2IF  = 7,
    RPRM_AUXCLK = 8,
    RPRM_REGULATOR  = 9,
    RPRM_GPIO   = 10,
    RPRM_SDMA   = 11,
    RPRM_IPU    = 12,
    RPRM_DSP    = 13,
    RPRM_I2C    = 14,
    RPRM_CAMERA = 15,
    RPRM_LED    = 16,
    RPRM_MAX
};

enum {
    RPRM_CONNECT        = 0,
    RPRM_DISCONNECT     = 1,
    RPRM_REQ_ALLOC      = 2,
    RPRM_REQ_FREE       = 3,
    RPRM_REQ_CONSTRAINTS    = 4,
    RPRM_REL_CONSTRAINTS    = 5,
    RPRM_REQ_DATA       = 6,
};

enum {
    RPRM_SCALE      = 0x1,
    RPRM_LATENCY        = 0x2,
    RPRM_BANDWIDTH      = 0x4,
};

struct rprm_alloc_data {
    char res_name[16];
    char data[];
} __packed;

struct rprm_free_data {
    uint32_t res_id;
} __packed;

struct rprm_constraints_data {
    uint32_t mask;
    long frequency;
    long bandwidth;
    long latency;
};

struct rprm_constraint {
    uint32_t res_id;
    struct rprm_constraints_data data;
} __packed;

struct rprm_request {
    uint32_t acquire;
    char data[];
} __packed;

struct rprm_ack_data {
    uint32_t res_id;
    uint32_t base;
    char data[];
} __packed;

struct rprm_ack {
    uint32_t acquire;
    uint32_t ret;
    char data[];
} __packed;

struct rprm_proc {
    char name[16];
} __packed;

struct rprm_reqdata {
    uint32_t res_id;
    uint32_t type;
    char data[];
} __packed;

enum {
    RPRM_REQTYPE_MAXFREQ = 0
};

struct rprm_gpt {
    uint32_t id;
    uint32_t src_clk;
};

struct rprm_auxclk {
    uint32_t id;
    uint32_t clk_rate;
    uint32_t parent_src_clk;
    uint32_t parent_src_clk_rate;
};

struct rprm_regulator {
    uint32_t id;
    uint32_t min_uv;
    uint32_t max_uv;
};

struct rprm_gpio {
    uint32_t id;
};

/**
 * struct rprm_i2c - resource i2c
 * @id: i2c id
 *
 * meant to store the i2c related information
 */
struct rprm_i2c {
    uint32_t id;
};

struct rprm_sdma {
    uint32_t num_chs;
    int32_t channels[MAX_NUM_SDMA_CHANNELS];
};

struct rprm_camera {
    uint32_t mode;
    uint32_t on;
};

struct rprm_led {
    uint32_t mode;
    uint32_t intensity;
};

struct rprm_freq {
    uint32_t value;
};

#endif /* _LINUX_RPMSG_RESMGR_H */
