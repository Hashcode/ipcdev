/**
 *  @file   ti-ipc.h
 *
 *  @brief      Definitions of ti-ipc internal types and structures.
 *
 *  ============================================================================
 *
 *  Copyright (c) 2013, Texas Instruments Incorporated
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


#ifndef TIIPC_H_0xf2ba
#define TIIPC_H_0xf2ba


#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @brief  Max number of user processes supported
 */
#define  MAX_PROCESSES          256u

/*!
 *  @brief  Max number of connections supported
 */
#define  MAX_CONNS              64

/*!
 *  @brief  Number of event entries to cache
 */
#define  CACHE_NUM              10

/*!
 *  @brief  Structure that defines the MsgList elem
 */
typedef struct MsgList {
    int index;
    int num_events;
    struct MsgList *next;
    struct MsgList *prev;
} MsgList_t;

/*!
 *  @brief  Structure that defines the Waiting Readers list element
 */
typedef struct WaitingReaders {
    int rcvid;
    struct WaitingReaders *next;
} WaitingReaders_t;


/* =============================================================================
 *  Function Prototypes
 * =============================================================================
 */
/*!
 *  @brief  Setup function for the ti-ipc module
 */
int ti_ipc_setup (Void);

/*!
 *  @brief  Destroy function for the ti-ipc module
 */
void ti_ipc_destroy (bool recover);



#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */


#endif /* TIIPC_H_0xf2ba */
