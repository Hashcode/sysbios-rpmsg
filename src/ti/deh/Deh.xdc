/*
 * Copyright (c) 2011, Texas Instruments Incorporated
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
 */

/*
 *  ======== Deh.xdc ========
 *
 */

import ti.sysbios.family.arm.m3.Hwi;
import ti.sysbios.knl.Task;
import ti.sysbios.knl.Swi;

/*!
 *  ======== Deh ========
 *  Device Error Handler
 *
 *  Exceptions that are unrecoverable need to be communicated to
 *  the host processor so that it can print debug information, do
 *  resource cleanup and ultimately reload DSP. The notification
 *  mechanism for sending events to the host processor has been
 *  conolidated in this module.
 */

@Template("./Deh.xdt")
@ModuleStartup
module Deh {

    /*! Exception Registers */
    struct excRegs {
        Ptr  r0;    /* CPU registers */
        Ptr  r1;
        Ptr  r2;
        Ptr  r3;
        Ptr  r4;
        Ptr  r5;
        Ptr  r6;
        Ptr  r7;
        Ptr  r8;
        Ptr  r9;
        Ptr  r10;
        Ptr  r11;
        Ptr  r12;
        Ptr  sp;
        Ptr  lr;
        Ptr  pc;
        Ptr  psr;
        Ptr  ICSR;  /* NVIC registers */
        Ptr  MMFSR;
        Ptr  BFSR;
        Ptr  UFSR;
        Ptr  HFSR;
        Ptr  DFSR;
        Ptr  MMAR;
        Ptr  BFAR;
        Ptr  AFSR;
    }

    /*! Crash dump buffer size */
    config SizeT bufSize = 0x200;

    /*!
     *  ======== sectionName ========
     *  Section where the internal character crashdump buffer is placed
     */
    metaonly config String sectionName = null;

    /*! The test function to plug into the hook exposed by BIOS */
    Void excHandler(UInt *excStack, UInt lr);

    readonly config Int WDT_TIME = (0 - (38400000 * 5));
    readonly config Int WDT_TIME_BOOT = (0 - 38400000 * 10);
    readonly config UInt WDT_CORE0 = 0xA803E000;
    readonly config UInt WDT_CORE1 = 0xA8088000;
    readonly config UInt WDT_CLKCTRL_CORE0 = 0xAA009450;
    readonly config UInt WDT_CLKCTRL_CORE1 = 0xAA009430;
    readonly config UInt WDT_ISR_0 = 55;
    readonly config UInt WDT_ISR_1 = 56;

    /*! timer registers */
    struct timerRegs {
        UInt tidr;
        UInt empty[3];
        UInt tiocpCfg;
        UInt empty1[3];
        UInt irq_eoi;
        UInt irqstat_raw;
        UInt tisr;      /* irqstat   */
        UInt tier;      /* irqen_set */
        UInt irqen_clr;
        UInt twer;      /* irqwaken; */
        UInt tclr;
        Int  tcrr;
        Int  tldr;
        UInt ttgr;
        UInt twps;
        UInt tmar;
        UInt tcar1;
        UInt tsicr;
        UInt tcar2;
    }

    /*! function called when entering idle */
    Void idleBegin();

    /*! function called when leaving idle */
    Void idleEnd();

    /*! watchdog functions */
    Void watchdog_init();
    Void watchdog_stop();
    Void watchdog_start();
    Void watchdog_kick();
    Void taskSwitch(Task.Handle p, Task.Handle n);
    Void swiPrehook(Swi.Handle swi);

internal:
    /*! Functions for decoding exceptions */
    Void excMemFault();
    Void excBusFault();
    Void excUsageFault();
    Void excHardFault();

    struct Module_State {
        Char        outbuf[];      /* the output buffer */
        Int         index;
        SizeT       isrStackSize;  /* stack info for ISR/SWI */
        Ptr         isrStackBase;
        timerRegs   *wdt_base;
    }
}
