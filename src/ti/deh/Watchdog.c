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

#ifdef SMP
#include <ti/sysbios/hal/Core.h>
#endif

#include <ti/pm/IpcPower.h>

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
 *  ======== initTimer ========
 */
static Void initTimer(Watchdog_TimerRegs *timer, Bool boot)
{
    Timer_Handle    tHandle;
    Types_FreqHz    tFreq;

    /* Get timer frequency */
    tHandle = Timer_Object_get(NULL, 0);
    Timer_getFreq(tHandle, &tFreq);

    timer->tisr = ~0;
    timer->tier = 2;
    timer->twer = 2;
    timer->tldr = (0 - (tFreq.lo * Watchdog_TIME_SEC));

    /* Booting can take more time, so set CRR to WDT_BOOT_TIME */
    if (boot) {
        timer->tcrr = (0 - (tFreq.lo * Watchdog_BOOT_TIME_SEC));
    }
    else {
        timer->tcrr = (0 - (tFreq.lo * Watchdog_TIME_SEC));
    }

    timer->tsicr |= 4; /* enable posted write mode */
}

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
    Int                 i;

    tHandle = Timer_Object_get(NULL, 0);
    Timer_getFreq(tHandle, &tFreq);  /* get timer frequency */

    for (i = 0; i < Watchdog_module->wdtCores; i++) {  /* loop for SMP cores */
        timer = (Watchdog_TimerRegs *) Watchdog_module->device[i].baseAddr;

        /* Check if timer is enabled by host-side */
        if ((REG32(Watchdog_module->device[i].clkCtrl) &
            WATCHDOG_WDT_CLKCTRL_IDLEST_MASK) ==
                                    WATCHDOG_WDT_CLKCTRL_IDLEST_MASK) {
            System_printf("Watchdog disabled: TimerBase = 0x%x ClkCtrl = 0x%x\n",
                                    timer, Watchdog_module->device[i].clkCtrl);
            continue;  /* for next core */
        }

        /* Configure the timer */
        initTimer(timer, TRUE);

        /* Enable interrupt in BIOS */
        Hwi_Params_init(&hwiParams);
        hwiParams.priority = 1;
        hwiParams.eventId = Watchdog_module->device[i].eventId;
        hwiParams.maskSetting = Hwi_MaskingOption_LOWER;
        key = Hwi_disable();
        Hwi_create(Watchdog_module->device[i].intNum, (Hwi_FuncPtr) timerFxn,
                                                            &hwiParams, NULL);
        Hwi_enableInterrupt(Watchdog_module->device[i].intNum);
        Hwi_restore(key);

        /* Enable timer */
        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TCLR);
        timer->tclr |= 1;
        Watchdog_module->status[i] = Watchdog_Mode_ENABLED;

#ifdef SMP
        System_printf("Watchdog enabled: TimerBase = 0x%x SMP-Core = %d "
                                            "Freq = %d\n", timer, i, tFreq.lo);
#else
        System_printf("Watchdog enabled: TimerBase = 0x%x Freq = %d\n",
                                                            timer, tFreq.lo);
#endif
    }

    /* Register callback function */
    if (!IpcPower_registerCallback(IpcPower_Event_RESUME, Watchdog_restore,
                                    NULL)) {
        System_printf("Watchdog_restore registered as a resume callback\n");
    }

    return;
}

/*
 *  ======== Watchdog_idleBegin ========
 */
Void Watchdog_idleBegin(Void)
{
    Int core = 0;

#ifdef SMP
    core = Core_getId();

    if (core != 0) {
        Watchdog_stop(core);
    }
    else
#endif
    {
        Watchdog_kick(core);
    }
}

/*
 *  ======== Watchdog_swiPrehook ========
 */
Void Watchdog_swiPrehook(Swi_Handle swi)
{
    Int core = 0;

#ifdef SMP
    core = Core_getId();

    if (core != 0) {
        Watchdog_start(core);
    }
    else
#endif
    {
        Watchdog_kick(core);
    }
}

/*
 *  ======== Watchdog_taskSwitch ========
 *  Refresh/restart watchdog timer whenever task switch scheduling happens
 */
Void Watchdog_taskSwitch(Task_Handle p, Task_Handle n)
{
    Int core = 0;

#ifdef SMP
    core = Core_getId();

    if (core != 0) {
        Watchdog_start(core);
    }
    else
#endif
    {
        Watchdog_kick(core);
    }
}

/*
 *  ======== Watchdog_isException ========
 */
Bool Watchdog_isException(UInt excNum)
{
    Int i;
    Bool found = FALSE;

    for (i = 0; i < Watchdog_module->wdtCores; i++) {
        if (excNum == Watchdog_module->device[i].intNum) {
            found = TRUE;
            break;
        }
    }

    return found;
}

/*
 *  ======== Watchdog_stop ========
 */
Void Watchdog_stop(Int core)
{
    Watchdog_TimerRegs *timer = Watchdog_module->device[core].baseAddr;

    if ((Watchdog_module->status[core] == Watchdog_Mode_ENABLED) && timer) {
        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TCLR);
        timer->tclr &= ~1;
    }
}

/*
 *  ======== Watchdog_start ========
 */
Void Watchdog_start(Int core)
{
    Watchdog_TimerRegs *timer = Watchdog_module->device[core].baseAddr;

    if ((Watchdog_module->status[core] == Watchdog_Mode_ENABLED) && timer) {
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
Void Watchdog_kick(Int core)
{
    Watchdog_TimerRegs *timer = Watchdog_module->device[core].baseAddr;

    if ((Watchdog_module->status[core] == Watchdog_Mode_ENABLED) && timer) {
        while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TGRR);
        timer->ttgr = 0;
    }
}

/*
 *  ======== Watchdog_restore ========
 *  Refresh/restart the watchdog timer
 */
Void Watchdog_restore(Int event, Ptr data)
{
    Int                 i;
    Watchdog_TimerRegs  *timer;

    for (i = 0; i < Watchdog_module->wdtCores; i++) {  /* loop for SMP cores */
        timer = (Watchdog_TimerRegs *) Watchdog_module->device[i].baseAddr;

        if (Watchdog_module->status[i] == Watchdog_Mode_ENABLED) {
            /* Configure the timer */
            initTimer(timer, FALSE);

            /* Enable timer */
            while (timer->twps & WATCHDOG_TIMER_TWPS_W_PEND_TCLR);
            timer->tclr |= 1;
        }
    }
}
