/**
 *  @file   PerfUtils.c
 *
 *  @brief     This modules provides functions for measuring performance
 *
 *  ============================================================================
 *
 *  Copyright (c) 2009-2012, Texas Instruments Incorporated
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

/* Linux specific header files */
#include <sys/time.h>
/* Standard headers */
#include <stdlib.h>
#include <string.h>

/* OSAL & Utils headers */
#include <PerfUtils.h>

/* Linux specific header files */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
/* OSAL & Utils headers */
#include <stdint.h>


#define SYSM3_PROC 2
#define APPM3_PROC 3
/* PERF UTILS ERROR CODES  START        */
/*Reserved index CML4 clock*/
typedef enum perf_util_index{
 PERF_UTIL_KERNEL_INDEX = 0,
 PERF_UTIL_GPTIMER_INDEX,
 PERF_UTIL_SYSCLK_INDEX,
 PERF_UTILS_CML4_INDEX,
 PERF_UTILS_MAX_IDENX ,
 PERF_UTILS_RECORDS_INDEX = -2
}perf_util_index;

//#define PERF_UTILS_DEBUG_SUPPORT 1
#define PERF_UTILS_SUCCESS             (0)
#define PERF_UTILS_FAILURE             (-1)
#define PERF_UTILS_INIT_FAILED         (-2)
#define PERF_UTILS_GPTIMER_TEN         (10)
//#define PERF_UTILS_DEBUG_SUPPORT 1
#define SYSLINK_MEM_IPC_HEAP2_ADDR 0x97FAA000
/* 6K memory mapping for perfutils*/
#define SYSLINK_MEM_IPC_HEAP2_SIZE ((MAX_NUM_KERNELSPACE_RECORDS) * (sizeof(Perf_FunctionRecord)))
//#define SYSLINK_MEM_IPC_HEAP2_SIZE 0x2900
/* PERF UTILS ERROR CODES  END          */

/* GENERIC MACROS TO ACCESS GPTIMER REGISTERS */
#define REG(MODULE,MODULENO,NAME,BASEADDR) \
        REG_OFFSET(MODULE##MODULENO, _##NAME,BASEADDR)
//#define REG_OFFSET(CMODULE_NO,NAME) REG_ADDR(CMODULE_NO##NAME##REG_OFFSET,CMODULE_NO##_MODULE_BASE##_ADDR)
#define REG_OFFSET(CMODULE_NO, NAME,BASEADDR) \
        REG_ADDR(CMODULE_NO##NAME##REG_OFFSET,BASEADDR)

#define REG_ADDR(OFFSET, BASE)   ((OFFSET) + (BASE))

/* GP TIMER REGISTER ADDRESSES AND OFFSET           */
#define GPTIMER_MODULE_REGSET_SIZE          0x00000058
#define GPTIMER_CTRLREG_AR_BIT_POS          0x00000001
#define GPTIMER_CTRLREG_PTV_BIT_POS         0x00000002
#define GPTIMER_CTRLREG_PRE_BIT_POS         0x00000005
#define GPTIMER_CTRLREG_ST_BIT_POS          0x00000000

/* GP TIMER 10                                      */
#define GPTIMER10_MODULE_BASE_ADDR          0x48086000
#if defined (SYSLINK_PLATFORM_OMAP4430)
#define GPTIMER10_CTRLREG_OFFSET            0x00000009
#define GPTIMER10_CRRREG_OFFSET             0x0000000a
#define GPTIMER10_LDRREG_OFFSET             0x0000000b
#elif defined(SYSLINK_PLATFORM_OMAP5430)
#define GPTIMER10_CTRLREG_OFFSET            0x0000000e
#define GPTIMER10_CRRREG_OFFSET             0x0000000f
#define GPTIMER10_LDRREG_OFFSET             0x00000010
#endif

/*GP TIMER 9                                        */
#define GPTIMER9_MODULE_BASE_ADDR           0x4803E000
#define GPIMER9_CLRREG_OFFSET               0x00000038
#define GPTIMER9_CRRREG_OFFSET              0x0000003C
#define GPTIMER9_LDRREG_OFFSET              0x00000040

/* L4PER_CM2 registers  */
#define CM_L4PER_GPTIMER_BASE_ADDR           0x4A009400
#if defined(SYSLINK_PLATFORM_OMAP4430)
#define CM_SYS_CLKSEL_REG                    0x4A306110
#elif defined(SYSLINK_PLATFORM_OMAP5430)
#define CM_SYS_CLKSEL_REG                    0x4AE06110
#endif
#define CM_L4PER_GPTIMER_REGSET_SIZE         0x00000030
#define CM_L4PER_GPTIMER10_CLKCTRLREG_OFFSET 0x0000000A
#define CM_L4PER_GPTIMER10_CLKCTRLREG        0x4A009428
#define CM_L4PER_GPTIMER10_CLKCTRLREG_SIZE   0x00000004

#define SYS_CLK_UNINITIALIZED                0x0000000
#define SYS_CLK_12MHZ_FREQ                   0x0B71B00
#define SYS_CLK_13MHZ_FREQ                   0x0C65D40
#define SYS_CLK_16_8MHZ_FREQ                 0x1005900
#define SYS_CLK_19_2MHZ_FREQ                 0x124F800
#define SYS_CLK_26MHZ_FREQ                   0x18CBA80
#define SYS_CLK_27MHZ_FREQ                   0x19BFCC0
#define SYS_CLK_38_4MHZ_FREQ                 0x249F000
#define CONV_FACTOR_SEC_TO_USEC              1000000

// Must match struct definition on Ducati side
#define MAX_NUM_USERSPACE_RECORDS   10
#define MAX_NUM_KERNELSPACE_RECORDS 5

typedef struct {
    unsigned int numEntries;
    unsigned int enterTimes[MAX_ENTRIES_PER_FUNCTION];
    unsigned int exitTimes[MAX_ENTRIES_PER_FUNCTION];
    char fxnName[128];
    unsigned int client_proc;
    unsigned int server_proc;
    unsigned int msg_size;
} Perf_FunctionRecord;

typedef struct
{
    unsigned int numEntries;
    struct timeval enterTimes[MAX_ENTRIES_PER_FUNCTION];
    struct timeval exitTimes[MAX_ENTRIES_PER_FUNCTION];
    char fxnName[128];
    unsigned int client_proc;
    unsigned int server_proc;
    unsigned int msg_size;
} Perf_LocalRecord;

#ifdef USE_GPTIMER
static Perf_FunctionRecord perfRecordsArr[MAX_NUM_USERSPACE_RECORDS];
static Perf_FunctionRecord *perfRecordsUserSpace;
#else
static Perf_LocalRecord    perfRecordsArr[MAX_NUM_USERSPACE_RECORDS];
static Perf_LocalRecord *perfRecordsUserSpace;
#endif

typedef struct
{
    unsigned int enablePrescale:1;
    unsigned int enableautoReload:1;
    unsigned int valuePrescalar:3;
    unsigned int gpTimer:4;
}Perf_GptimerConfig;

typedef struct Memory_MapInfo_tag {
    unsigned int   src;
    /*!< Address to be mapped. */
    unsigned int   size;
    /*!< Size of memory region to be mapped. */
    unsigned int   dst;
    /*!< Mapped address. */
} Memory_MapInfo ;

static Memory_MapInfo memoryMap[PERF_UTILS_MAX_IDENX];

static volatile unsigned int* addrGptimerCtrlReg;
static volatile unsigned int* addrGptimerReadReg;
static volatile unsigned int* addrGptimerLoadReg;
static volatile unsigned int* addrGptimerBaseReg;
static volatile unsigned int* addrCmSysClkSelReg;
static volatile unsigned int* timeStampUserSpace;
static volatile unsigned int* addrCmL4GptimerBaseReg;
static volatile unsigned int* addrCmL4PGptimerCtrlReg;

static unsigned int sysClockFreq;
static unsigned int valuePrescalar;

static unsigned int
OsalDrv_ioMap (unsigned int addr, unsigned int size)
{
    unsigned int pageSize = getpagesize ();
    unsigned int taddr;
    unsigned int tsize;
    uintptr_t * userAddr = NULL;

	taddr = addr;
	tsize = size;
	/* Align the physical address to page boundary */
	tsize = tsize + (taddr % pageSize);
	taddr = taddr - (taddr % pageSize);
	userAddr = (uintptr_t *)mmap_device_io(tsize, taddr);
	if (userAddr == (uintptr_t *) MAP_FAILED) {
		userAddr = NULL;
		perror ("Failed to map memory to user space with mmap_device_io!");
	}
	else {
		/* Change the user address to reflect the actual user address of
		 * memory block mapped. This is done since during mmap memory block
		 * was shifted (+-) so that it is aligned to page boundary.
		 */
		taddr = (unsigned int)userAddr;
		taddr = taddr + (addr % pageSize);
	}

    return taddr;
}


/*!
 *  @brief  Function to unmap a memory region specific to the driver.
 *
 *  @sa     OsalDrv_close,OsalDrv_open,OsalDrv_map
 */
static void
OsalDrv_ioUnmap (unsigned int addr, unsigned int size)
{
    unsigned int pageSize = getpagesize ();
    unsigned int taddr;
    unsigned int tsize;
    unsigned int status;

	taddr = addr;
	tsize = size;

	/* Get the actual user address and size. Since these are modified at the
	 * time of mmaping, to have memory block page boundary aligned.
	 */
	tsize = tsize + (taddr % pageSize);
	taddr = taddr - (taddr % pageSize);
	status = munmap_device_io((uintptr_t) taddr, tsize);
	if (status == -1) {
		perror ("Failed to unmap memory with munmap_device_io! ");
	}
}

/*========= PerfUtils_calcSysClkFreq============
 * read the status from CM_SYS_CLK_REG
 * and will map that value to sys freq
 */
static
unsigned int PerfUtils_calcSysClkFreq (void)
{
    unsigned int reg;
    unsigned int sysClkFreq;
    static int statusInit = 0;
    Memory_MapInfo *cmSysClkSel;

    cmSysClkSel = &(memoryMap[PERF_UTIL_SYSCLK_INDEX]);
    cmSysClkSel->src  = CM_SYS_CLKSEL_REG;
    cmSysClkSel->size = 0x4;

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_calcSysClkFreq :Enter\n");
#endif
    if (0 == statusInit) {

       cmSysClkSel->dst = OsalDrv_ioMap (cmSysClkSel->src, cmSysClkSel->size);
        if(cmSysClkSel->dst == NULL) {
            printf("PerfUtils_calcSysClkFreq: Memory_map failed"
                                         "returned [0x%x]\n", errno);
            return -1;
         }
        statusInit = 1;
    }

    printf ("PerfUtils_calcSysClkFreq :Enter1\n");
    addrCmSysClkSelReg = (unsigned int*)cmSysClkSel->dst;
    printf ("PerfUtils_calcSysClkFreq :Enter2\n");
    reg = (unsigned int)*addrCmSysClkSelReg;
    printf ("PerfUtils_calcSysClkFreq :Enter3\n");
    //reg &= 0x00000007;

    printf ("PerfUtils_calcSysClkFreq :Enter4\n");
#if 0
    switch (reg) {
        case 0 :
                sysClkFreq = SYS_CLK_UNINITIALIZED;
                break;
        case 1 :
                sysClkFreq = SYS_CLK_12MHZ_FREQ;
                break;
        case 2 :
                sysClkFreq = SYS_CLK_13MHZ_FREQ;
                break;
        case 3 :
                sysClkFreq = SYS_CLK_16_8MHZ_FREQ;
                break;
        case 4 :
                sysClkFreq = SYS_CLK_19_2MHZ_FREQ;
                break;
        case 5 :
                sysClkFreq = SYS_CLK_26MHZ_FREQ;
                break;
        case 6 :
                sysClkFreq = SYS_CLK_27MHZ_FREQ;
                break;
        case 7 :
                sysClkFreq = SYS_CLK_38_4MHZ_FREQ;
                break;
    }
#endif
                sysClkFreq = SYS_CLK_38_4MHZ_FREQ;

    OsalDrv_ioUnmap(cmSysClkSel->dst, cmSysClkSel->size);
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf("SysClk is cloking @ [%d]\n",sysClkFreq);
    printf ("PerfUtils_calcSysClkFreq :Leave\n");
#endif
    return sysClkFreq;
}
/*
 *========= PerfUtils_setRegValue==============
 * Generic function for writing value to register
 */
static inline
void PerfUtils_setRegValue (volatile unsigned int *addrReg, unsigned int bitPos,
                                        unsigned int noBits, unsigned int value)
{
    unsigned int valueTmp = 0xFFFFFFFF;
    unsigned int valueMask = (0x1 << noBits);
    unsigned int valueStore;

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_setRegValue:Enter addrReg:[%x] *addrReg[%x] bitPos:[%x]\n"
                  "noBits:[%x] value:[%x]\n", (unsigned int)addrReg, (unsigned int)*addrReg, bitPos, noBits, value);
#endif
    valueMask -= 1;

    if (0 == value) {
        valueTmp ^= ((valueMask << bitPos));
        *(addrReg) &= valueTmp;
    }
    else {
        valueStore =  (value << bitPos);
#ifdef PERF_UTILS_DEBUG_SUPPORT
        printf ("PerfUtils_setRegValue:Writing value [%x] to [%x]\n",
                                                       valueStore,(unsigned int)addrReg);
#endif
        *(addrReg) |= valueStore;
    }
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_setRegValue:Leave addrReg:[%x] *addrReg[%x] bitPos:[%x]\n"
                  "noBits:[%x] value:[%x]\n", (unsigned int)addrReg, (unsigned int)*addrReg, bitPos, noBits, value);
#endif

}
/*
 *============= PerfUtils_gptimerModuleConfig===========
 * Configure the gptimer module for autoreload,prescalar
 */
static
int PerfUtils_gptimerModuleConfig (Perf_GptimerConfig *gptimerConfig)
{

    Memory_MapInfo *gptimerModConf;
    int status = PERF_UTILS_SUCCESS;
    unsigned int valueTest;

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig :Enter\n");
#endif
    if (NULL == gptimerConfig) {
        printf ("Module cconfiguration parameted passed is NULL\n");
        return PERF_UTILS_FAILURE;
    }
    gptimerModConf = &(memoryMap[PERF_UTIL_GPTIMER_INDEX]);
    gptimerModConf->src = GPTIMER10_MODULE_BASE_ADDR;
    gptimerModConf->size = GPTIMER_MODULE_REGSET_SIZE;

    gptimerModConf->dst = OsalDrv_ioMap(gptimerModConf->src, gptimerModConf->size);
    if (gptimerModConf->dst == NULL) {
        printf ("PerfUtils_gptimerModuleConfig:Memory_map\
                                   failed status [%x]\n", status);
        return status;
    }

    addrGptimerBaseReg = (unsigned int*)gptimerModConf->dst;
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig:addrGptimerBaseReg [%x]\n",
                                                       (unsigned int)addrGptimerBaseReg);
#endif
    addrGptimerCtrlReg = REG(GPTIMER, 10, CTRL, addrGptimerBaseReg);
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig:addrGptimerCtrlReg [%x]\n",
                                                       (unsigned int)addrGptimerCtrlReg);
#endif
    addrGptimerReadReg = REG(GPTIMER, 10, CRR, addrGptimerBaseReg);

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig:addrGptimerReadReg [%x]\n",
                                                       (unsigned int)addrGptimerReadReg);
#endif
    addrGptimerLoadReg = REG(GPTIMER, 10, LDR, addrGptimerBaseReg);
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig:addrGptimerLoadReg [%x]\n",
                                                       (unsigned int)addrGptimerLoadReg);
#endif

    /*  Setting to Autoreload mode
        0x0: One shot timer
        0x1: Autoreload timer
    */
    if (gptimerConfig->enableautoReload) {
        PerfUtils_setRegValue (addrGptimerCtrlReg,
                                GPTIMER_CTRLREG_AR_BIT_POS, 1, 1);
    }

    /*The timer counter is prescaled with the value 2(PTV+1).
    Example: PTV = 3, counter increases value (if started)
    after 16 functional clock periods.
    */
    if (gptimerConfig->enablePrescale) {
        PerfUtils_setRegValue (addrGptimerCtrlReg,GPTIMER_CTRLREG_PTV_BIT_POS,
                                                gptimerConfig->valuePrescalar, 1);
        valuePrescalar = (0x1 << 2);

    /* Enabling prescalar
    0x0: The TIMER clock input pin clocks the counter.
    0x1: The divided input pin clocks the counter.
    */
        PerfUtils_setRegValue (addrGptimerCtrlReg,
                                GPTIMER_CTRLREG_PRE_BIT_POS, 1, 0);
    }
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig:*addrGptimerCtrlReg [%x]\n",
                                                       *addrGptimerCtrlReg);
#endif
    /*Initializing Timer Counter value to 0 */
    *addrGptimerReadReg = 0x00000000;
    /*Initializing Loader Register value to 0 */
    *addrGptimerLoadReg = 0x00000000;

    timeStampUserSpace = addrGptimerReadReg;
    valueTest = *(timeStampUserSpace);

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_gptimerModuleConfig :Leave\n");
#endif
    return status;
}
/*
 *========= PerfUtils_startGpTimer======
 *          To start the gptimer
 */
void PerfUtils_startGpTimer (int gpTimerNo)
{
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_startGpTimer :Enter GPTIMER[%x]\n", gpTimerNo);
#endif
#ifdef USE_GPTIMER
    PerfUtils_setRegValue (addrGptimerCtrlReg,
                            GPTIMER_CTRLREG_ST_BIT_POS, 1, 1);
#endif
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_startGpTimer :Leave GPTIMER[%x]\n", gpTimerNo);
#endif
}
/*======= PerfUtils_stopGpTimer=========
 *        To  Stop the gptimer
 */

void PerfUtils_stopGpTimer (int gpTimerNo)
{
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_stopGpTimer :Enter GPTIMER[%x]\n", gpTimerNo);
#endif
#ifdef USE_GPTIMER
    PerfUtils_setRegValue (addrGptimerCtrlReg,
                            GPTIMER_CTRLREG_ST_BIT_POS, 1, 0);

    /* Resetting Read Register Value  */
    *addrGptimerReadReg = 0x00000000;
#endif
#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_stopGpTimer :Leave GPTIMER[%x]\n", gpTimerNo);
#endif
}
/*
 *============PerfUtils_selectClkSrc================
 * Selecting the clock src  b/w 32Khz and system clock
 */
static
int PerfUtils_selectClkSrc (int clkSrc)
{
    int status = PERF_UTILS_SUCCESS;
    unsigned int readvalue;
    Memory_MapInfo *clkMgmntL4Gptimer =
                &(memoryMap[PERF_UTILS_CML4_INDEX]);

    clkMgmntL4Gptimer->src = (CM_L4PER_GPTIMER_BASE_ADDR);
    clkMgmntL4Gptimer->size = (CM_L4PER_GPTIMER_REGSET_SIZE);

    clkMgmntL4Gptimer->dst = OsalDrv_ioMap(clkMgmntL4Gptimer->src, clkMgmntL4Gptimer->size);
    if (clkMgmntL4Gptimer->dst == NULL) {
        printf ("PerfUtils_selectClkSrc:Memory_map failed status[%x]\n", status);
        return status;
    }

  addrCmL4GptimerBaseReg = (unsigned int*) (clkMgmntL4Gptimer->dst);
  addrCmL4PGptimerCtrlReg = REG(CM_L4PER_GPTIMER, 10, CLKCTRL,
                                             addrCmL4GptimerBaseReg);
  readvalue  = *addrCmL4PGptimerCtrlReg;
  while (!(readvalue & 0x02)) {
        PerfUtils_setRegValue (addrCmL4PGptimerCtrlReg, 0, 2, 2);
        readvalue = *addrCmL4PGptimerCtrlReg;
    }

  OsalDrv_ioUnmap(clkMgmntL4Gptimer->dst, clkMgmntL4Gptimer->size);
    return status;
}

/*
 * ========= PerfUtils_init ========
 */
int PerfUtils_init()
{
    int status;
    unsigned int sysClkFreq;
    Perf_GptimerConfig gpTimerConfig;

    PerfUtils_selectClkSrc (10);
#ifdef USE_GPTIMER
    sysClkFreq = PerfUtils_calcSysClkFreq();
    if (0 == sysClkFreq) {
        printf("PerfUtils_init : SYSCLK not initialized in kernel\n"
                     "can't  use  sys clk timer for profiling\n");
        status = PERF_UTILS_INIT_FAILED;
        return status;
    }

    sysClockFreq = sysClkFreq;
    gpTimerConfig.enableautoReload = 1;
    gpTimerConfig.enablePrescale = 0;
    gpTimerConfig.valuePrescalar = 0;
    gpTimerConfig.gpTimer =10;
    status = PerfUtils_gptimerModuleConfig (&gpTimerConfig);
    if (0 > status) {
        printf ("PerfUtils_init:PerfUtils_gptimerModuleConfig"
                                                "failed status[%x]\n", status);
        return status;
    }
#endif

    perfRecordsUserSpace = perfRecordsArr;

    memset(perfRecordsUserSpace, 0, sizeof(perfRecordsUserSpace)
        * MAX_NUM_USERSPACE_RECORDS);

	return status;
}

/*
 * ========PerfUtils_destroy========
 * To destroy the gptimer module
 */
int PerfUtils_destroy (void)
{
    int status = 0;

    OsalDrv_ioUnmap((unsigned int)addrGptimerBaseReg, GPTIMER_MODULE_REGSET_SIZE);

    return status;
}


/*
 * ======== PerfUtils_addFxn ========
 * To add functions for profiling
 */
void PerfUtils_addFxn(unsigned int fxnIndex, char * fxnName, unsigned int clientproc, unsigned int serverproc, unsigned int msgsize)
{
#ifdef USE_GPTIMER
    Perf_FunctionRecord *record = &(perfRecordsUserSpace[fxnIndex]);
#else
    Perf_LocalRecord    *record = &(perfRecordsUserSpace[fxnIndex]);
#endif

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf("PerfUtils_addFxn: Enter\n");
    printf("PerfUtils_addFxn: Adding function %s\n",fxnName);
#endif

    strcpy(record->fxnName, fxnName);
    record->client_proc = clientproc;
    record->server_proc = serverproc;
    record->msg_size = msgsize;

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf("PerfUtils_addFxn: Leave\n");
#endif
}

/*
 * ============= PerfUtils_enter ============
 * Record enter timing for the registered function
 */
int PerfUtils_enter(unsigned int fxnIndex)
{
    int status = 0;
    int noEntry = 0;
#ifdef USE_GPTIMER
    Perf_FunctionRecord *record = (perfRecordsUserSpace + fxnIndex);
#else
    Perf_LocalRecord *record = (perfRecordsUserSpace + fxnIndex);
#endif

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf ("PerfUtils_enter: Enter\n");
#endif
    noEntry = record->numEntries;


    if(noEntry < MAX_ENTRIES_PER_FUNCTION) {
#ifdef USE_GPTIMER
        record->enterTimes[record->numEntries] = *(timeStampUserSpace);
#else
        status = gettimeofday(&(record->enterTimes[noEntry]),NULL);

#ifdef PERF_UTILS_DEBUG_SUPPORT
        printf("enter Timestamp sec [%d] usec [%d] \n",record->enterTimes[noEntry].tv_sec,
                                                            record->enterTimes[noEntry].tv_usec);
#endif
        if (0 != status) {
            printf("PerfUtils_enter:error in getimeofday\n");
        }
#endif
    }

#ifdef PERF_UTILS_DEBUG_SUPPORT
   printf("PerfUtils_enter: Leave\n");
#endif
    return status;
}

/*
 * ============= PerfUtils_exit ===============
 * Record the exit timing for the registered function
 */
int PerfUtils_exit(unsigned int fxnIndex)
{
    int status = 0;
    int noEntry = 0;
#ifdef USE_GPTIMER
    unsigned int exitTime = *(timeStampUserSpace);
    Perf_FunctionRecord *record = (perfRecordsUserSpace + fxnIndex);
#else
    Perf_LocalRecord *record = (perfRecordsUserSpace + fxnIndex);
#endif

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf("PerfUtils_exit: Enter\n");
#endif

    noEntry = record->numEntries;
    if(noEntry < MAX_ENTRIES_PER_FUNCTION) {

#ifdef USE_GPTIMER
        record->exitTimes[record->numEntries] = exitTime;
#else
        status = gettimeofday(&(record->exitTimes[noEntry]),NULL);
#endif

#if defined(PERF_UTILS_DEBUG_SUPPORT) && !defined(USE_GPTIMER)
        printf("exit Timestamp sec [%d] usec [%d] \n",record->exitTimes[noEntry].tv_sec,
                                                            record->exitTimes[noEntry].tv_usec);
#endif
        if (0 != status) {
            printf("PerfUtils_enter:error in getimeofday\n");
        }

        record->numEntries++;
    }

#ifdef PERF_UTILS_DEBUG_SUPPORT
    printf("PerfUtils_exit: Leave\n");
#endif

    return status;
}

static
void PerfUtils_printUserSpaceReport (void)
{
    unsigned int i, j;
    unsigned int sysClkFreq;
    unsigned int enterTime, exitTime;
    unsigned int totalTimeuSec, diff;
#if 0
    FILE *fp;
#endif
#ifdef USE_GPTIMER
    Perf_FunctionRecord *record;
    sysClkFreq = sysClockFreq;
    sysClkFreq = sysClkFreq/100000;
#else
    Perf_LocalRecord *record;
#endif
#if 0
	fp  = fopen("Report.txt","w+");

    if (NULL == fp){
        printf("Opening file filed\n");
        exit(0);
     }
#endif
    printf("============UserSpace report============\n");

    for(j = 0; j < MAX_NUM_USERSPACE_RECORDS; j++) {
        record = (perfRecordsUserSpace + j);
        for(i = 0; i < record->numEntries; i++) {
            if (0 == i) {
                printf("%s\n",record->fxnName);
                printf("clientproc %u\n",record->client_proc);
                printf("serverproc %u\n",record->server_proc);
                printf("msgsize %u\n",record->msg_size);
                printf("count  %u\n",record->numEntries);
                totalTimeuSec = 0;
            }
#ifdef USE_GPTIMER
            enterTime = record->enterTimes[i];
            exitTime =  record->exitTimes[i];
#else
            enterTime = ((record->enterTimes[i].tv_sec * 1000000) + record->enterTimes[i].tv_usec);
            exitTime = ((record->exitTimes[i].tv_sec * 1000000) + record->exitTimes[i].tv_usec);
#endif
            diff = (exitTime - enterTime);
#ifdef USE_GPTIMER
            /* Converting to usecs */
            totalTimeuSec += (diff * 10 / sysClkFreq);
            printf ("> %s took %u us or period %u\n", record->fxnName, diff * 10 / sysClkFreq, diff);
#else
            printf("record->enterTimes[i].tv_sec [%d] record->exitTimes[i].tv_sec [%d]\n",
                        record->enterTimes[i].tv_sec,record->exitTimes[i].tv_sec);
            printf("record->enterTimes[i].tv_usec [%d] record->exitTimes[i].tv_usec [%d]\n",
                        record->enterTimes[i].tv_usec,record->exitTimes[i].tv_usec);
            printf ("> %s took %u us\n", record->fxnName, diff);
#endif

        }
        if (record->numEntries > 0) {
            printf("========================================\n");
            printf("AVG time: %u us\n", totalTimeuSec / record->numEntries);
            printf("========================================\n");
        }
    }
//    fclose(fp);
    printf("===========End of UserSpace report======\n");
}


/*
 * ======== PerfUtils_printReport ========
 *          Print report to user
 */

void PerfUtils_printReport (int printLevel)
{
    if (printLevel == 1) {
        PerfUtils_printUserSpaceReport ();
    }
}

