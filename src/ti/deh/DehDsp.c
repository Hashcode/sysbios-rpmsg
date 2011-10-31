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
 *  ======== Deh.c ========
 *
 */

#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/family/c64p/Hwi.h>
#include <ti/sysbios/family/c64p/Exception.h>
#include <ti/sysbios/hal/ammu/AMMU.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/interfaces/ITimer.h>
#include <ti/sysbios/timers/dmtimer/Timer.h>

#include "package/internal/Deh.xdc.h"

/*---- watchdog timer ----*/
#define DEH_WDT_ISR     Deh_WDT_ISR_DSP
#define DEH_WDT_BASE    Deh_WDT_DSP
#define DEH_WDT_CLKCTRL Deh_WDT_CLKCTRL_DSP

#define DEH_WDT_CLKCTRL_IDELST_MASK    (3 << 16)

#define REG32(A)   (*(volatile UInt32 *) (A))

/*
 *  ======== Deh_Module_startup ========
 */
Int Deh_Module_startup(Int phase)
{
    if (AMMU_Module_startupDone() == TRUE) {
        Deh_watchdog_init();
        return Startup_DONE;
    }
    return Startup_NOTDONE;
}

/*
 *  ======== Deh_idleBegin ========
 */
Void Deh_idleBegin(Void)
{
    Deh_timerRegs *base = module->wdt_base;

    if (base) {
        base->tclr |= 1;
        base->tcrr = (UInt32) Deh_WDT_TIME;
    }

}

/*
 *  ======== Deh_swiPrehook ========
 */
Void Deh_swiPrehook(Swi_Handle swi)
{
    Deh_watchdog_kick();
}

/*
 *  ======== Deh_taskSwitch ========
 */
Void Deh_taskSwitch(Task_Handle p, Task_Handle n)
{
    Deh_watchdog_kick();
}

/*
 *  ======== Deh_watchdog_init ========
 */
Void Deh_watchdog_init(Void)
{
    Hwi_Params      hp;
    UInt            key;
    Deh_timerRegs  *base = (Deh_timerRegs *) DEH_WDT_BASE;

    if ((REG32(DEH_WDT_CLKCTRL) & DEH_WDT_CLKCTRL_IDELST_MASK)
                                 == DEH_WDT_CLKCTRL_IDELST_MASK) {
        System_printf("DEH: Watchdog disabled %x\n", REG32(DEH_WDT_CLKCTRL));
        return;
    }

    module->wdt_base = (Deh_timerRegs *) DEH_WDT_BASE;
    base->tisr = ~0;
    base->tier = 2;
    base->twer = 2;
    base->tldr = Deh_WDT_TIME;
    /* Booting can take more time, so set CRR to WDT_TIME_BOOT */
    base->tcrr = Deh_WDT_TIME_BOOT;
    key = Hwi_disable();
    Hwi_Params_init(&hp);
    hp.priority = 1;
    hp.maskSetting = Hwi_MaskingOption_LOWER;
    hp.eventId = 52;
    Hwi_create(DEH_WDT_ISR,
        (Hwi_FuncPtr)ti_sysbios_family_c64p_Exception_dispatch__E,
        &hp, NULL);
    Hwi_enableInterrupt(DEH_WDT_ISR);
    Hwi_restore(key);
    Deh_watchdog_start();
    System_printf("DEH: Watchdog started\n");
}

/*
 *  ======== Deh_watchdog_stop ========
 */
Void Deh_watchdog_stop(Void)
{
    Deh_timerRegs *base = module->wdt_base;

    if (base) {
        base->tclr &= ~1;
    }
}

/*
 *  ======== Deh_watchdog_start ========
 */
Void Deh_watchdog_start(Void)
{
    Deh_timerRegs *base = module->wdt_base;

    if (base) {
        base->tclr |= 1;
    }
}

/*
 *  ======== Deh_watchdog_kick ========
 *  Can be called from isr context
 */
Void Deh_watchdog_kick(Void)
{
    Deh_timerRegs *base = module->wdt_base;

    if (base) {
        Deh_watchdog_start();
        base->ttgr = 0;
    }
}

/*
 *  ======== Deh_excHandlerDsp ========
 *  Read data from exception handler and print it to crash dump buffer
 */
Void Deh_excHandlerDsp()
{
    Exception_Status excStatus;
    Exception_getLastStatus(&excStatus);
    memcpy(module->outbuf, excStatus.excContext, sizeof(*excStatus.excContext));

    System_abort("Terminating execution...\n");
}
