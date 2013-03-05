Read Me -- RIDL
---------------

Unless otherwise noted, a command can be replaced with any word
starting with the same character.  Commands are case-insensitive.  The
size of the buffer for the RIDL prompt is one hundred characters, and
input is read with gets().  So, if you type more than 100 characters
at any one RIDL prompt, it is likely bad things will happen.

information - gives the memory addresses of printf and exit.

load <executable_name> <number_of_arguments> <argv[0]> <argv[1]> ...

Loads the executable <executable_name> into memory with the supplied
command-line arguments.  This will result in any DT_NEEDED entries for
that executable being loaded into memory as well.

core <entry_point> <number_of_arguments> <argv[0]> <argv[1]>

Loads the core image prog.dump into memory with the specified memory
address as the entry point and the supplied arguments as command-line
parameters.

exit

Exit RIDL and return to the execution environment.

help

Display a list of commands.

execute

Transfers control to the loaded program.  If you're lucky, eventually
the loaded program will transfer control back to RIDL.

unload

Unloads the last program that was loaded into memory.  Doesn't
actually free any memory -- this command is just here for testing.

base_image <executable_name>

Load the specified executable's symbols into the symbol table for the
base image.

symbol <symname> <symval>

Manually insert the specified symbol into the symbol table for the
base image.  This command is currently disabled.

debug

Turns on debugging support.  You can also do this with the "--debug"
command-line parameter to RIDL.  The executable must have been built
with debugging support for this command to work.  When debugging is
enabled, the core loader will chat endlessly about its various
activities while it loads a file.  It will also produce a file named
"prog.dump" at the end of loading, which is a core dump of the loaded
executable.  You can load this file back into RIDL later with the
"core" command.  Besides this file, the loader will also write files
suffixed ".elfdbg" for each ELF executable loaded.  These .elfdbg
files will have all relocations performed and can be viewed with
"dis6x", so they are very useful in hunting down relocation bugs.
Turning debugging on will also enable profiling.

profile

Turns on profiling support.  This will provide cycle counts for each
stage of the loading process.

Command-line parameter: --script file

If this argument is given to RIDL at the command line, it will read
its input from the file "file", rather than from standard input.
Comments may be added to the script by starting the line with the '#'
character.
