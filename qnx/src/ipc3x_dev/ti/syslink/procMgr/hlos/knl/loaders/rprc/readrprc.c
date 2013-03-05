/*
 *  Copyright (c) 2011, Texas Instruments Incorporated
 *  All rights reserved.
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
 */
/*
 *  ======== dbg-loader.c ========
 */

/* Standard header files */
#include <ti/syslink/Std.h>
#include <ti/syslink/utils/String.h>
#include <errno.h>

#include <sys/stat.h>

#include "_rprcloader.h"
#include "rprcfmt.h"
#include <Processor.h>

static int dump_image(Processor_Handle handle, void * data, int size);

static struct rproc_fw_resource * rprc_res_table = NULL;
static int rprc_res_table_len = 0;

/*
 *  ======== main ========
 */
int load_firmware_file(Processor_Handle handle, char * filename)
{
    FILE * fp;
    struct stat st;
    void * data;
    int size;
    int status;

    if (filename == NULL) {
        fprintf(stderr, "Usage: invalid filename\n");
        return (-1);
    }

    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "Param: could not open: %s\n", filename);
        return (-2);
    }

    if (fstat(fileno(fp), &st) != 0) {
        fprintf(stderr, "fstat failed with errno=%d\n", errno);
        return -1;
    }
    size = st.st_size;
    data = malloc(size);
    if (data == NULL) {
        fprintf(stderr, "Malloc failure in load_firmware_file\n");
        fclose(fp);
        return -1;
    }
    fread(data, 1, size, fp);
    fclose(fp);

    status = dump_image(handle, data, size);

    free(data);

    return status;
}

int unload_firmware_file(void)
{
    if (rprc_res_table) {
        free(rprc_res_table);
        rprc_res_table = NULL;
        rprc_res_table_len = 0;
    }

    return 0;
}

int get_resource_info(u32 type, char *name, u64 *da, u64 *pa, u32 *len)
{
    int i = 0;
    if (rprc_res_table == NULL)
        return (-1);

    for (i = 0; i < rprc_res_table_len; i++) {
        if (rprc_res_table[i].type == type && !strcmp((char *)rprc_res_table[i].name, name))
            break;
    }
    if (i == rprc_res_table_len)
        return (-2);

    *da = rprc_res_table[i].da;
    *pa = rprc_res_table[i].pa;
    *len = rprc_res_table[i].len;
    return 0;
}
/*
 *  ======== dump_resources ========
 */
static void dump_resources(Processor_Handle handle, struct rproc_fw_section * s)
{
    struct rproc_fw_resource * res = (struct rproc_fw_resource * )s->content;
    int i;
    u32 pa;

    printf("resource table: %d\n", sizeof(struct rproc_fw_resource));
    rprc_res_table = malloc(sizeof(struct rproc_fw_resource) * s->len);
    rprc_res_table_len = s->len;
    for (i = 0; i < s->len / sizeof(struct rproc_fw_resource); i++) {
        /* Lookup PA */
        if (res[i].type != RSC_DEVMEM) {
            Processor_translateAddr (handle, &pa, res[i].da);
            res[i].pa = (u64)pa;
        }
        printf("resource: %d, da: 0x%llx, pa: 0x%llx,len: %d, name: %s\n",
               res[i].type, res[i].da, res[i].pa, res[i].len, res[i].name);
        memcpy(&rprc_res_table[i], &res[i], sizeof(struct rproc_fw_resource));
    }
    printf("\n");
}

/*
 *  ======== dump_image ========
 */
static int dump_image(Processor_Handle handle, void * data, int size)
{
    struct rproc_fw_header *hdr;
    struct rproc_fw_section *s;

    hdr = (struct rproc_fw_header *)data;

    /* check that magic is what we expect */
    if (memcmp(hdr->magic, RPROC_FW_MAGIC, sizeof(hdr->magic))) {
        fprintf(stderr, "invalid magic number: %.4s\n", hdr->magic);
        return -1;
    }

    /* baseimage information */
    printf("magic number %.4s\n", hdr->magic);
    printf("header version %d\n", hdr->version);
    printf("header size %d\n", hdr->header_len);
    printf("header data\n%s\n", hdr->header);

    /* get the first section */
    s = (struct rproc_fw_section *)(hdr->header + hdr->header_len);

    while ((u8 *)s < (u8 *)(data + size)) {
        printf("section: %d, address: 0x%llx, size: 0x%x\n", s->type,
               s->da, s->len);

        if (s->type == FW_RESOURCE) {
            dump_resources(handle, s);
        }

        rprcloader_copy((u32)s->da, (u32)&s->content, (u32)s->len);

        s = ((void *)s->content) + s->len;
    }

    return 0;
}
