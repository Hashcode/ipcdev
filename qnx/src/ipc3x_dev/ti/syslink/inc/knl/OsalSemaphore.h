/**
 *  @file   OsalSemaphore.h
 *
 *  @brief      Kernel utils Semaphore interface definitions.
 *
 *              This abstracts the Semaphore interface in Kernel code and
 *              is implemented using the wait queues. It has interfaces
 *              for creating, destroying, waiting and triggering the Semaphores.
 *
 *
 *  @ver        02.00.00.46_alpha1
 *
 *  ============================================================================
 *
 *  Copyright (c) 2008-2009, Texas Instruments Incorporated
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


#ifndef OSALSEMAPHORE_H_0xF6D6
#define OSALSEMAPHORE_H_0xF6D6


/* OSAL and utils */


#if defined (__cplusplus)
extern "C" {
#endif


/*!
 *  @def    OSALSEMAPHORE_MODULEID
 *  @brief  Module ID for OsalSemaphore OSAL module.
 */
#define OSALSEMAPHORE_MODULEID                 (UInt16) 0xF6D6

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/*!
* @def   OSALSEMAPHORE_STATUSCODEBASE
* @brief Stauts code base for MEMORY module.
*/
#define OSALSEMAPHORE_STATUSCODEBASE      (OSALSEMAPHORE_MODULEID << 12u)

/*!
* @def   OSALSEMAPHORE_MAKE_FAILURE
* @brief Convert to failure code.
*/
#define OSALSEMAPHORE_MAKE_FAILURE(x)  ((Int) (0x80000000  \
                                        + (OSALSEMAPHORE_STATUSCODEBASE +(x))))
/*!
* @def   OSALSEMAPHORE_MAKE_SUCCESS
* @brief Convert to success code.
*/
#define OSALSEMAPHORE_MAKE_SUCCESS(x)      (OSALSEMAPHORE_STATUSCODEBASE + (x))

/*!
* @def   OSALSEMAPHORE_E_MEMORY
* @brief Indicates OsalSemaphore alloc/free failure.
*/
#define OSALSEMAPHORE_E_MEMORY             OSALSEMAPHORE_MAKE_FAILURE(1)

/*!
* @def   OSALSEMAPHORE_E_INVALIDARG
* @brief Invalid argument provided
*/
#define OSALSEMAPHORE_E_INVALIDARG         OSALSEMAPHORE_MAKE_FAILURE(2)

/*!
* @def   OSALSEMAPHORE_E_FAIL
* @brief Generic failure
*/
#define OSALSEMAPHORE_E_FAIL               OSALSEMAPHORE_MAKE_FAILURE(3)

/*!
* @def   OSALSEMAPHORE_E_TIMEOUT
* @brief A timeout occurred
*/
#define OSALSEMAPHORE_E_TIMEOUT            OSALSEMAPHORE_MAKE_FAILURE(4)

/*!
 *  @def    OSALSEMAPHORE_E_HANDLE
 *  @brief  Invalid handle provided
 */
#define OSALSEMAPHORE_E_HANDLE             OSALSEMAPHORE_MAKE_FAILURE(5)

/*!
 *  @def    OSALSEMAPHORE_E_WAITNONE
 *  @brief  WAIT_NONE timeout value was provided, but semaphore was not
 *          available.
 */
#define OSALSEMAPHORE_E_WAITNONE           OSALSEMAPHORE_MAKE_FAILURE(6)

/*!
* @def   OSALSEMAPHORE_SUCCESS
* @brief Operation successfully completed
*/
#define OSALSEMAPHORE_SUCCESS              OSALSEMAPHORE_MAKE_SUCCESS(0)


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @def    OSALSEMAPHORE_WAIT_FOREVER
 *  @brief  Indicates forever wait for APIs that can wait.
 */
#define OSALSEMAPHORE_WAIT_FOREVER           (~((UInt32) 0u))

/*!
 *  @def    OSALSEMAPHORE_WAIT_NONE
 *  @brief  Indicates zero wait for APIs that can wait.
 */
#define OSALSEMAPHORE_WAIT_NONE               ((UInt32) 0u)

/*!
 *  @def    OSALSEMAPHORE_TYPE_VALUE
 *  @brief  Returns the value of semaphore type (binary/counting)
 */
#define OSALSEMAPHORE_TYPE_VALUE(type)        (type & 0x0000FFFF)

/*!
 *  @def    OSALSEMAPHORE_INTTYPE_VALUE
 *  @brief  Returns the value of semaphore interruptability type
 */
#define OSALSEMAPHORE_INTTYPE_VALUE(type)     (type & 0xFFFF0000)

/*!
 *  @brief  Declaration for the OsalSemaphore object handle.
 *          Definition of OsalSemaphore_Object is not exposed.
 */
typedef struct OsalSemaphore_Object * OsalSemaphore_Handle;

/*!
 *  @brief   Enumerates the types of semaphores
 */
typedef enum {
    OsalSemaphore_Type_Binary          = 0x00000000,
    /*!< Binary semaphore */
    OsalSemaphore_Type_Counting        = 0x00000001,
    /*!< Counting semaphore */
    OsalSemaphore_Type_EndValue        = 0x00000002
    /*!< End delimiter indicating start of invalid values for this enum */
} OsalSemaphore_Type;

/*!
 *  @brief   Enumerates the interruptible/non-interruptible types.
 */
typedef enum {
    OsalSemaphore_IntType_Interruptible    = 0x00000000,
    /*!< Waits on this mutex are interruptible */
    OsalSemaphore_IntType_Noninterruptible = 0x00010000,
    /*!< Waits on this mutex are non-interruptible */
    OsalSemaphore_IntType_EndValue         = 0x00020000
    /*!< End delimiter indicating start of invalid values for this enum */
} OsalSemaphore_IntType;


/* =============================================================================
 *  APIs
 * =============================================================================
 */
/* Creates the semaphore object. */
OsalSemaphore_Handle OsalSemaphore_create (UInt32 semType);

/* Deletes the semaphore object */
Int OsalSemaphore_delete (OsalSemaphore_Handle * semHandle);

/* Wait on the said Semaphore in the kernel thread context */
Int OsalSemaphore_pend (OsalSemaphore_Handle semHandle, UInt32 timeout);

/* Signal the semaphore and make it available for other threads. */
Int OsalSemaphore_post (OsalSemaphore_Handle semHandle);


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ifndef OSALSEMAPHORE_H_0xF6D6 */
