/*
* dlw_client.c
*
* RIDL implementation of client functions required by dynamic loader API.
* Please see list of client-required API functions in dload_api.h.
*
* This implementation of RIDL is expected to run on the DSP.  It uses C6x
* RTS functions for file I/O and memory management (both host and target
* memory).
*
* A loader that runs on a GPP for the purposes of loading C6x code onto a
* DSP will likely need to re-write all of the functions contained in this
* module.
*
* Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
*
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the
* distribution.
*
* Neither the name of Texas Instruments Incorporated nor the names of
* its contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
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
*
*/

#include "ArrayList.h"
#include "dload_api.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "dlw_debug.h"
#include "dlw_dsbt.h"
#include "dlw_trgmem.h"

#define LOADER_DEBUG 0
/*****************************************************************************/
/* Client Provided File I/O                                                  */
/*****************************************************************************/
/*****************************************************************************/
/* DLIF_FSEEK() - Seek to a position in specified file.                      */
/*****************************************************************************/
int DLIF_fseek(LOADER_FILE_DESC *stream, int32_t offset, int origin)
{
   return fseek(stream, offset, origin);
}

/*****************************************************************************/
/* DLIF_FTELL() - Return the current position in the given file.             */
/*****************************************************************************/
int32_t DLIF_ftell(LOADER_FILE_DESC *stream)
{
   return ftell(stream);
}

/*****************************************************************************/
/* DLIF_FREAD() - Read data from file into a host-accessible data buffer     */
/*      that can be accessed through "ptr".                                  */
/*****************************************************************************/
size_t DLIF_fread(void *ptr, size_t size, size_t nmemb,
                  LOADER_FILE_DESC *stream)
{
   return fread(ptr, size, nmemb, stream);
}

/*****************************************************************************/
/* DLIF_FCLOSE() - Close a file that was opened on behalf of the core        */
/*      loader. Core loader has ownership of the file pointer, but client    */
/*      has access to file system.                                           */
/*****************************************************************************/
int32_t DLIF_fclose(LOADER_FILE_DESC *fd)
{
   return fclose(fd);
}

/*****************************************************************************/
/* Client Provided Host Memory Management                                    */
/*****************************************************************************/
/*****************************************************************************/
/* DLIF_MALLOC() - Allocate host memory suitable for loader scratch space.   */
/*****************************************************************************/
void* DLIF_malloc(size_t size)
{
   return malloc(size*sizeof(uint8_t));
}

/*****************************************************************************/
/* DLIF_FREE() - Free host memory previously allocated with DLIF_malloc().   */
/*****************************************************************************/
void DLIF_free(void* ptr)
{
   free(ptr);
}

/*****************************************************************************/
/* Client Provided Target Memory Management                                  */
/*****************************************************************************/

/*****************************************************************************/
/* RIDL-Specific hack to facilitate target memory allocation.                */
/*****************************************************************************/
/*****************************************************************************/
/* DLIF_ALLOCATE() - Return the load address of the segment/section          */
/*      described in its parameters and record the run address in            */
/*      run_address field of DLOAD_MEMORY_REQUEST.                           */
/*****************************************************************************/
BOOL DLIF_allocate(struct DLOAD_MEMORY_REQUEST *targ_req)
{
   /*------------------------------------------------------------------------*/
   /* Get pointers to API segment and file descriptors.                      */
   /*------------------------------------------------------------------------*/
   struct DLOAD_MEMORY_SEGMENT* obj_desc = targ_req->segment;
   LOADER_FILE_DESC* f = targ_req->fp;

   /*------------------------------------------------------------------------*/
   /* Request target memory for this segment from the "blob".                */
   /*------------------------------------------------------------------------*/
   if (!DLTMM_malloc(targ_req, obj_desc))
   {
      DLIF_error(DLET_MEMORY, "Failed to allocate target memory for segment.\n");
      return 0;
   }

   /*------------------------------------------------------------------------*/
   /* As required by API, copy the described segment into memory from file.  */
   /* We're the client, not the loader, so we can use fseek() and fread().   */
   /*------------------------------------------------------------------------*/
   /* ??? I don't think we want to do this if we are allocating target       */
   /*   memory for the run only placement of this segment.  If it is the     */
   /*   load placement or both load and run placement, then we can do the    */
   /*   copy.                                                                */
   /*------------------------------------------------------------------------*/
   memset(targ_req->host_address, 0, obj_desc->memsz_in_bytes);
   fseek(f,targ_req->offset,SEEK_SET);
   fread(targ_req->host_address,obj_desc->objsz_in_bytes,1,f);

   /*------------------------------------------------------------------------*/
   /* Once we have target address for this allocation, add debug information */
   /* about this segment to the debug record for the module that is          */
   /* currently being loaded.                                                */
   /*------------------------------------------------------------------------*/
   if (DLL_debug)
   {
      /*---------------------------------------------------------------------*/
      /* Add information about this segment's location to the segment debug  */
      /* information associated with the module that is currently being      */
      /* loaded.                                                             */
      /*---------------------------------------------------------------------*/
      /* ??? We need a way to determine whether the target address in the    */
      /*        segment applies to the load address of the segment or the    */
      /*        run address.  For the time being, we assume that it applies  */
      /*        to both (that is, the dynamic loader does not support        */
      /*        separate load and run placement for a given segment).        */
      /*---------------------------------------------------------------------*/
      DLDBG_add_segment_record(obj_desc);
   }

#if LOADER_DEBUG
   if (debugging_on)
      printf("DLIF_allocate: buffer 0x%x\n", targ_req->host_address);
#endif

   /*------------------------------------------------------------------------*/
   /* Target memory request was successful.                                  */
   /*------------------------------------------------------------------------*/
   return 1;
}

/*****************************************************************************/
/* DLIF_RELEASE() - Unmap or free target memory that was previously          */
/*      allocated by DLIF_allocate().                                        */
/*****************************************************************************/
BOOL DLIF_release(struct DLOAD_MEMORY_SEGMENT* ptr)
{
#if LOADER_DEBUG
   if (debugging_on)
      printf("DLIF_free: %d bytes starting at 0x%x\n",
                                     ptr->memsz_in_bytes, ptr->target_address);
#endif

   /*------------------------------------------------------------------------*/
   /* Find the target memory packet associated with this address and mark it */
   /* as available (will also merge with adjacent free packets).             */
   /*------------------------------------------------------------------------*/
   DLTMM_free(ptr->target_address);

   return 1;
}

/*****************************************************************************/
/* DLIF_COPY() - Copy data from file to host-accessible memory.              */
/*      Returns a host pointer to the data in the host_address field of the  */
/*      DLOAD_MEMORY_REQUEST object.                                         */
/*****************************************************************************/
BOOL DLIF_copy(struct DLOAD_MEMORY_REQUEST* targ_req)
{
   targ_req->host_address = (void*)(targ_req->segment->target_address);
   return 1;
}

/*****************************************************************************/
/* DLIF_READ() - Read content from target memory address into host-          */
/*      accessible buffer.                                                   */
/*****************************************************************************/
BOOL DLIF_read(void *ptr, size_t size, size_t nmemb, TARGET_ADDRESS src)
{
   if (!memcpy(ptr, (const void *)src, size * nmemb))
      return 0;

   return 1;
}

/*****************************************************************************/
/* DLIF_WRITE() - Write updated (relocated) segment contents to target       */
/*      memory.                                                              */
/*****************************************************************************/
BOOL DLIF_write(struct DLOAD_MEMORY_REQUEST* req)
{
   /*------------------------------------------------------------------------*/
   /* Nothing to do since we are relocating directly into target memory.     */
   /*------------------------------------------------------------------------*/
   return 1;
}

/*****************************************************************************/
/* DLIF_EXECUTE() - Transfer control to specified target address.            */
/*****************************************************************************/
int32_t DLIF_execute(TARGET_ADDRESS exec_addr)
{
   /*------------------------------------------------------------------------*/
   /* This call will only work if the host and target are the same instance. */
   /* The compiler may warn about this conversion from an object to a        */
   /* function pointer.                                                      */
   /*------------------------------------------------------------------------*/
   return ((int32_t(*)())(exec_addr))();
}


/*****************************************************************************/
/* Client Provided Communication Mechanisms to assist with creation of       */
/* DLLView debug information.  Client needs to know exactly when a segment   */
/* is being loaded or unloaded so that it can keep its debug information     */
/* up to date.                                                               */
/*****************************************************************************/
/*****************************************************************************/
/* DLIF_LOAD_DEPENDENT() - Perform whatever maintenance is needed in the     */
/*      client when loading of a dependent file is initiated by the core     */
/*      loader.  Open the dependent file on behalf of the core loader,       */
/*      then invoke the core loader to get it into target memory. The core   */
/*      loader assumes ownership of the dependent file pointer and must ask  */
/*      the client to close the file when it is no longer needed.            */
/*                                                                           */
/*      If debug support is needed under the Braveheart model, then create   */
/*      a host version of the debug module record for this object.  This     */
/*      version will get updated each time we allocate target memory for a   */
/*      segment that belongs to this module.  When the load returns, the     */
/*      client will allocate memory for the debug module from target memory  */
/*      and write the host version of the debug module into target memory    */
/*      at the appropriate location.  After this takes place the new debug   */
/*      module needs to be added to the debug module list.  The client will  */
/*      need to update the tail of the DLModules list to link the new debug  */
/*      module onto the end of the list.                                     */
/*                                                                           */
/*****************************************************************************/
int DLIF_load_dependent(const char* so_name)
{
   /*------------------------------------------------------------------------*/
   /* Find the path and file name associated with the given so_name in the   */
   /* client's file registry.                                                */
   /*------------------------------------------------------------------------*/
   /* If we can't find the so_name in the file registry (or if the registry  */
   /* is not implemented yet), we'll open the file using the so_name.        */
   /*------------------------------------------------------------------------*/
   int to_ret = 0;
   FILE* fp = fopen(so_name, "rb");

   /*------------------------------------------------------------------------*/
   /* We need to make sure that the file was properly opened.                */
   /*------------------------------------------------------------------------*/
   if (!fp)
   {
      DLIF_error(DLET_FILE, "Can't open dependent file '%s'.\n", so_name);
      return 0;
   }

   /*------------------------------------------------------------------------*/
   /* If the dynamic loader is providing debug support for a DLL View plug-  */
   /* in or script of some sort, then we are going to create a host version  */
   /* of the debug module record for the so_name module.                     */
   /*------------------------------------------------------------------------*/
   /* In the Braveheart view of the world, debug support is only to be       */
   /* provided if the DLModules symbol is defined in the base image.         */
   /* We will set up a DLL_debug flag when the command to load the base      */
   /* image is issued.                                                       */
   /*------------------------------------------------------------------------*/
   if (DLL_debug)
      DLDBG_add_host_record(so_name);

   /*------------------------------------------------------------------------*/
   /* Tell the core loader to proceed with loading the module.               */
   /*------------------------------------------------------------------------*/
   /* Note that the client is turning ownership of the file pointer over to  */
   /* the core loader at this point. The core loader will need to ask the    */
   /* client to close the dependent file when it is done using the dependent */
   /* file pointer.                                                          */
   /*------------------------------------------------------------------------*/
   to_ret = DLOAD_load(fp, 0, NULL);

   /*------------------------------------------------------------------------*/
   /* If the dependent load was successful, update the DLModules list in     */
   /* target memory as needed.                                               */
   /*------------------------------------------------------------------------*/
   if (to_ret != 0)
   {
      /*---------------------------------------------------------------------*/
      /* We will need to copy the information from our host version of the   */
      /* debug record into actual target memory.                             */
      /*---------------------------------------------------------------------*/
      if (DLL_debug)
      {
         /*---------------------------------------------------------------*/
         /* Allocate target memory for the module's debug record.  Use    */
         /* host version of the debug information to determine how much   */
         /* target memory we need and how it is to be filled in.          */
         /*---------------------------------------------------------------*/
         /* Note that we don't go through the normal API functions to get */
         /* target memory and write the debug information since we're not */
         /* dealing with object file content here.  The DLL View debug is */
         /* supported entirely on the client side.                        */
         /*---------------------------------------------------------------*/
         DLDBG_add_target_record(to_ret);
      }
   }

   /*------------------------------------------------------------------------*/
   /* Report failure to load dependent.                                      */
   /*------------------------------------------------------------------------*/
   else
      DLIF_error(DLET_MISC, "Failed load of dependent file '%s'.\n", so_name);

   return to_ret;
}

/*****************************************************************************/
/* DLIF_UNLOAD_DEPENDENT() - Perform whatever maintenance is needed in the   */
/*      client when unloading of a dependent file is initiated by the core   */
/*      loader.  Invoke the DLOAD_unload() function to get the core loader   */
/*      to release any target memory that is associated with the dependent   */
/*      file's segments.                                                     */
/*****************************************************************************/
void DLIF_unload_dependent(uint32_t handle)
{
   /*------------------------------------------------------------------------*/
   /* If the specified module is no longer needed, DLOAD_load() will spin    */
   /* through the object descriptors associated with the module and free up  */
   /* target memory that was allocated to any segment in the module.         */
   /*------------------------------------------------------------------------*/
   /* If DLL debugging is enabled, find module debug record associated with  */
   /* this module and remove it from the DLL debug list.                     */
   /*------------------------------------------------------------------------*/
   if (DLOAD_unload(handle))
   {
      DSBT_release_entry(handle);
      if (DLL_debug) DLDBG_rm_target_record(handle);
   }
}

/*****************************************************************************/
/* Client Provided API Functions to Support Logging Warnings/Errors          */
/*****************************************************************************/

/*****************************************************************************/
/* DLIF_WARNING() - Write out a warning message from the core loader.        */
/*****************************************************************************/
void DLIF_warning(LOADER_WARNING_TYPE wtype, const char *fmt, ...)
{
   va_list ap;
   va_start(ap,fmt);
   printf("<< D L O A D >> WARNING: ");
   vprintf(fmt,ap);
   va_end(ap);
}

/*****************************************************************************/
/* DLIF_ERROR() - Write out an error message from the core loader.           */
/*****************************************************************************/
void DLIF_error(LOADER_ERROR_TYPE etype, const char *fmt, ...)
{
   va_list ap;
   va_start(ap,fmt);
   printf("<< D L O A D >> ERROR: ");
   vprintf(fmt,ap);
   va_end(ap);
}

/*****************************************************************************
 * END API FUNCTION DEFINITIONS
 *****************************************************************************/
