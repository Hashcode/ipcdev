/*
 *  @file   ClockOps.c
 *
 *  @brief      Generic clock module interface across platforms and OS
 *
 *
 *  ============================================================================
 *
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
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


/* Standard headers */
#include <ti/syslink/Std.h>
/* Module level headers */
#include <ti/syslink/inc/ClockOps.h>

#ifndef SYSLINK_BUILDOS_QNX
/* OSAL & Utils headers */
#include <ti/syslink/utils/Trace.h>
#else /* QNX */
/* standard include for reg access */
#include <stdint.h>
#include <sys/mman.h>
#include <hw/inout.h>

#include <ti/syslink/inc/Dm8168Clock.h>
//#include <ti/syslink/inc/ClockOps.h>
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/utils/String.h>

#endif //SYSLINK_BUILDOS_


#if defined (__cplusplus)
extern "C" {
#endif

#ifndef SYSLINK_BUILDOS_QNX

/* =============================================================================
 * APIs
 * =============================================================================
 */
/*!
 *  @brief      Function returns the Clock handle
 *
 *  @handle     Ptr to the clockOps object.conaining the clock function.table
 *  @clkname    name of clk for which handle is to be obtained
 *
 *  @sa         ClockOps_put
 */
Ptr
ClockOps_get (ClockOps_Handle handle, String clkName)
{

    return(handle->clkFxnTable.get(clkName));
}

/*!
 *  @brief       Function to release a Clock
 *
 *  @handle      Ptr to the clockOps object.conaining the clock function.table
 *  @clkHandle   Handle to the clock
 *
 *  @sa         ClockOps_get
 */
Void
ClockOps_put(ClockOps_Handle handle, Ptr clkHandle)
{
    handle->clkFxnTable.put(clkHandle);
}
/*!
 *  @brief       Function to enable a Clock
 *
 *  @handle      Ptr to the clockOps object.conaining the clock function.table
 *  @clkHandle   Handle to the clock
 *
 *  @sa         ClockOps_disable
 */
Int32
ClockOps_enable(ClockOps_Handle handle, Ptr clkHandle)
{
    return (handle->clkFxnTable.enable(clkHandle));
}

/*!
 *  @brief       Function to disable a Clock
 *
 *  @handle      Ptr to the clockOps object.conaining the clock function.table
 *  @clkHandle   Handle to the clock
 *
 *  @sa         ClockOps_enable
 */
Void
ClockOps_disable(ClockOps_Handle handle, Ptr clkHandle)
{
    handle->clkFxnTable.disable(clkHandle);
}

/*!
 *  @brief      Function gets the Clock speed .
 *
 *  @handle     Ptr to the clockOps object.conaining the clock function.table
 *  @clkHandle   Handle to the clock
 *
 *  @sa         ClockOps_setRate
 */
ULong
ClockOps_getRate(ClockOps_Handle handle, Ptr clkHandle)
{
    return(handle->clkFxnTable.getRate(clkHandle));
}

/*!
 *  @brief       Function sets the dlock speed
 *
 *  @handle      Ptr to the clockOps object.conaining the clock function.table
 *  @clkHandle   Handle to the clock
 *  @rate        Clock speed to be set
 *
 *  @sa         ClockOps_getRate
 */
Int32
ClockOps_setRate(ClockOps_Handle handle, Ptr clkHandle, ULong rate)
{
    return (handle->clkFxnTable.setRate(clkHandle, rate));
}

#else /* QNX */

/*Function Prototypes */
void prcm_enable(UInt32 clkstctrl, UInt32 clkctrl, UInt32 regVal1, UInt32 regVal2, UInt32 cmpVal1, UInt32 cmpVal2, UInt32 wait1, UInt32 wait2);
void GEMSSClkEnable(void);
void GEMSSClkDisable(void);;
void DucatiClkEnable(void) ;
void DucatiClkDisable(void) ;
void prcm_disable_spinbox(void);
void prcm_disable_mailbox(void);
void prcm_disable_gptimer4(void);



#define LAST_CORE 1

String handleArray[] = {

"spinbox_ick",
"mailbox_ick",
"gpt4_ick",
"gpt4_fck",
"mmu_ick",
"mmu_cfg_ick",
"gem_ick" /* dsp_ick string is changed in latest linux release to gem_ick*/
};

typedef enum {
	SPINBOX=1,
	MAILBOX=2,
	IGPTIMER4=3,
	FGPTIMER4=4,
	MMU=5,
	MMUCFG=6,
	DSP=7,
	DUCATI=8
}clkType;

/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
#define PRCM_BASE_ADDR              0x48180000
#define PRCM_SIZE                   0x00003000
#define PM_ACTIVE_PWRSTST           0x00000A04

#define CM_MMU_CLKSTCTRL            0x0000140C
#define CM_ALWON_MMUDATA_CLKCTRL    0x0000159C
#define CM_MMUCFG_CLKSTCTRL         0x00001410
#define CM_ALWON_MMUCFG_CLKCTRL     0x000015A8

#define GEM_L2RAM_BASE_ADDR         0x40800000
#define GEM_L2RAM_SIZE              0x00040000
#define DSPMEM_SLEEP                0x00000650

#define OSC_0                       20
#define OSC_1                       20
#define PLL_BASE_ADDRESS            0x481C5000
#define DSP_PLL_BASE                (PLL_BASE_ADDRESS+0x080)
#define DSPBOOTADDR                 0x00000048
#define CM_ALWON_SPINBOX_CLKCTRL    0x00001598
#define CM_ALWON_MAILBOX_CLKCTRL    0x00001594
#define CM_ALWON_L3_SLOW_CLKSTCTRL  0x00001400
#define CM_ALWON_TIMER_4_CLKCTRL    0x0000157C
#define CTRL_MODULE_BASE_ADDR       0x48140000

#define RM_DEFAULT_RSTCTRL          0x00000B10
#define RM_DEFAULT_RSTST            0x00000B14
#define CM_DEFAULT_DUCATI_CLKSTCTRL 0x00000518
#define CM_DEFAULT_DUCATI_CLKCTRL   0x00000574

/* for gptimer4 fck */
#define CM_SYSCLK18_CLKSEL          0x00000378
#define CM_DMTIMER_CLKSRC           0x000002E0
#define CM_SYSCLK18_CLKSRC          0x000002F0
#define CM_DPLL_SYSCLK18_CLKSEL     (PRCM_BASE_ADDR+CM_SYSCLK18_CLKSEL)
#define SYSCLK18_CLKSRC             (PLL_BASE_ADDRESS+CM_SYSCLK18_CLKSRC)
#define DMTIMER_CLKSRC              (PLL_BASE_ADDRESS+CM_DMTIMER_CLKSRC)

#define ADPLLJ_CLKCRTL_HS2	0x00000801 //HS2 Mode,TINTZ =1  --used by all PLL's except HDMI

/* ISS PLL releated defines */
#define ISS_PLL_BASE            (PLL_BASE_ADDRESS+0x140)


//ADPLL intrnal Offset Registers
#define CLKCTRL                                 0x4
#define TENABLE                                 0x8
#define TENABLEDIV                              0xC
#define M2NDIV                                  0x10
#define MN2DIV                                  0x14
#define STATUS                                  0x24

/*!
 *  @brief  PRM address for GEM
 */
#define CM_GEM_CLKSTCTRL                0x400
/*!
 *  @brief  Clock mgmt for GEM
 */
#define CM_ACTIVE_GEM_CLKCTRL           0x420
/*!
 *  @brief  Reset control for GEM
 */
#define RM_ACTIVE_RSTCTRL               0xA10
/*!
 *  @brief  Reset status for GEM
 */
#define RM_ACTIVE_RSTST                 0xA14

#define REMAP(x)             (mmap_device_io(4,x))
#define UNMAP(x)             (munmap_device_io(x,4))
#define REGWR(x,y)           (out32(x,y))
#define REGRD(x)             (in32(x))
#define REGRDMWR(x, mask, val) REGWR(x,((REGRD(x) & mask) | val))

/* =============================================================================
 * Static globals
 * =============================================================================
 */
UInt32 refSpinCount = 0;
UInt32 refMboxCount = 0;
UInt32 refGptimer4ick = 0;
UInt32 refGptimer4fck = 0;
UInt32 refDSP = 0;
UInt32 refDucati = 0;

/* =============================================================================
 * APIs called by DM8168VIDEOM3PROC module
 * =============================================================================
 */
/*!
 *  @brief      Function returns the clock handle .
 *
 *  @clkHandle   clk handle returned to corresponding clk name
 *  @clkname     name of clk for which handle is to be obtained
 *
 *  @sa         DM8168CLOCK_put
 */
Ptr
ClockOps_get (ClockOps_Handle handle, String clkName)
{
	clkType ctype = SPINBOX;

	if (!String_cmp(clkName,"spinbox_ick")) {
		ctype = SPINBOX;
	}
	else if (!String_cmp(clkName,"mailbox_ick")) {
		ctype = MAILBOX;
	}
	else if (!String_cmp(clkName,"gpt4_ick")) {
		ctype = IGPTIMER4;
	}
	else if (!String_cmp(clkName,"gpt4_fck")) {
		ctype = FGPTIMER4;
	}
	else if (!String_cmp(clkName,"mmu_ick")) {
		ctype = MMU;
	}
	else if (!String_cmp(clkName,"mmu_cfg_ick")) {
		ctype = MMUCFG;
	}
/* dsp_ick string is changed in latest linux release to gem_ick*/
	else if (!String_cmp(clkName,"gem_ick")) {
		ctype = DSP;
	}
	else if (!String_cmp(clkName,"ducati_ick")) {
		ctype = DUCATI;
	}
	else {
		/* should not come here */
	}

    return ((Ptr)ctype);
}

/*!
 *  @brief      Function returns the clock handle .
 *
 *  @clkHandle   clk handle returned to corresponding clk name
 *  @clkname     name of clk for which handle is to be obtained
 *
 *  @sa         DM8168CLOCK_put
 */
Void
ClockOps_put(ClockOps_Handle handle, Ptr clkHandle)
{
    //clk_put((struct clk *)clkHandle);
}
/*!
 *  @brief      Function returns the clock handle .
 *
 *  @clkHandle   clk handle returned to corresponding clk name
 *  @clkname     name of clk for which handle is to be obtained
 *
 *  @sa         DM8168CLOCK_put
 */
Int32
ClockOps_enable(ClockOps_Handle handle, Ptr clkHandle)
{
	switch ((UInt32)clkHandle){

			case SPINBOX:
				if (refSpinCount == 0) {
					/*Enable Clock to SPIN box*/
					prcm_enable(0, PRCM_BASE_ADDR+CM_ALWON_SPINBOX_CLKCTRL, 0 , 2, 0x0, 0x100, TRUE, FALSE);
					refSpinCount++;
				}
				else if(refSpinCount > 0) {
					refSpinCount++;
				}
				else {
					/* refSpinCount should not be less than zero */
				}
				break;
			case MAILBOX:
				if (refMboxCount == 0) {
					/*Enable Clock to mail box*/
					prcm_enable(0, PRCM_BASE_ADDR+CM_ALWON_MAILBOX_CLKCTRL, 0 , 2, 0x0, 0x100, TRUE, FALSE);
					refMboxCount++;
				}
				else if(refMboxCount > 0) {
					refMboxCount++;
				}
				else {
					/* refMboxCount should not be less than zero */
				}
				break;
			case IGPTIMER4:
				if (refGptimer4ick == 0) {
					prcm_enable(PRCM_BASE_ADDR+CM_ALWON_L3_SLOW_CLKSTCTRL , PRCM_BASE_ADDR+CM_ALWON_TIMER_4_CLKCTRL, 2, 2, 0x0, 0x100, TRUE, TRUE);
					refGptimer4ick++;
				}
				else if(refGptimer4ick > 0) {
					refGptimer4ick++;
				}
				else {
					/* refGptimer4ick should not be less than zero */
				}
				break;
			case FGPTIMER4:
				if (refGptimer4fck == 0) {
#ifdef CLOCK_FIX
                    uintptr_t sysclk18ClksrcPtr = REMAP(SYSCLK18_CLKSRC);
                    uintptr_t clkselPtr = REMAP(CM_DPLL_SYSCLK18_CLKSEL);
                    uintptr_t dmtimerClksrcPtr = REMAP(DMTIMER_CLKSRC);

                    /* CLKIN32/RTCDIVIDER multiplexor */
                    REGRDMWR(sysclk18ClksrcPtr, 0xFFFFFFFE, 0);

                    /* sysclk18 */
                    REGRDMWR(clkselPtr, 0xFFFFFFFE, 0);

                    /* timer4 */
                    REGRDMWR(dmtimerClksrcPtr, 0xFFF8FFFF, 0);

                    UNMAP(sysclk18ClksrcPtr);
                    UNMAP(clkselPtr);
                    UNMAP(dmtimerClksrcPtr);
#endif
					refGptimer4fck++;
				}
				else if(refGptimer4fck > 0) {
					refGptimer4fck++;
				}
				else {
					/* refGptimer4fck should not be less than zero */
				}
				break;
			case MMU:
				/* required config is done as a part of the case DSP */
				break;
			case MMUCFG:
				/* required config is done as a part of the case DSP */
				break;
			case DSP:
				if (refDSP == 0) {
					GEMSSClkEnable();
					refDSP++;
				}
				break;
			case DUCATI:
				if (refDucati == 0) {
					DucatiClkEnable();
					refDucati++;
				}
				break;
			default:
				break;

			}

    return 1;
}
/*!
 *  @brief      Function returns the clock handle .
 *
 *  @clkHandle   clk handle returned to corresponding clk name
 *  @clkname     name of clk for which handle is to be obtained
 *
 *  @sa         DM8168CLOCK_put
 */
Void
ClockOps_disable(ClockOps_Handle handle, Ptr clkHandle)
{
   switch ((UInt32)clkHandle){

			case SPINBOX:
				/*disable Clock to SPIN box*/
				if (refSpinCount == LAST_CORE) {
					prcm_disable_spinbox();
					refSpinCount = 0;
				}
				else if(refSpinCount > LAST_CORE) {
					refSpinCount--;
				}
				else {
					/* refSpinCount is less that or equal to zero */
				}
				break;
			case MAILBOX:
				/*disable Clock to MAIL box*/
				if (refMboxCount == LAST_CORE) {
					prcm_disable_mailbox();
					refMboxCount = 0;
				}
				else if(refMboxCount > LAST_CORE) {
					refMboxCount--;
				}
				else {
					/* refMboxCount is less that or equal to zero */
				}
				break;
			case IGPTIMER4:
				if ( refGptimer4ick == LAST_CORE) {
					prcm_disable_gptimer4();
					refGptimer4ick = 0;
					refGptimer4fck = 0;
				}
				else if(refGptimer4ick > LAST_CORE) {
					refGptimer4ick--;
				}
				else {
					/* refGptimer4ick is less that or equal to zero */
				}
				break;
			case FGPTIMER4:
				break;
			case MMU:
				break;
			case MMUCFG:
				break;
			case DSP:
				if (refDSP > 0 ) {
					GEMSSClkDisable();
					refDSP = 0;
				}
				break;
			case DUCATI:
				if (refDucati > 0) {
					DucatiClkDisable();
					refDucati = 0;
				}
				break;
			default:
				break;

			}
}

/*!
 *  @brief      Function returns the clock handle .
 *
 *  @clkHandle   clk handle returned to corresponding clk name
 *  @clkname     name of clk for which handle is to be obtained
 *
 *  @sa         DM8168CLOCK_put
 */
ULong
ClockOps_getRate(ClockOps_Handle handle, Ptr clkHandle)
{
    //return(clk_get_rate((struct clk *)clkHandle));
    return 1;
}

/*!
 *  @brief      Function returns the clock handle .
 *
 *  @clkHandle   clk handle returned to corresponding clk name
 *  @clkname     name of clk for which handle is to be obtained
 *
 *  @sa         DM8168CLOCK_put
 */
Int32
DM8168CLOCK_setRate(Ptr clkHandle, ULong rate)
{
    //return (clk_set_rate((struct clk *)clkHandle, rate));
    return 1;
}

void PLL_Clocks_Config(UInt32 Base_Address,UInt32 OSC_FREQ,UInt32 N,UInt32 M,UInt32 M2,UInt32 CLKCTRL_VAL)
{
    UInt32 m2nval,mn2val,read_clkctrl;
    Int32 i = 0;

    uintptr_t m2ndivPtr = REMAP(Base_Address+M2NDIV);
    uintptr_t mn2divPtr = REMAP(Base_Address+MN2DIV);
    uintptr_t tenabledivPtr = REMAP(Base_Address+TENABLEDIV);
    uintptr_t tenablePtr = REMAP(Base_Address+TENABLE);
    uintptr_t clkctrlPtr = REMAP(Base_Address+CLKCTRL);
    uintptr_t statusPtr = REMAP(Base_Address+STATUS);

    m2nval = (M2<<16) | N;
    mn2val =  M;

    REGWR(m2ndivPtr, m2nval);
    REGWR(mn2divPtr, mn2val);
    REGWR(tenabledivPtr, 0x1);
    REGWR(tenabledivPtr, 0x0);
    REGWR(tenablePtr, 0x1);
    REGWR(tenablePtr, 0x0);

    read_clkctrl = REGRD(clkctrlPtr);

    //configure the TINITZ(bit0) and CLKDCO BITS IF REQUIRED
    REGWR(clkctrlPtr, ((read_clkctrl & 0xff7fe3ff) | CLKCTRL_VAL));

    read_clkctrl = REGRD(clkctrlPtr);

    // poll for the freq,phase lock to occur
    while ((REGRD(statusPtr) & 0x00000600) != 0x00000600);

    //wait fot the clocks to get stabized
    for (i = 0; i <1000; i++)
    {
        ;
    }

    UNMAP(m2ndivPtr);
    UNMAP(mn2divPtr);
    UNMAP(tenabledivPtr);
    UNMAP(tenablePtr);
    UNMAP(clkctrlPtr);
    UNMAP(statusPtr);
}

void prcm_enable(UInt32 clkstctrl, UInt32 clkctrl, UInt32 regVal1, UInt32 regVal2, UInt32 cmpVal1, UInt32 cmpVal2, UInt32 wait1, UInt32 wait2){
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_GEM_CLKSTCTRL);

    if (clkstctrl != 0) {
        uintptr_t clkstctrlArgPtr = REMAP(clkstctrl);

        /*reg write using out */
        REGWR(clkstctrlArgPtr, regVal1);

        UNMAP(clkstctrlArgPtr);
    }

    if (clkstctrl == REGRD(clkstctrlPtr)) {
        uintptr_t pwrststPtr = REMAP(PRCM_BASE_ADDR+PM_ACTIVE_PWRSTST);
        while (REGRD(pwrststPtr)!= 0x37);	/*Check Power is ON*/
        UNMAP(pwrststPtr);
    }
    UNMAP(clkstctrlPtr);

    if (clkctrl != 0) {
        /*reg write using out */
        uintptr_t clkctrlArgPtr = REMAP(clkctrl);
        REGWR(clkctrlArgPtr, regVal2);
        UNMAP(clkctrlArgPtr);
    }

    if (wait1) {
        uintptr_t clkctrlArgPtr = REMAP(clkctrl);
        while ((REGRD(clkctrlArgPtr)&cmpVal1)!=cmpVal1);
        UNMAP(clkctrlArgPtr);
    }

    if (wait2) {
        uintptr_t clkstctrlArgPtr = REMAP(clkstctrl);
        while ((REGRD(clkstctrlArgPtr)&cmpVal2)!=cmpVal2);
        UNMAP(clkstctrlArgPtr);
    }
}

void prcm_disable_spinbox(void){
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ALWON_SPINBOX_CLKCTRL);
    REGRDMWR(clkctrlPtr, 0xFFFFFFFD, 0);
    UNMAP(clkctrlPtr);
}

void prcm_disable_mailbox(void){
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ALWON_MAILBOX_CLKCTRL);
    REGRDMWR(clkctrlPtr, 0xFFFFFFFD, 0);
    UNMAP(clkctrlPtr);
}

void prcm_disable_gptimer4(void){
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ALWON_TIMER_4_CLKCTRL);
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ALWON_L3_SLOW_CLKSTCTRL);

    REGRDMWR(clkctrlPtr, 0xFFFFFFFD, 0);

    REGRDMWR(clkstctrlPtr, 0xFFFFFFFD, 0);
    UNMAP(clkctrlPtr);
    UNMAP(clkstctrlPtr);
}

void prcm_disable_gem(void){
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ACTIVE_GEM_CLKCTRL);
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_GEM_CLKSTCTRL);

    REGRDMWR(clkctrlPtr, 0xFFFFFFFD, 0);

    REGRDMWR(clkstctrlPtr, 0xFFFFFFFD, 0);
    UNMAP(clkctrlPtr);
    UNMAP(clkstctrlPtr);
}

void prcm_disable_mmu(void){
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ALWON_MMUDATA_CLKCTRL);
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_MMU_CLKSTCTRL);

    REGRDMWR(clkctrlPtr, 0xFFFFFFFD, 0);

    REGRDMWR(clkstctrlPtr, 0xFFFFFFFD, 0);
    UNMAP(clkctrlPtr);
    UNMAP(clkstctrlPtr);
}

void prcm_disable_mmuconfig(void){
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_ALWON_MMUCFG_CLKCTRL);
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_MMUCFG_CLKSTCTRL);

    REGRDMWR(clkctrlPtr, 0xFFFFFFFD, 0);

    REGRDMWR(clkstctrlPtr, 0xFFFFFFFD, 0);
    UNMAP(clkctrlPtr);
    UNMAP(clkstctrlPtr);
}


void DucatiClkEnable(void)
{
    UInt32 val;
    uintptr_t rstctrlPtr = REMAP(PRCM_BASE_ADDR+RM_DEFAULT_RSTCTRL);
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_DEFAULT_DUCATI_CLKSTCTRL);
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_DEFAULT_DUCATI_CLKCTRL);

#ifdef CLOCK_FIX
    /* set the ISS PLL before the clock enable */
    PLL_Clocks_Config(ISS_PLL_BASE, OSC_0, 19, 800, 2, ADPLLJ_CLKCRTL_HS2);
#endif

    /* enable the ducati logic */
    val = REGRD(rstctrlPtr);
    val &= (~(0x1 << 4));
    REGWR(rstctrlPtr,   val);

    /* enable power domain transition */
    REGWR(clkstctrlPtr, 2);

    /* enable ducati clocks */
    REGWR(clkctrlPtr,   2);

    /* wait for clocks CLKIN200TR and CLKINTR to become active */
    do {
        val = REGRD(clkstctrlPtr);
    } while ((val & 0x300) != 0x300);

    /* wait for module to be fully functional */
    do {
        val = REGRD(clkctrlPtr);
    } while ((val & 0x30000) != 0);

    UNMAP(rstctrlPtr);
    UNMAP(clkstctrlPtr);
    UNMAP(clkctrlPtr);

}

void DucatiClkDisable(void)
{
    uintptr_t rstctrlPtr = REMAP(PRCM_BASE_ADDR+RM_DEFAULT_RSTCTRL);
    uintptr_t clkctrlPtr = REMAP(PRCM_BASE_ADDR+CM_DEFAULT_DUCATI_CLKCTRL);
    uintptr_t clkstctrlPtr = REMAP(PRCM_BASE_ADDR+CM_DEFAULT_DUCATI_CLKSTCTRL);
    UInt32 val;

    /* assert reset for cores m3_0 and m3_1 */
    val = REGRD(rstctrlPtr);
    val |= (0x3 << 2);
    REGWR(rstctrlPtr, val);

    /* disable ducati clocks */
    REGWR(clkctrlPtr, 0);

    /* disable power domain transition */
    REGWR(clkstctrlPtr, 0);

    /* disable the ducati logic */
    val = REGRD(rstctrlPtr);
    val |= (0x1 << 4);
    REGWR(rstctrlPtr, val);

    /* ensure posted write has completed */
    do {
        val = REGRD(rstctrlPtr);
    } while ((val & (0x1 << 4)) == 0);

    UNMAP(rstctrlPtr);
    UNMAP(clkctrlPtr);
    UNMAP(clkstctrlPtr);
}


void GEMSSClkEnable(void)
{

    /* set the DSP PLL before clock enable */
    PLL_Clocks_Config(DSP_PLL_BASE,OSC_0,19,500,1,ADPLLJ_CLKCRTL_HS2);

    /*Enable Clock to MMU CFG*/
    prcm_enable(PRCM_BASE_ADDR+CM_MMUCFG_CLKSTCTRL, PRCM_BASE_ADDR+CM_ALWON_MMUCFG_CLKCTRL, 2 , 2, 0x0, 0x100, TRUE, TRUE);

    /*Enable Clock to Data*/
    prcm_enable(PRCM_BASE_ADDR+CM_MMU_CLKSTCTRL, PRCM_BASE_ADDR+CM_ALWON_MMUDATA_CLKCTRL, 2 , 2, 0x0, 0x100, TRUE, TRUE);

    /*Enable Clock to GEMSS*/
    prcm_enable(PRCM_BASE_ADDR+CM_GEM_CLKSTCTRL, PRCM_BASE_ADDR+CM_ACTIVE_GEM_CLKCTRL, 2 , 2, 0x0, 0x700, FALSE, TRUE);

}

void GEMSSClkDisable(void)
{
    prcm_disable_gem();
    prcm_disable_mmu();
    prcm_disable_mmuconfig();
}

#endif //SYSLINK_BUILDOS

#if defined (__cplusplus)

}
#endif
