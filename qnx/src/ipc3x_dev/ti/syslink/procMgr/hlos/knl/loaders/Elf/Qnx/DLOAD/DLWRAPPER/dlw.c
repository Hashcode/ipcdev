/*
* dlw.c
*
* Client-side driver of reference implementation of the dynamic loader for
* C6x.
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ArrayList.h"
#include "symtab.h"
#include "dload.h"
#include "dload_api.h"
#include "util.h"
#include "dlw_debug.h"
#include "dlw_dsbt.h"
#include "dlw_trgmem.h"

int export_var=100;

/*---------------------------------------------------------------------------*/
/* Global function pointer that will be set to point at whatever entry       */
/* point that we want to be in effect when the "execute" command is          */
/* specified.                                                                */
/*---------------------------------------------------------------------------*/
static int (*loaded_program)() = NULL;

/*---------------------------------------------------------------------------*/
/* Handle of the loaded program.                                             */
/*---------------------------------------------------------------------------*/
static uint32_t prog_handle;

/*****************************************************************************/
/* Loader Testing Shell Functions                                            */
/*                                                                           */
/* These are intended for debugging and testing the loader.                  */
/*****************************************************************************/

/*****************************************************************************/
/* LOAD_EXECUTABLE() - Invoke the loader to create an executable image of    */
/*      the specified program, saving the loader's return value as the       */
/*      future entry point.                                                  */
/*****************************************************************************/
static void load_executable(const char* file_name, int argc, char** argv)
{
   /*------------------------------------------------------------------------*/
   /* Open specified file from "load" command, load it, then close the file. */
   /*------------------------------------------------------------------------*/

   /*------------------------------------------------------------------------*/
   /* NOTE!! We require that any shared objects that we are trying to load   */
   /* be in the current directory.                                           */
   /*------------------------------------------------------------------------*/
   /*------------------------------------------------------------------------*/
   /* If there is registry available, we'll need to look up the given        */
   /* file_name in the registry to find the actual pathname to be used       */
   /* while opening the file.  Otherwise, we'll just use the given file      */
   /* name.                                                                  */
   /*------------------------------------------------------------------------*/
   FILE* fp = fopen(file_name, "rb");

   /*------------------------------------------------------------------------*/
   /* Were we able to open the file successfully?                            */
   /*------------------------------------------------------------------------*/
   if (!fp)
   {
      DLIF_error(DLET_FILE, "Failed to open file '%s'.\n", file_name);
      return;
   }

   /*------------------------------------------------------------------------*/
   /* If we have a DLModules symbol available in the base image, then we     */
   /* need to create debug information for this file in target memory to     */
   /* support whatever DLL View plug-in or script is implemented in the      */
   /* debugger.                                                              */
   /*------------------------------------------------------------------------*/
   if (DLL_debug)
      DLDBG_add_host_record(file_name);

   /*------------------------------------------------------------------------*/
   /* Now, we are ready to start loading the specified file onto the target. */
   /*------------------------------------------------------------------------*/
   prog_handle = DLOAD_load(fp, argc, argv);
   fclose(fp);

   /*------------------------------------------------------------------------*/
   /* If the load was successful, then we'll need to write the debug         */
   /* information for this file into target memory.                          */
   /*------------------------------------------------------------------------*/
   if (prog_handle)
   {
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
	 DLDBG_add_target_record(prog_handle);
      }

      /*---------------------------------------------------------------------*/
      /* Find entry point associated with loaded program's handle, set up    */
      /* entry point in "loaded_program".                                    */
      /*---------------------------------------------------------------------*/
      DLOAD_get_entry_point(prog_handle, (TARGET_ADDRESS)(&loaded_program));
      printf("loaded_program: 0x%x\n", loaded_program);
   }

   /*------------------------------------------------------------------------*/
   /* Report failure to load an object file.                                 */
   /*------------------------------------------------------------------------*/
   else
      DLIF_error(DLET_MISC, "Failed load of file '%s'.\n", file_name);
}

/*****************************************************************************/
/* EXECUTE_PROGRAM() - Execute loaded program and print return value.        */
/*****************************************************************************/
static void execute_program(int argc, char** argv)
{
   /*------------------------------------------------------------------------*/
   /* Have we got an entry point that we begin executing at?                 */
   /*------------------------------------------------------------------------*/
   if (loaded_program == NULL)
   {
      fprintf(stderr,"<< D O L T >> ERROR: No program has been loaded.\n");
      return;
   }

   /*------------------------------------------------------------------------*/
   /* Call loaded program at the entry point in "loaded_program".            */
   /*------------------------------------------------------------------------*/
   printf("Return value: %d\n", loaded_program(argc,argv));
}

/*---------------------------------------------------------------------------*/
/* Global flag to control debug output.                                      */
/*---------------------------------------------------------------------------*/
#if LOADER_DEBUG
BOOL debugging_on = 0;
#endif

/*---------------------------------------------------------------------------*/
/* Global flag to enable profiling.                                          */
/*---------------------------------------------------------------------------*/
#if LOADER_DEBUG || LOADER_PROFILE
BOOL profiling_on = 0;
#endif

/*---------------------------------------------------------------------------*/
/* User command data structure                                               */
/*---------------------------------------------------------------------------*/
enum command_id_t
{
    INFORMATION,
    LOAD_EXECUTABLE,
    CORE,
    EXIT,
    HELP,
    EXECUTE_PROGRAM,
    UNLOAD_PROGRAM,
    BASE_IMAGE,
    DEBUG,
    PROFILE,
    COMMENT,
    DUMP_TRGMEM,
    CORE_VERSION,
    ERROR_ID
};

struct commands_t
{
    const char *command;
    enum command_id_t id;
};

static struct commands_t commands[] = {
    { "information", INFORMATION },
    { "load_executable", LOAD_EXECUTABLE },
    { "core", CORE },
    { "exit", EXIT },
    { "help", HELP },
    { "execute_program", EXECUTE_PROGRAM },
    { "unload_program", UNLOAD_PROGRAM },
    { "base_image", BASE_IMAGE },
    { "dump_trgmem", DUMP_TRGMEM },
    { "version", CORE_VERSION },
#if LOADER_DEBUG
    { "debug", DEBUG },
#endif
#if LOADER_DEBUG || LOADER_PROFILE
    { "profile", PROFILE },
#endif
    { "#", COMMENT },
};

/*****************************************************************************/
/* HELP() - Print a brief summary of the available commands.                 */
/*****************************************************************************/
static void help(void)
{
    int i;

    fprintf(stderr, "Commands:\n");

    for (i = 0; i < sizeof(commands) / sizeof(*commands); i++)
        printf("\t%s\n", commands[i].command);
}

/*****************************************************************************/
/* FIND_COMMAND_ID() - Look up a command name or partial command name in the */
/*                     commands table.  Return an enum representing command. */
/*****************************************************************************/
static enum command_id_t find_command_id(const char *tok)
{
    int i, found = -1;
    size_t tok_len = strlen(tok);

    for (i = 0; i < sizeof(commands) / sizeof(*commands); i++)
        if (!strncasecmp(tok, commands[i].command, tok_len))
        {
            if (found != -1)
            {
                fprintf(stderr,
                        "<< D O L T >> ERROR: Ambiguous command '%s'\n", tok);
                return ERROR_ID;
            }
            found = i;
        }

    if (found != -1) return commands[found].id;
    else
    {
        fprintf(stderr, "Unknown command '%s'\n", tok);
        return ERROR_ID;
    }
}

/*****************************************************************************/
/* INPUT_SERVER abstracts whether the commands come from the interactive     */
/* command line, a script file, or the --command option			     */
/*****************************************************************************/
typedef struct input_server_t
{
    enum { IST_FILE, IST_STR } type;
    FILE         *input_file;
    const char   *string;
    size_t        string_pos;
} INPUT_SERVER;

static INPUT_SERVER *cmd_set_file(FILE *input_file)
{
    INPUT_SERVER *input_server = malloc(sizeof(INPUT_SERVER));
    input_server->type = IST_FILE;
    input_server->input_file = input_file;
    return input_server;
}

static INPUT_SERVER *cmd_set_string(const char *str)
{
    INPUT_SERVER *input_server = malloc(sizeof(INPUT_SERVER));
    input_server->type = IST_STR;
    input_server->string = str;
    input_server->string_pos = 0;
    return input_server;
}

static int cmd_getc(INPUT_SERVER *server)
{
    if (server->type == IST_FILE) return fgetc(server->input_file);
    else
    {
	int ch;
	if (!server->string) return EOF;
	ch = server->string[server->string_pos++];
	if (!ch) return EOF;
	return ch;
    }
}

static int cmd_error(INPUT_SERVER *server)
{
    if (server->type == IST_FILE) return ferror(server->input_file);
    else return server->string == NULL;
}

static int cmd_eof(INPUT_SERVER *server)
{
    if (server->type == IST_FILE) return feof(server->input_file);
    else return !cmd_error(server) && !server->string[server->string_pos];
}

static int cmd_isstdin(INPUT_SERVER *server)
{
    if (server->type == IST_FILE) return server->input_file == stdin;
    else return 0;
}

/*****************************************************************************/
/* GET_LINE() - Read up to buf_len bytes worth of command-line input.	     */
/*****************************************************************************/
static char *get_line(char *cmd_buffer, size_t buf_len, INPUT_SERVER *server)
{
    size_t pos = 0;
    /* fgets always NUL-terminates, we'll do the same */

    while (pos < (buf_len - 1))
    {
	int ch;

	if ((ch = cmd_getc(server)) == EOF)
	{
	    if (cmd_error(server)) return NULL;
	    else if (pos) goto done; /* EOF, but read at least 1 char */
	    else return NULL;
	}
	else
	{
	    if (ch == ';' || ch == '\n')
	    {
		/* End of command.  Discard ';' or '\n' */
		goto done;
	    }
	    else if (ch == '#')
	    {
		/* comment start, discard everything until end-of-line */
		while (((ch = cmd_getc(server)) != EOF) && ch != '\n')
		    ;
		goto done;
	    }
	    else
	    {
		cmd_buffer[pos++] = ch;
	    }
	}
    }

    /* Buffer overrun */
    fprintf(stderr, "<< D O L T >> ERROR: command buffer overrun.\n");
    exit(1);

  done:

    cmd_buffer[pos] = '\0';
    return cmd_buffer;
}

/*****************************************************************************/
/* MAIN() - RIDL client driver.  Implements a user-interactive shell, but    */
/*      can also take input from a script file using "--script <file>"       */
/*      command-line option.                                                 */
/*****************************************************************************/
#if UNIT_TEST
int ridl_main(int argc, char** argv)
#else
int ridl_main(int argc, char** argv)
#endif
{
   /*------------------------------------------------------------------------*/
   /* Current length of a RIDL command is limited to a fixed-sized buffer.   */
   /* Exceeding this fixed buffer will result in a runtime error.            */
   /*------------------------------------------------------------------------*/
   char cmd_buffer[1000];

   /*------------------------------------------------------------------------*/
   /* Identify command-line input stream.  By default, this is stdin, but    */
   /* we can take commands from a script, using the --script option.         */
   /*------------------------------------------------------------------------*/
   INPUT_SERVER *cmd_server = cmd_set_file(stdin);

   /*------------------------------------------------------------------------*/
   /* Process command-line arguments to RIDL executable.                     */
   /*------------------------------------------------------------------------*/
   int a;
   for (a = 1; a < argc; a++)
   {
      /*---------------------------------------------------------------------*/
      /* RIDL can take input from a script file.  This is useful for running */
      /* RIDL under a test harness.  Specify the script file using --script  */
      /* option (usage: --script <file>).                                    */
      /*---------------------------------------------------------------------*/
      if (!strcmp(argv[a], "--script"))
      {
          if (cmd_server) free(cmd_server);
          cmd_server = cmd_set_file(fopen(argv[a+1], "r"));
      }

      /*---------------------------------------------------------------------*/
      /* RIDL can run commands specified on the command line.  This is	     */
      /* useful for running RIDL under a test harness.  Specify the commands */
      /* --command option (usage: --command "<cmd1>; <cmd2>").		     */
      /*---------------------------------------------------------------------*/
      if (!strcmp(argv[a], "--command"))
      {
          if (cmd_server) free(cmd_server);
          cmd_server = cmd_set_string(argv[a+1]);
      }

#if LOADER_DEBUG
      /*---------------------------------------------------------------------*/
      /* Internal debug.  Can specify debugging from the command-line with   */
      /* --debug option.  User interface also supports a "debug" command.    */
      /*---------------------------------------------------------------------*/
      if (!strcmp(argv[a], "--debug"))
         debugging_on = 1;
#endif

#if LOADER_DEBUG || LOADER_PROFILE
      /*---------------------------------------------------------------------*/
      /* Profiling.  It is important that this loader is small and fast.     */
      /* We may need to turn on profiing via --profile option to find where  */
      /* the loader is sluggish.                                             */
      /*---------------------------------------------------------------------*/
      if (!strcmp(argv[a], "--profile"))
         profiling_on = 1;
#endif
   }

   /*------------------------------------------------------------------------*/
   /* Initialize the dynamic loader.                                         */
   /*------------------------------------------------------------------------*/
   DLOAD_initialize();

   /*------------------------------------------------------------------------*/
   /* Initialize the client's model of the master DSBT.                      */
   /*------------------------------------------------------------------------*/
   AL_initialize(&DSBT_master, sizeof(DSBT_Entry), 1);

   /*------------------------------------------------------------------------*/
   /* Banner for user interaction mode.                                      */
   /*------------------------------------------------------------------------*/
   if (cmd_isstdin(cmd_server))
   {
      printf("Welcome to the Reference Implementation of the Dynamic Loader (RIDL).\n");
      printf("Using %s\n", DLOAD_version());
   }


   /*------------------------------------------------------------------------*/
   /* Command processing loop.                                               */
   /*------------------------------------------------------------------------*/
   cmd_buffer[0]='\0';
   while(1)
   {
      int32_t prog_argc,i;
      char* pathname;
      char* tok, *str_argc;
      Array_List prog_argv;

      /*---------------------------------------------------------------------*/
      /* In user interactive mode, prompt user for next command.             */
      /*---------------------------------------------------------------------*/
      if (cmd_isstdin(cmd_server)) printf("RIDL> ");

      /*---------------------------------------------------------------------*/
      /* Read up to semicolon, newline, or comment character.                */
      /*---------------------------------------------------------------------*/
      if (get_line(cmd_buffer, sizeof(cmd_buffer), cmd_server) == NULL)
      {
	  if (cmd_eof(cmd_server))
	  {
	      if (cmd_isstdin(cmd_server)) printf("exit\n");
	      exit(0);
	  }
	  else
	  {
	      fprintf(stderr,
		      "<< D O L T >> FATAL: "
		      "Unknown error reading commands\n");
	      exit(1);
	  }
      }

      /*---------------------------------------------------------------------*/
      /* Scan first token, skip over any empty command lines.                */
      /*---------------------------------------------------------------------*/
      tok = strtok(cmd_buffer, " \n");
      if (!tok) continue;

      /*---------------------------------------------------------------------*/
      /* We have some kind of command.  Transfer control to the right place  */
      /* in the loader.                                                      */
      /*---------------------------------------------------------------------*/
      switch (find_command_id(tok))
      {
         /*------------------------------------------------------------------*/
         /* "exit"                                                           */
         /*                                                                  */
         /* This is a safe exit out of RIDL user-interface.                  */
         /*------------------------------------------------------------------*/
         case EXIT:
           exit(0);
           break;

         /*------------------------------------------------------------------*/
         /* "help"                                                           */
         /*                                                                  */
         /* Display a list of commands                                       */
         /*------------------------------------------------------------------*/
         case HELP:
           help();
           break;

         /*------------------------------------------------------------------*/
         /* "information"                                                    */
         /*                                                                  */
         /* Some minimally useful information.  Where is dl6x's printf()     */
         /* and where is its exit()?                                         */
         /*------------------------------------------------------------------*/
         case INFORMATION:
            printf("Host printf: %x\nHost exit: %x\n",printf, exit);
            break;

         /*------------------------------------------------------------------*/
         /* "load <program> [<argc> <argv[0]> <argv[1]> ...]"                */
         /*                                                                  */
         /* Load an executable program into memory with the supplied command */
         /* line arguments.  This will load any dependent files as well.     */
         /*------------------------------------------------------------------*/
         case LOAD_EXECUTABLE:
            pathname = strtok(NULL," \n");
            str_argc = strtok(NULL," \n");
            if (str_argc)
               prog_argc = strtoul(str_argc, NULL, 0);
            else
               prog_argc = 0;

            /*---------------------------------------------------------------*/
            /* Initialize Target Memory Allocator Interface                  */
            /*---------------------------------------------------------------*/
            AL_initialize(&prog_argv, sizeof(char*), 1);

            /*---------------------------------------------------------------*/
            /* Allocate a private copy of each argv string specified in the  */
            /* load command.                                                 */
            /*---------------------------------------------------------------*/
            for (i = 0; i < prog_argc; i++)
            {
               char* temp = malloc(100);
               strcpy(temp,strtok(NULL, " \n"));
               AL_append(&prog_argv,&temp);
            }

            /*---------------------------------------------------------------*/
            /* Write out progress information for arguments read from load   */
            /* command.                                                      */
            /*---------------------------------------------------------------*/
            for (i = 0; i < prog_argc; i++)
               printf("Arg %d: %s\n",i,*((char**)(prog_argv.buf) + i));

            /*---------------------------------------------------------------*/
            /* Go invoke the core loader to load the specified program.      */
            /*---------------------------------------------------------------*/
            load_executable(pathname, prog_argc, (char**)(prog_argv.buf));

            /*---------------------------------------------------------------*/
            /* Did we get a valid program handle back from the loader?       */
            /*---------------------------------------------------------------*/
            if (!prog_handle && !cmd_isstdin(cmd_server))
            {
               fprintf(stderr,
                       "<< D O L T >> FATAL: load_executable failed in "
                       "script. Terminating.\n");
               exit(1);
            }

#if LOADER_DEBUG
            /*---------------------------------------------------------------*/
            /* If debug mode is turned on, then the loader will dump         */
            /* whatever is in target memory  to a "prog.dump" file.          */
	    /* This file can be processed by dis6x or ofd6x for further      */
	    /* debugging.                                                    */
            /*---------------------------------------------------------------*/
            if (debugging_on)
            {
               FILE* dumpfile = fopen("prog.dump","wb");
               if (!dumpfile)
                  fprintf(stderr,
                          "<< D O L T >> ERROR: prog.dump could not be "
                          "written.\n");
               else
               {
		  DLTMM_fwrite_trg_mem(dumpfile);
                  fclose(dumpfile);
               }
            }
#endif
            break;

         /*------------------------------------------------------------------*/
         /* "core <entry point> [<argc> <argv[0]> <argv[1]> ...]"            */
         /*                                                                  */
         /* Load the core image "prog.dump" into target memory, setting the  */
         /* specified address as the entry point.  Can also provide an       */
         /* argument list to be passed into the executable program.          */
         /*------------------------------------------------------------------*/
         case CORE:
         {
            /*---------------------------------------------------------------*/
            /* Find and open "prog.dump" file.  This should have been        */
            /* created by a previous "load" command under debug.             */
            /*---------------------------------------------------------------*/
            FILE* core = fopen("prog.dump","rb");
            if (!core)
            {
               fprintf(stderr,
                       "<< D O L T >> ERROR: prog.dump could not be read.\n");
               continue;
            }

            /*---------------------------------------------------------------*/
            /* Read the contents of "prog.dump" into target memory area.     */
            /*---------------------------------------------------------------*/
            DLTMM_fread_trg_mem(core);
            fclose(core);

            /*---------------------------------------------------------------*/
            /* Set entry point to specified entry point value (1st argument  */
            /* to "core" command).                                           */
            /*---------------------------------------------------------------*/
            loaded_program = (int(*)())(strtoul(strtok(NULL," \n"),NULL,0));

            /*---------------------------------------------------------------*/
            /* Process any argc, argv stuff that we want to use for this     */
            /* run.                                                          */
            /*---------------------------------------------------------------*/
            prog_argc = strtoul(strtok(NULL," \n"), NULL, 0);
            AL_initialize(&prog_argv,sizeof(char*), 1);

            for (i = 0; i < prog_argc; i++)
            {
               char* temp = malloc(100);
               strcpy(temp,strtok(NULL," \n"));
               AL_append(&prog_argv,&temp);
            }

            for (i = 0; i < prog_argc; i++)
               printf("Arg %d: %s\n",i,*((char**)(prog_argv.buf) + i));
          }
          break;

         /*------------------------------------------------------------------*/
         /* "execute"                                                        */
         /*                                                                  */
         /* Transfer control to the most recently loaded program.  Expect    */
         /* "loaded_program" to be pointing at entry point that we want to   */
         /* start with.                                                      */
         /*------------------------------------------------------------------*/
         case EXECUTE_PROGRAM:
            execute_program(prog_argc, (char**)(prog_argv.buf));
            break;

         /*------------------------------------------------------------------*/
         /* "unload"                                                         */
         /*                                                                  */
         /* Unload the last program that was loaded into memory.  Does this  */
         /* include any base image symbols?                                  */
         /*------------------------------------------------------------------*/
         /* We'll only worry about removing debug information if the module  */
         /* is actually unloaded from target space (DLOAD_unload() returns   */
         /* TRUE only if module is no longer needed and has indeed been      */
         /* unloaded from target memory).                                    */
         /*------------------------------------------------------------------*/
         case UNLOAD_PROGRAM:
            if (DLOAD_unload(prog_handle))
            {
               DSBT_release_entry(prog_handle);
               if (DLL_debug) DLDBG_rm_target_record(prog_handle);
            }

            loaded_program = NULL;

            break;

         /*------------------------------------------------------------------*/
         /* "base_image <program>"                                           */
         /*                                                                  */
         /* Load the global symbols from the dynamic symbol table of the     */
         /* specified program.  It is assumed that the specified program     */
         /* has already been loaded into target memory and is running.       */
         /*------------------------------------------------------------------*/
         case BASE_IMAGE:
         {
            char* base_image_name = strtok(NULL, " \n");
            FILE* image = fopen(base_image_name, "rb");

            /*---------------------------------------------------------------*/
	    /* Make sure that base image file was successfully opened.       */
            /*---------------------------------------------------------------*/
	    if (!image)
	    {
	       DLIF_error(DLET_FILE, "Failed to open base image file '%s'.\n",
	                  base_image_name);
	       break;
	    }

            /*---------------------------------------------------------------*/
	    /* Base image is assumed to be already loaded and running on the */
	    /* target.  The dynamic loader still has to read all of the      */
	    /* dynamic symbols in the base image so that we can link an      */
	    /* incoming DLL against the base image.                          */
            /*---------------------------------------------------------------*/
	    if (!(prog_handle = DLOAD_load_symbols(image)))
            {
               /*------------------------------------------------------------*/
               /* If we didn't get a proper file handle back from the        */
               /* DLOAD_load_symbols() API function, then we need to recover */
               /* gracefully.                                                */
               /*------------------------------------------------------------*/
               /* If a failure occurs from a script file, then we assume a   */
               /* catastrophic failure and terminate the session.            */
               /*------------------------------------------------------------*/
	       if (!prog_handle && !cmd_isstdin(cmd_server))
               {
                  fprintf(stderr,
                          "<< D O L T >> FATAL: base_image failed in script. "
                          "Terminating.\n");
                  exit(1);
               }
            }

            if (image) fclose(image);

	    /*---------------------------------------------------------------*/
	    /* Query base image for DLModules symbol to determine whether    */
	    /* debug support needs to be provided by the client side of the  */
	    /* loader.                                                       */
	    /*---------------------------------------------------------------*/
	    /* Note that space for the DLL View debug list is already        */
	    /* allocated as part of the base image, the header record is     */
	    /* initialized to zero and needs to be filled in when the first  */
	    /* module record is written to target memory.                    */
	    /*---------------------------------------------------------------*/
	    DLL_debug = DLOAD_query_symbol(prog_handle,
	                                          "_DLModules", &DLModules_loc);
	 }
	 break;

         /*------------------------------------------------------------------*/
         /* dump_trgmem <offset>, <nbytes>, <filename>                       */
         /*                                                                  */
         /* Dumps nbytes from the target memory starting at <offset>.        */
         /*------------------------------------------------------------------*/
         case DUMP_TRGMEM:
         {
            char* offset_str = strtok(NULL, ",");
            char* nbytes_str = strtok(NULL, ",");
            uint32_t offset  = strtoul(offset_str, NULL, 0);
            uint32_t nbytes  = strtoul(nbytes_str, NULL, 0);
            char* fn         = strtok(NULL, " \n");
            FILE* fp         = fopen(fn, "wb");

            DLTMM_dump_trg_mem(offset, nbytes, fp);

            fclose(fp);
            break;
         }

         /*------------------------------------------------------------------*/
	 /* "version"                                                        */
	 /*                                                                  */
	 /* Echo version ID string for dynamic loader core to STDOUT.        */
         /*------------------------------------------------------------------*/
	 case CORE_VERSION:
            printf("\n%s\n", DLOAD_version());
	    break;

         /*------------------------------------------------------------------*/
         /* "debug"                                                          */
         /*                                                                  */
         /* Turn on debugging.  Could also increase level of debug if        */
         /* we wanted to go for different levels of debug detail.            */
         /*------------------------------------------------------------------*/
#if LOADER_DEBUG
         case DEBUG:
            debugging_on = 1;
            break;
#endif

         /*------------------------------------------------------------------*/
         /* "profile"                                                        */
         /*                                                                  */
         /* Turn on profiling.  Used to find inefficiencies in loader speed  */
         /* while debugging internally.                                      */
         /*------------------------------------------------------------------*/
#if LOADER_DEBUG || LOADER_PROFILE
         case PROFILE:
            profiling_on = 1;
            break;
#endif
         /*------------------------------------------------------------------*/
         /* Unrecognized commands will be reported and ignored.  We just     */
         /* move onto the next prompt or line in the script.                 */
         /* Note that in a script, if a line begins with '#', it is treated  */
         /* as a comment.  Comment delimiter tokens must be first on a line. */
         /*------------------------------------------------------------------*/
         case COMMENT:
         default:
            if (tolower(tok[0]) != '#')
               fprintf(stderr,
                       "<< D O L T >> ERROR: Unrecognized command ignored.\n");
            continue;
      }
   }

   /*NOTREACHED*/
   return 0;
}
