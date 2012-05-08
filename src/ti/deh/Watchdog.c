/*
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *  ======== Watchdog.c ========
 *
 */

#include <xdc/runtime/Assert.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Types.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/interfaces/ITimer.h>
#include <ti/sysbios/timers/dmtimer/Timer.h>

#ifdef DSP
#include <ti/sysbios/family/c64p/Hwi.h>
#endif

#ifdef M3_ONLY
#include <ti/sysbios/family/arm/m3/Hwi.h>
#endif

#include "package/internal/Watchdog.xdc.h"

/* Macro to write to GP timer registers */
#define REG32(A)   (*(volatile UInt32 *) (A))

/* Bit mask for writing into GP timer TGRR reg for posted write mode */
#define WATCHDOG_TIMER_TWPS_W_PEND_TGRR     0x8

/* Bit mask for writing into GP timer TLDR reg for posted write mode */
#define WATCHDOG_TIMER_TWPS_W_PEND_TLDR     0x4

/* Bit mask for writing into GP timer TCRR reg for posted write mode */
#define WATCHDOG_TIMER_TWPS_W_PEND_TCRR     0x2

/* Bit mask for writing into GP timer TCLR reg for posted write mode */
#define WATCHDOG_TIMER_TWPS_W_PEND_TCLR     0x1

/* GP Timers Clock Control register bit mask */
#define WATCHDOG_WDT_CLKCTRL_IDLEST_MASK    (3 << 16)


/*
 *  ======== Watchdog_init ========
 */
Void Watchdog_init( Void (*timerFxn)(Void) )
{
    Hwi_Params          hwiParams;
    UInt                key;
    Timer_Handle        tHandle;
    Types_FreqHz        tFreq;
    Watchdog_TimerRegs  *timer;

    tHandle = Timer_Object_get(NULL, 0);
    Timer_getFreq(tHandle, &tFreq);  /* get timer frequency */

    timer = (Watchdog_TimerRegs *) Watchdog_module->device[0].baseAddr;

    /* Check if timer is enabled by host-side */
    if ((REG32(Watchdog_module->device[0].clkCtrl) &
        WATCHDOG_WDT_CLKCTRL_IDLEST_MASK) == WATCHDOG_WDT_CLKCTRL_IDLEST_MASK) {
        System_printf("Watchdog disabled: TimerBase = 0x%x ClkCtrl = 0x%x\n",
                                    timer, Watchdog_module->device[0].clkCtrl);
        return;
    }

    /* Update timer registers */
    timer->tisr = ~0;
    timer->tier = 2;
    timer->twer = 2;
    timer->tldr = (0 - (tFreq.lo * Watchdog_TIME_SEC));
    timer->tsicr |= 4; /* enable posted write mode */

    /* Booting can take more time, so set CRR to WDT_TIME_BOOT */
    timer->tcrr = (0 - (tFreq.lo * Watchdog_BOOT_TIME_SEC));

    /* Update timer registers */
    Hwi_Params_init(&hwiParams);
    hwiParams.priority = 1;
    hwiParams.eventId = Watchdog_module->device[0].eventId;
    hwiParams.maskSetting = Hwi_MaskingOption_LOWER;
    key = Hwi_disable();
    Hwi_create(Watchdog_module->device[0].intNum, (Hwi_FuncPtr) timerFxn,
                                                            &hwiParams, NULL);
    Hwi_enableInterrupt(Watchdog_module->device[0].intNum);
    Hwi_restore(key);

    /* Enable timer */
    timer->tclr |= 1;
    System_printf("Watchdog enabled: TimerBase = 0x%x Freq = %d\n",
                                            timer, tFreq.lo);

    return;
}

/*
 *  ======== Watchdog_idleBegin ========
 */
Void Watchdog_idleBegin(Void)
{
    Watchdog_kick();
}

/*
 *  ======== Watchdog_swiPrehook ========
 */
Void Watchdog_swiPrehook(Swi_Handle swi)
{
    Watchdog_kick();
}

/*
 *  ======== Watchdog_taskSwitch ========
 *  Refresh/restart watchdog timer whenever task switch scheduling happens
 */
Void Watchdog_taskSwitch(Task_Handle p, Task_Handle n)
{
    Watchdog_kick();
}

/*
 *  ======== Watchdog_isException ========
 */
Bool Watchdog_isException(UInt excNum)
{
    if (excNum == Watchdog_module->device[0].intNum) {
        return TRUE;
    }

    return FALSE;
}

/*
 *  ======== Watchdog_stop ========
 */
Void Watchdog_stop(Void)
{
    Watchdog_TimerRegs *timer = Watchdog_module->device[0].baseAddr;

    if (timer) {
        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TCLR);
        timer->tclr &= ~1;
    }
}

/*
 *  ======== Watchdog_start ========
 */
Void Watchdog_start(Void)
{
    Watchdog_TimerRegs *timer = Watchdog_module->device[0].baseAddr;

    if (timer) {
        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TCLR);
        timer->tclr |= 1;

        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TGRR);
        timer->ttgr = 0;
    }
}

/*
 *  ======== Watchdog_kick ========
 *  Refresh/restart the watchdog timer
 */
Void Watchdog_kick(Void)
{
    Watchdog_TimerRegs *timer = Watchdog_module->device[0].baseAddr;

    if (timer) {
        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TGRR);
        timer->ttgr = 0;
    }
}
