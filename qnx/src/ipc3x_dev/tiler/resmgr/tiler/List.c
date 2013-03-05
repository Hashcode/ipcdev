/*
 *  @file   List.c
 *
 *  @brief      Creates a doubly linked list.
 *
 *
 *  @ver        02.00.00.53_alpha2
 *
 *  ============================================================================
 *
 *  Copyright (c) 2010, Texas Instruments Incorporated
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


#include "proto.h"
#include "List.h"

/* =============================================================================
 *  All success and failure codes for the module. Defined here because they are
 *  only used internally.
 * =============================================================================
 */

/*!
 *  @def    List_S_BUSY
 *  @brief  The resource is still in use
 */
#define List_S_BUSY           2

/*!
 *  @def    List_S_ALREADYSETUP
 *  @brief  The module has been already setup
 */
#define List_S_ALREADYSETUP   1

/*!
 *  @def    List_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define List_S_SUCCESS        0

/*!
 *  @def    List_E_FAIL
 *  @brief  Generic failure.
 */
#define List_E_FAIL           -1

/*!
 *  @def    List_E_INVALIDARG
 *  @brief  Argument passed to function is invalid.
 */
#define List_E_INVALIDARG     -2

/*!
 *  @def    List_E_MEMORY
 *  @brief  Operation resulted in memory failure.
 */
#define List_E_MEMORY         -3

/*!
 *  @def    List_E_ALREADYEXISTS
 *  @brief  The specified entity already exists.
 */
#define List_E_ALREADYEXISTS  -4

/*!
 *  @def    List_E_NOTFOUND
 *  @brief  Unable to find the specified entity.
 */
#define List_E_NOTFOUND       -5

/*!
 *  @def    List_E_TIMEOUT
 *  @brief  Operation timed out.
 */
#define List_E_TIMEOUT        -6

/*!
 *  @def    List_E_INVALIDSTATE
 *  @brief  Module is not initialized.
 */
#define List_E_INVALIDSTATE   -7

/*!
 *  @def    List_E_OSFAILURE
 *  @brief  A failure occurred in an OS-specific call
 */
#define List_E_OSFAILURE      -8

/*!
 *  @def    List_E_RESOURCE
 *  @brief  Specified resource is not available
 */
#define List_E_RESOURCE       -9

/*!
 *  @def    List_E_RESTART
 *  @brief  Operation was interrupted. Please restart the operation
 */
#define List_E_RESTART        -10

/* Function to remove specific elem from list */
void
List_elemClear (List_Elem * elem)
{
    if (elem != NULL) {
         elem->next = elem->prev = elem;
    }
}


/* Function to check if list is empty */
bool
List_empty (List_Handle handle)
{
    bool          isEmpty = false;
    List_Elem * obj     = (List_Elem *) handle;
    List_Elem *elem = NULL;

    if (handle != NULL) {
        elem = obj->next;
        if ((elem == obj) && (elem == obj->prev)) {
            /*! @retval TRUE List is empty */
            isEmpty = true;
        }
    }

    /*! @retval FALSE List is not empty */
    return isEmpty;
}


/* Function to get front element */
List_Elem *
List_get (List_Handle handle)
{
    List_Elem *   elem = NULL;

    if (handle != NULL) {
        elem = List_dequeue(handle);
    }

    /*! @retval Valid-pointer Pointer to first element */
    return elem ;
}


/* Function to put elem at the end */
void
List_put (List_Handle handle, List_Elem * elem)
{
    if (handle != NULL && elem != NULL) {
        List_enqueue (handle, elem);
    }
}



/* Function to get next elem of current one (non-atomic) */
List_Elem *
List_next (List_Handle handle, List_Elem * elem)
{
    List_Elem *   retElem = NULL;
    List_Elem * obj     = (List_Elem *) handle;

    if (handle != NULL) {
        /* elem == NULL -> start at the head */
        if (elem == NULL) {
            retElem = obj->next;
        }
        else {
            retElem = elem->next;
        }

        if (retElem == (List_Elem *) obj) {
            /*! @retval NULL List reached end or list is empty */
            retElem = NULL;
        }

    }

    /*! @retval Valid-pointer Pointer to the next element */
    return retElem;
}


/* Function to get previous elem of current one (non-atomic) */
List_Elem *
List_prev (List_Handle handle, List_Elem * elem)
{
    List_Elem *   retElem = NULL;
    List_Elem * obj     = (List_Elem *) handle;

    if (handle != NULL && elem != NULL) {
        /* elem == NULL -> start at the head */
        if (elem == NULL) {
            retElem = obj->prev;
        }
        else {
            retElem = elem->prev;
        }

        if (retElem == (List_Elem *)obj) {
            /*! @retval NULL List reached end or list is empty */
            retElem = NULL;
        }
    }

    /*! @retval Valid-pointer Pointer to the prev element */
    return retElem;
}


/* Function to insert elem before existing elem */
void
List_insert  (List_Handle handle, List_Elem * newElem, List_Elem * curElem)
{
    if (handle != NULL && newElem != NULL && curElem != NULL) {
            /* Cannot directly call enqueue since the object has other fields */
            newElem->next       = curElem;
            newElem->prev       = curElem->prev;
            newElem->prev->next = newElem;
            curElem->prev       = newElem;
    }
}


/* Function to remove specific elem from list */
void
List_remove (List_Handle handle, List_Elem * elem)
{
    if (elem != NULL) {
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
    }
}


/* Function to put element before head */
void
List_putHead (List_Handle handle, List_Elem *elem)
{
    if (handle != NULL && elem != NULL) {
        List_enqueueHead (handle, elem);
    }
}


/* Get element from front of List (non-atomic) */
List_Elem *
List_dequeue (List_Handle handle)
{
    List_Elem *   elem = NULL;
    List_Elem * obj  = (List_Elem *) handle;

    elem = obj->next;
    /* See if the List was empty */
    if (elem == (List_Elem *)obj) {
        /*! @retval NULL List is empty */
        elem = NULL;
    }
    else {
        obj->next   = elem->next;
        elem->next->prev = obj;
    }
    return elem;
}


/* Put element at end of List (non-atomic) */
void
List_enqueue (List_Handle handle, List_Elem * elem)
{
    List_Elem * obj    = (List_Elem *) handle;

    if (handle != NULL && elem != NULL) {
        elem->next      = obj;
        elem->prev      = obj->prev;
        obj->prev->next = elem;
        obj->prev       = elem;
    }
}


/* Put element at head of List (non-atomic) */
void
List_enqueueHead (List_Handle handle, List_Elem * elem)
{
	List_Elem * obj    = (List_Elem *) handle;

    if (handle != NULL && elem != NULL) {
        elem->next      = obj->next;
        elem->prev      = obj;
        obj->next->prev = elem;
        obj->next       = elem;
    }
}


/* Move list element to new place in list (non-atomic) */
void
List_move(List_Handle handle, List_Elem *list)
{
	    List_remove(handle, list);
        List_enqueueHead(handle, list);
}


/* Check if the list has only one element (non-atomic) */
int
List_is_singular(List_Handle handle)
{
		List_Elem *elem = (List_Elem *)handle;
        return !List_empty(handle) && (elem->next == elem->prev);
}
