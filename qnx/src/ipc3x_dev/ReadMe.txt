                             SysLink 3 / Tiler

This is the Qnx SysLink IPC that uses the VirtQueue transport to communicate
with the sys-bios rpmsg.

Source Code:
============
    - Qnx SysLink Source
      The source code is located in the gforge01 git tree.

      git clone ssh://<aid/xid>@gforge01.dal.design.ti.com/gitroot/syslink_qnx
      branch: sl3_dev
      Provide enterprise password when prompted.

    - BIOS RPMSG Source
      The source code is located in git.omapzoom.org

      branch: master
      tag: 2.00.11.31
      commit-id: 91f6379282230ef5d1818ce8363ce1e64f23cda2
      patches needed on top for Qnx can be found at:
      \\dta0866189\SHARE\sl3\patches\sl3_1.05.23\sysbios-rpmsg
      0001-IPC-Protect-Message-Sending-to-Serialize-Messages.patch
      0002-WIP-Add-j5-and-j5eco-support.patch
      0003-Resources-Update-ti811x-resource-table-for-shmem-car.patch
      0004-Update-number-of-HwSpinlocks.patch

      Note: For running the benchmarking test, follow the instructions at the
            top of the MPU-side benchmarking test app.

      Default firmware files are generated in
      /sysbios-rpmsg/src/ti/examples/srvmgr/ti_platform_<...>.  The folder is
      named for the platform.  For example, for the OMAP5 ipu firmware, the folder
      is named ti_platform_omap54xx_ipu.

Tools:
======
    - Qnx Momentics IDE 6.5.x
      For more information please refer to the Wiki:
      http://opbuwiki.dal.design.ti.com/index.php/QNX_Main_Page#Developer_Setup
    - OMAP5 BSP Information is available in thie Wiki:
      http://opbuwiki.dal.design.ti.com/index.php/OMAP5_QNX_BSP
    - J5Eco BSP Information can be found on this Wiki:
      http://opbuwiki.dal.design.ti.com/index.php/J5eco_QNX_RPMSG#QNX_J5_eco_BSP
    - Please refer to the ReadMe in the Bios RPMSG source for the tools
      required to build the BIOS RPMSG source.

Build Instructions (Windows):
=============================
    1) Open a command prompt and change directory to the syslink project
       directory.
    2) (Optional) To override the default install root, create a new file named
       qconf-override.mk with the following contents:
           INSTALL_ROOT_nto := C:/ide-4.7-workspace/trunk
           USE_INSTALL_ROOT = 1
       Set the environment variable QCONF_OVERRIDE as follows:
           set QCONF_OVERRIDE = C:\ide-4.7-workspace\trunk\qconf-override.mk
    3) From the command prompt:
           To build for OMAP5:
               > make SYSLINK_PLATFORM=omap5430 TILER_PLATFORM=omap5430 SMP=1
           To build for TI811X (note, there is no tiler support for j5 eco):
               > make SYSLINK_PLATFORM=ti81xx SYSLINK_VARIANT=TI811X
       Note: To export the tiler headers and built binaries/libraries to the
             install root, you can instead run "make install"

Note: To build the binaries/libraries with the debug compile options of -g and
      -O0, add the following to the build command:

          "SYSLINK_DEBUG=1 TILER_DEBUG=1".

Clean Instructions (Windows):
=============================
    1) Open a command prompt and change directory to the syslink project
       directory.
    3) From the command prompt:
           > make clean

Build Output:
=============

    Tiler binary and libs:
    ----------------------
    \tiler\resmgr\tiler\arm\le.v7\tiler	             Tiler Resource Manager
    \tiler\usr\memmgr\arm\so.le.v7\libmemmgr.so        Tiler MemMgr API library
    \tiler\usr\tilerusr\arm\so.le.v7\libtilerusr.so    Tiler TilerUsr API library
    \tiler\usr\utils\arm\so.le.v7\libmemmgr_utils.so   Tiler MmemMgr Utils library

    Tiler samples:
    --------------
    \tiler\usr\tests\memmgr\arm\le.v7\memmgr_test     Tiler MemMgr test
    \tiler\usr\tests\tiler\arm\le.v7\tiler_test	      Tiler parametric test
    \tiler\usr\tests\tilerusr\arm\le.v7\tilerusr_test Tiler TilerUsr test
    \tiler\usr\tests\utils\arm\le.v7\utils_test	      Tiler Utils test

    SharedMemAllocator binary and libs:
    -----------------------------------
    \sharedmemallocator\resmgr\arm\le.v7\shmemallocator	             ResMgr
    \sharedmemallocator\usr\arm\so.le.v7\libsharedmemallocator.so    User Lib

    SharedMemAllocator samples:
    ---------------------------
    \sharedmemallocator\samples\sharedMemAllocator\usr\arm\le.v7\SharedMemoryAllocatorTestApp

    SysLink binary and lib (under \ti\syslink\build\Qnx\):
    ------------------------------------------------------
    resmgr\arm\le.v7\syslink                    Syslink Resource Manager
    lib\arm\so.le.v7\libsyslink_client.so       Syslink client library (
                                                needed for using HWSpinLock API
                                                and running the
                                                MQCopy test app)

    SysLink samples (under \ti\syslink\samples\hlos\):
    --------------------------------------------------
    benchmark\usr\arm\le.v7\rpmsg-omx-benchmark     Benchmark test
    MessageQCopy\usr\arm\le.v7\mqcopytestapp        MessageQCopy test
    rpmsg-omx\usr\arm\le.v7\rpmsg-omx-test          Rpmsg-omx interface test
    hwspinlock\usr\arm\le.v7\HwSpinLockTestApp      HWSpinLock test

    Note: If an install root was specified, you will find the above mentioned binaries and
    libraries in the follwing paths in the install root:

    <install_root>\bin\
        -> syslink
        -> rpmsg-omx-benchmark
        -> mqcopytestapp
        -> rpmsg-omx-test
        -> HwSpinLockTestApp
    <install_root>\sbin\
        -> tiler
        -> memmgr_test
        -> tiler_test
        -> tilerusr_test
        -> utils_test
    <install_root>\usr\lib
        -> libmemmgr.so
        -> libtilerusr.so
        -> libmemmgr_utils.so
        -> libsyslink_client.so

To Run SysLink/Tiler/SharedMemAllocator:
========================================

    SharedMemAllocator
    ------------------
    On the target, sharedmemallocator can be started by running the following
    command
        # shmemallocator
    No additional command line arguments are required.
    The default carveouts will be created unless the resource manager
    source file is modified to define different carveouts.
    Note: If the source file is modified, then the carveouts will need to be
    modified in the syslink Platform.c file, and the sysbios-rpmsg resource
    table file in order for the carveouts to be visible to the remote core.

    Tiler
    -----
    On the target, tiler can be started by running the following command
        # tiler
    No additional command line arguments required.  To see what options are
    available, run "use tiler" or view the tiler.use file.

    SysLink
    -------
    On the target, syslink can be started by running the following commad
        # syslink <proc_name> <ipu_firmware_file> ...
    The IPU firmware file to be loaded must be specified when starting syslink.
    The proc_name is the same as the MultiProc name for the proc that is to
    be loaded and is specific to the platform. Multiple proc_name/file pairs
    can be listed and all will be loaded.
    To see what other options may be available, run "use syslink" or view the
    syslink.use file.
    For example:
        - To start syslink with DSP for TI811X:
            # syslink DSP <dsp_firmware_file>
    For backward-compatibility with previous OMAP5 releases, the "-f" and "-d"
    options can be used instead to load IPU and DSP, respectively:
        # syslink -f <ipu_firmware_file> -d <dsp_firmware_file>

Disabling Hibernation and Changing the Hibernation Timeout:
=======================================
    To disable hibernation, use the "-H 0" option when starting syslink:

    # syslink -f <ducati_firmware_file> -H 0

    To change the hibernation timeout, use the "-T" option when starting syslink.
    The value should  be in msec:

    # syslink -f <ducati_firmware_file> -T 10000

Remote Core Traces:
===================
    To see a dump of the remote core trace buffer, simply cat
    /dev/syslink-trace<proc_id>.  Proc_id is the same as the MultiProc procId.

    For IPU traces for OMAP5:
    # cat /dev/syslink-trace2

    For DSP traces for OMAP5 or TI811X:
    # cat /dev/syslink-trace0

Changing SharedMemAllocator Carveouts:
======================================
    The carveouts used by the SharedMemAllocator are defined in
    \sharedmemallocator\resmgr\SharedMemoryAllocator.c at in the "blocks" array.
    In order to modify the addresses/sizes/number of the carveouts, modify this
    array and then re-build the module.

Defining Carveouts for the Remote Cores:
========================================
    In order to be able to perform the address translation and to have the
    carveout mapped into the MMU for the remote core, the carveouts must be
    defined in the sysbio-rpmsg resource table for the cores which need access
    to them.  Carveouts should be added as "devmem" entries in the table, with
    the virtual and physical addresses given in the entry.

    The MPU-side Platform.c file for the target should also be updated to define
    the updated/new carveouts in the Syslink_Override_Params.  The carveouts
    should be listed here for each core which needs access to them.

Validating on the TARGET:
=============================

    SysLink standalone tests:
    -------------------------------------
    1) Setup the target:
       Copy the following files to the target:
           syslink
           mqcopytestapp
           rpmsg-omx-test
           libsyslink_client.so
           test_omx_ipu.xem3 (for OMAP5 IPU)
           test_omx_dsp.xe64T (for OMAP5 or TI811X DSP)

       Note: The binaries are from the sysbios-rpmsg tree, and
             can be found in the examples folder.

    3) Start syslink from the terminal:
       For OMAP5:
       # /tmp/syslink -f /tmp/test_omx_ipu.xem3 -d /tmp/test_omx_dsp.xe64T -H 0
       For TI811X:
       # /tmp/syslink DSP /tmp/test_omx_dsp.xe64T -H 0

       You will see output like below on the console (OMAP5 output shown as example):
       # /tmp/syslink -f /tmp/test_omx_ipu.xem3 -d /tmp/test_omx_dsp.xe64T -H 0
       Starting syslink resource manager...
       RscTable_process: RscTable version is [1]
       RscTable_process: vring [256] @ [0xa0000000]
       RscTable_process: vring [256] @ [0xa0004000]
       RscTable_process: carveout [IPU_MEM_TEXT] @ da [0x00000000] pa [0xeda00000] len [0x600000]
       RscTable_process: carveout [IPU_MEM_DATA] @ da [0x80000000] pa [0xee000000] len [0x9c00000]
       RscTable_process: carveout [IPU_MEM_IPC_DATA] @ da [0x9f000000] pa [0xf7c00000] len [0x100000]
       RscTable_process: trace [trace:sysm3] @ da [0x9f000000] len [0x8000]
       RscTable_process: devmem [IPU_MEM_IPC_VRING] @ da [0xa0000000] pa [0xed900000] len [0x100000]
       RscTable_process: devmem [IPU_MEM_IOBUFS] @ da [0x90000000] pa [0xf7d00000] len [0x5a00000]
       RscTable_process: devmem [IPU_TILER_MODE_0_1] @ da [0x60000000] pa [0x60000000] len [0x10000000]
       RscTable_process: devmem [IPU_TILER_MODE_2] @ da [0x70000000] pa [0x70000000] len [0x8000000]
       RscTable_process: devmem [IPU_TILER_MODE_3] @ da [0x78000000] pa [0x78000000] len [0x8000000]
       RscTable_process: devmem [IPU_PERIPHERAL_L4CFG] @ da [0xaa000000] pa [0x4a000000] len [0x1000000]
       RscTable_process: devmem [IPU_PERIPHERAL_L4PER] @ da [0xa8000000] pa [0x48000000] len [0x1000000]
       RscTable_process: devmem [IPU_IVAHD_CONFIG] @ da [0xba000000] pa [0x5a000000] len [0x1000000]
       RscTable_process: devmem [IPU_IVAHD_SL2] @ da [0xbb000000] pa [0x5b000000] len [0x1000000]
       RscTable_process: devmem [IPU_PERIPHERAL_DMM] @ da [0xae000000] pa [0x4e000000] len [0x100000]
       << DLOAD >> WARNING: '' does not have a dynamic segment; assuming that it is a static executable and it cannot be relocated.
         Programming Benelli memory regions
       =========================================
       VA = [0xa0000000] of size [0x100000] at PA = [0xed900000]
       VA = [0x0] of size [0x600000] at PA = [0xeda00000]
       VA = [0x80000000] of size [0x9c00000] at PA = [0xee000000]
       VA = [0x9f000000] of size [0x100000] at PA = [0xf7c00000]
         Programming Benelli L4 peripherals
       =========================================
       PA [0x60000000] VA [0x60000000] size [0x10000000]
       PA [0x70000000] VA [0x70000000] size [0x8000000]
       PA [0x78000000] VA [0x78000000] size [0x8000000]
       PA [0x4a000000] VA [0xaa000000] size [0x1000000]
       PA [0x48000000] VA [0xa8000000] size [0x1000000]
       PA [0x4e000000] VA [0xae000000] size [0x100000]
       PA [0x5a000000] VA [0xba000000] size [0x1000000]
       PA [0x5b000000] VA [0xbb000000] size [0x1000000]
       PA [0x58000000] VA [0xb8000000] size [0x1000000]
       RscTable_process: RscTable version is [1]
       RscTable_process: vring [256] @ [0xa0000000]
       RscTable_process: vring [256] @ [0xa0004000]
       RscTable_process: carveout [DSP_MEM_TEXT] @ da [0x20000000] pa [0xe0d00000] len [0x100000]
       RscTable_process: carveout [DSP_MEM_DATA] @ da [0x90000000] pa [0xe0e00000] len [0x100000]
       RscTable_process: carveout [DSP_MEM_HEAP] @ da [0x90100000] pa [0xe0f00000] len [0x300000]
       RscTable_process: carveout [DSP_MEM_IPC_DATA] @ da [0x9f000000] pa [0xe1200000] len [0x100000]
       RscTable_process: trace [trace:dsp] @ da [0x9f000000] len [0x8000]
       RscTable_process: devmem [DSP_MEM_IPC_VRING] @ da [0xa0000000] pa [0xe0c00000] len [0x100000]
       RscTable_process: devmem [DSP_MEM_IOBUFS] @ da [0x80000000] pa [0xe1300000] len [0x5a00000]
       RscTable_process: devmem [DSP_TILER_MODE_0_1] @ da [0x60000000] pa [0x60000000] len [0x10000000]
       RscTable_process: devmem [DSP_TILER_MODE_2] @ da [0x70000000] pa [0x70000000] len [0x8000000]
       RscTable_process: devmem [DSP_TILER_MODE_3] @ da [0x78000000] pa [0x78000000] len [0x8000000]
       RscTable_process: devmem [DSP_PERIPHERAL_L4CFG] @ da [0x4a000000] pa [0x4a000000] len [0x1000000]
       RscTable_process: devmem [DSP_PERIPHERAL_L4PER] @ da [0x48000000] pa [0x48000000] len [0x1000000]
       RscTable_process: devmem [DSP_PERIPHERAL_DMM] @ da [0x4e000000] pa [0x4e000000] len [0x100000]
       << DLOAD >> WARNING: '' does not have a dynamic segment; assuming that it is a static executable and it cannot be relocated.
         Programming Tesla memory regions
       =========================================
       VA = [0xa0000000] of size [0x100000] at PA = [0xe0c00000]
       VA = [0x20000000] of size [0x100000] at PA = [0xe0d00000]
       VA = [0x90000000] of size [0x100000] at PA = [0xe0e00000]
       VA = [0x90100000] of size [0x300000] at PA = [0xe0f00000]
       VA = [0x9f000000] of size [0x100000] at PA = [0xe1200000]
         Programming L4 peripherals
       =========================================
       VA = [0x60000000] and PA [0x60000000] of size = [0x10000000]
       VA = [0x70000000] and PA [0x70000000] of size = [0x8000000]
       VA = [0x78000000] and PA [0x78000000] of size = [0x8000000]
       VA = [0x4a000000] and PA [0x4a000000] of size = [0x1000000]
       VA = [0x48000000] and PA [0x48000000] of size = [0x1000000]
       VA = [0x49000000] and PA [0x49000000] of size = [0x1000000]
       VA = [0xba000000] and PA [0x5a000000] of size = [0x1000000]
       Syslink resource manager started
       #

    4) rpmsg-omx-test:
       # rpmsg-omx-test 1 10

       You will see output like below on the console:
       # rpmsg-omx-test 10
       omx_sample: Connected to OMX
       omx_sample (1): OMX_GetHandle (H264_decoder).
               msg_id: 99, fxn_idx: 5, data_size: 13, data: OMX_Callback
       omx_sample (1): Got omx_handle: 0x5c0ffee5
       omx_sample(1): OMX_SetParameter (0x5c0ffee5)
       omx_sample (1): Got result 0
       omx_sample (2): OMX_GetHandle (H264_decoder).
               msg_id: 99, fxn_idx: 5, data_size: 13, data: OMX_Callback
       omx_sample (2): Got omx_handle: 0x5c0ffee5
       omx_sample(2): OMX_SetParameter (0x5c0ffee5)
       omx_sample (2): Got result 0

       [...]

       omx_sample (10): OMX_GetHandle (H264_decoder).
               msg_id: 99, fxn_idx: 5, data_size: 13, data: OMX_Callback
       omx_sample (10): Got omx_handle: 0x5c0ffee5
       omx_sample(10): OMX_SetParameter (0x5c0ffee5)
       omx_sample (10): Got result 0
       omx_sample: Closed connection to OMX!
       #

    5) mqcopytestapp:
       # mqcopytestapp 1

       To see the test app output, run sloginfo:
       Jan 01 00:00:00    4    42     0 MessageQCopyApp sample application

       Jan 01 00:00:00    4    42     0 Entered MessageQCopyApp_startup

       Jan 01 00:00:00    4    42     0 Leaving MessageQCopyApp_startup. Status [0x0]

       Jan 01 00:00:00    4    42     0 Entered MessageQCopyApp_execute

       Jan 01 00:00:00   4    42     0 <1>mqcopy_server_test_cb 0 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 1 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 2 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 3 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 4 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 5 for handle 6141352 from 50 [hello world!]

       [...]

       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 495 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 496 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 497 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 498 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_server_test_cb 499 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>_MessageQCopy_callback_bufReady: no object for endpoint: 103

       Jan 01 00:00:00    4    42     0 Leaving MessageQCopyApp_execute. Status [0x0]

       Jan 01 00:00:00    4    42     0 Entered MessageQCopyApp_shutdown

       Jan 01 00:00:00    4    42     0 Leaving MessageQCopyApp_shutdown. Status [0x0]

       # mqcopytestapp 2

       To see the test app output, run sloginfo:
       Jan 01 00:00:00    4    42     0 MessageQCopyApp sample application

       Jan 01 00:00:00    4    42     0 Entered MessageQCopyApp_startup

       Jan 01 00:00:00    4    42     0 Leaving MessageQCopyApp_startup. Status [0x0]

       Jan 01 00:00:00    4    42     0 Entered MessageQCopyApp_execute

       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 0 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 1 for handle 6141352 from 51 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 2 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 3 for handle 6141352 from 51 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 4 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 5 for handle 6141352 from 51 [hello world!]

       [...]

       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 495 for handle 6141352 from 51 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 496 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 497 for handle 6141352 from 51 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 498 for handle 6141352 from 50 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>mqcopy_client_test_cb 499 for handle 6141352 from 51 [hello world!]
       Jan 01 00:00:00    4    42     0 <1>_MessageQCopy_callback_bufReady: no object for endpoint: 103

       Jan 01 00:00:00    4    42     0 <1>_MessageQCopy_callback_bufReady: no object for endpoint: 103

       Jan 01 00:00:00    4    42     0 Leaving MessageQCopyApp_execute. Status [0x0]

       Jan 01 00:00:00    4    42     0 <1>_MessageQCopy_callback_bufReady: no object for endpoint: 103

       Jan 01 00:00:00    4    42     0 Entered MessageQCopyApp_shutdown

       Jan 01 00:00:00    4    42     0 Leaving MessageQCopyApp_shutdown. Status [0x0]

    6) HwSpinLockTestApp:
       Note: Use the test_omx_dsp to run this test.

       # HwSpinLockTestApp

       You will see output like below on the console:

       # HwSpinLockTestApp
       PROC=> HwSpinlock_create(0) CREATED

       PROC=> lock (0) entered

       PROC=> return from HwSpinlock_leave(0)
       HwSpinlock_delete(token0): PASSED

       PROC=> HwSpinlock_create(1) CREATED

       PROC=> lock (1) entered

       PROC=> return from HwSpinlock_leave(1)
       HwSpinlock_delete(token1): PASSED

       PROC=> HwSpinlock_create(1) CREATED

       PROC=> lock (2) entered

       PROC=> return from HwSpinlock_leave(2)
       HwSpinlock_delete(token2): PASSED
       #

    Tiler Standalone Tests:
    -----------------------

    1) Setup the target

       Copy the following to the target:

       tiler
       memmgr_test
       tilerusr_test
       utils_test
       libmemmgr.so
       libtilerusr.so
       libmemmgr_utils.so

    2) Start tiler from the terminal:
       # tiler

       You will see output like below on the console:
       # tiler
       configured grow size is 16
       configured grow size is 16
       tiler: Unable to register to low memory event: Function not implemented

    3) memmgr_test (Note: this test takes a long time on Virtio)
       # memmgr_test
       TEST #  1 - alloc_1D_test(4096, 0)
       TEST_DESC - Allocate & Free 4096b 1D buffer
       ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 1, UNAVAILABLE: 0
       TEST #  2 - alloc_2D_test(64, 64, PIXEL_FMT_8BIT)
       TEST_DESC - Allocate & Free 64x64x1b 1D buffer
       ==> TEST OK

       [...]

       TEST # 103 - star_test(1000, 10)
       TEST_DESC - Random set of 1000 Allocs/Maps and Frees/UnMaps for 10 slots
       ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 103, UNAVAILABLE: 0
       so far FAILED: 0, SUCCEEDED: 103, UNAVAILABLE: 0
       FAILED: 0, SUCCEEDED: 103, UNAVAILABLE: 0

    4) tilerusr_test (Note: this test takes a long time on Virtio)
       # tilerusr_test
       TEST #  1 - alloc_1D_test(4096, 0)
       TEST_DESC - Allocate & Free 4096b 1D buffer
       ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 1, UNAVAILABLE: 0
       TEST #  2 - alloc_2D_test(64, 64, PIXEL_FMT_8BIT)
       TEST_DESC - Allocate & Free 64x64x1b 1D buffer
       ==> TEST OK

       [...]

       TEST # 79 - star_test(1000, 10)
       TEST_DESC - Random set of 1000 Allocs/Maps and Frees/UnMaps for 10 slots
       ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 79, UNAVAILABLE: 0
       so far FAILED: 0, SUCCEEDED: 79, UNAVAILABLE: 0
       FAILED: 0, SUCCEEDED: 79, UNAVAILABLE: 0

    5) utils_test
       # utils_test
       TEST #  1 - test_new()
       TEST_DESC - ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 1, UNAVAILABLE: 0
       TEST #  2 - test_list()
       TEST_DESC - ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 2, UNAVAILABLE: 0

       [...]

       TEST #  7 - test_math()
       TEST_DESC - ==> TEST OK
       so far FAILED: 0, SUCCEEDED: 7, UNAVAILABLE: 0
       so far FAILED: 0, SUCCEEDED: 7, UNAVAILABLE: 0
       FAILED: 0, SUCCEEDED: 7, UNAVAILABLE: 0
