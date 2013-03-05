/**
 *  @file   List.h
 *
 *  @brief      Creates a doubly linked list. It works as utils for other
 *              modules
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


#ifndef LIST_H_0XB734
#define LIST_H_0XB734

#include <stdbool.h>

/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
/*!
 *  @def    List_traverse
 *  @brief  Traverse the full list till the last element.
 */
#define List_traverse(x,y) for(x = (y)->next; \
                               x != (y);\
                                x = x->next)

#define List_traverse_safe(x, y, z) for(x = (z)->next, \
		                        y = (x)->next; \
		                        x != z; \
                                x = y, \
                                y = (x)->next)

#define List_elem(x,y,z) ((y *)((char *)(x)-(uint32_t)(&((y *)0)->z)))

#define List_traverse_elem(a, b, c, d) for(a = List_elem((c)->next, typeof(*a), d), \
		                        b = List_elem(a->d.next, typeof(*a), d); \
		                        &a->d != (c); \
                                a = b, b = List_elem(b->d.next, typeof(*b), d))

/*!
 *  @brief  Structure defining object for the list element.
 */
typedef struct List_Elem_tag {
    struct List_Elem_tag *  prev; /*!< Pointer to the previous element */
    struct List_Elem_tag *  next; /*!< Pointer to the next element */
} List_Elem;

/*! @brief Defines List handle type */
typedef List_Elem * List_Handle;


/* =============================================================================
 *  APIs
 * =============================================================================
 */

/*!
 *  @brief      Function to clear element contents.
 *
 *  @param      elem Element to be cleared
 *
 *  @sa
 */
void List_elemClear (List_Elem * elem);

/*!
 *  @brief      Function to check if list is empty.
 *
 *  @param      handle  Pointer to the list
 *
 *  @retval     TRUE    List is empty
 *  @retval     FALSE   List is not empty
 *
 *  @sa
 */
bool List_empty (List_Handle handle);

/*!
 *  @brief      Function to get first element of List.
 *
 *  @param      handle  Pointer to the list
 *
 *  @retval     NULL          Operation failed
 *  @retval     Valid-pointer Pointer to first element
 *
 *  @sa         List_put
 */
List_Elem * List_get (List_Handle handle);

/*!
 *  @brief      Function to insert element at the end of List.
 *
 *  @param      handle  Pointer to the list
 *  @param      elem    Element to be inserted
 *
 *  @sa         List_get
 */
void List_put (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Function to traverse to the next element in the list (non
 *              atomic)
 *
 *  @param      handle  Pointer to the list
 *  @param      elem    Pointer to the current element
 *
 *  @retval     NULL          Operation failed
 *  @retval     Valid-pointer Pointer to next element
 *
 *  @sa         List_prev
 */
List_Elem * List_next (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Function to traverse to the previous element in the list (non
 *              atomic)
 *
 *  @param      handle  Pointer to the list
 *  @param      elem    Pointer to the current element
 *
 *  @retval     NULL          Operation failed
 *  @retval     Valid-pointer Pointer to previous element
 *
 *  @sa         List_next
 */
List_Elem * List_prev (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Function to insert element before the existing element
 *              in the list.
 *
 *  @param      handle  Pointer to the list
 *  @param      newElem Element to be inserted
 *  @param      curElem Existing element before which new one is to be inserted
 *
 *  @sa         List_remove
 */
void List_insert (List_Handle handle, List_Elem * newElem, List_Elem * curElem);

/*!
 *  @brief      Function to removes element from the List.
 *
 *  @param      handle    Pointer to the list
 *  @param      elem      Element to be removed
 *
 *  @sa         List_insert
 */
void List_remove (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Function to put the element before head.
 *
 *  @param      handle    Pointer to the list
 *  @param      elem      Element to be added at the head
 *
 *  @sa         List_put
 */
void List_putHead (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Get element from front of List (non-atomic)
 *
 *  @param      handle  Pointer to the list
 *
 *  @retval     NULL          Operation failed
 *  @retval     Valid-pointer Pointer to removed element
 *
 *  @sa         List_enqueue, List_enqueueHead
 */
List_Elem* List_dequeue (List_Handle handle);

/*!
 *  @brief      Put element at end of List (non-atomic)
 *
 *  @param      handle  Pointer to the list
 *  @param      elem    Element to be put
 *
 *  @sa         List_dequeue
 */
void List_enqueue (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Put element at head of List (non-atomic)
 *
 *  @param      handle   Pointer to the list
 *  @param      elem     Element to be added
 *
 *  @sa         List_dequeue
 */
void List_enqueueHead (List_Handle handle, List_Elem * elem);

/*!
 *  @brief      Move an element to a new place in the list (non-atomic)
 *
 *  @param      handle   Pointer to the place to move the element after
 *  @param      elem     Element to be moved
 *
 *  @sa         List_move
 */
void List_move(List_Handle handle, List_Elem *list);

/*!
 *  @brief      Check if the list has only one element (non-atomic)
 *
 *  @param      handle   Pointer to list
 *
 *  @sa         List_is_singular
 */

int List_is_singular(List_Handle handle);

#endif /* LIST_H_0XB734 */
