/*
* lnk.cmd
*
* C6x dynamic loader linker command file.
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
-c
-heap  0x10000000
-stack 0x200000
--args 0x200

MEMORY
{
    PMEM:   o = 00000020h   l = 01000000h
    BLOB:   o = 02000000h   l = 02000000h
    BMEM:   o = 80000000h   l = 20000000h
}

SECTIONS
{
    .text       >       PMEM
    .stack      >       BMEM
    .args       >       BMEM

    GROUP
    {
            .dsbt
            .got
            .bss
            .neardata
            .rodata
    }>BMEM

    .cinit      >       BMEM
    .cio        >       BMEM
    .const      >       BMEM
    .data       >       BMEM
    .blob       > 	BLOB
    .switch     >       BMEM
    .sysmem     >       BMEM
    .far        >       BMEM
    .ppinfo     >       BMEM
    .ppdata     >       BMEM, palign(32) /* Work-around kelvin bug */
}
