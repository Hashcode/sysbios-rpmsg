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
 */
/*
 *  ======== HdmiWa.c ========
 */

#include <xdc/std.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/timers/dmtimer/Timer.h>

#include <ti/ipc/rpmsg/MessageQCopy.h>
#include <ti/srvmgr/NameMap.h>
#include <ti/ipc/MultiProc.h>

#include "package/internal/HdmiWa.xdc.h"

#define REG32(A)            (*(volatile UInt32 *) (A))
#define HDMIREG(offset)     (HdmiWa_hdmiBaseAddr + offset)

#define GPT_IRQSTATUS_MAT_IT_FLAG             0x1
#define GPT_IRQSTATUS_OVF_IT_FLAG             0x2

#define GPT_IRQWAKEEN_MAT_WUP_ENA             0x1
#define GPT_IRQWAKEEN_OVF_WUP_ENA             0x2

#define GPT_IRQENABLE_CLR_MAT_EN_FLAG         0x1
#define GPT_IRQENABLE_CLR_OVF_EN_FLAG         0x2
#define GPT_IRQENABLE_CLR_MAT_OVF_EN_FLAG     0x3

#define GPT_TCLR_COMPARE_ENABLE               (1 << 0x6)

#define HDMI_ACR_CTRL                         HDMIREG(0x0904)
#define HDMI_FREQ_SVAL                        HDMIREG(0x0908)
#define HDMI_INTR2                            HDMIREG(0x05C8)
#define HDMI_CTS_CHG_INT                      (1 << 3)
#define HDMI_CTS_SEL_SW                       (1 << 0)
#define HDMI_NCTSPKT_EN                       (1 << 1)

#define HDMI_FREQ_SVAL_MCLK_IS_128_FS         0x0
#define HDMI_FREQ_SVAL_MCLK_IS_1024_FS        0x5

/* 10 us in nanoseconds units */
#define TEN_MICROSECONDS                      10000

/* 1ms in nanoseconds units */
#define ONE_MILLISECOND                       1000000

#define TIME_IN_NS_TO_GPT_CYCLES(timeNs, gptFreq) \
                                          (timeNs * (gptFreq / 100000) / 10000)

extern const Timer_Handle SysM3TickTmr;

/*
 *  ======== HdmiWa_taskFxn ========
 */
static Void HdmiWa_taskFxn(UArg arg0, UArg arg1)
{
    MessageQCopy_Handle    handle;
    UInt32                 myEndpoint = 0;
    UInt32                 remoteEndpoint;
    UInt16                 dstProc;
    UInt16                 len;
    UInt32                 temp;
    UInt32                 ctsInterval = 0;
    UInt32                 timerPeriod = 0;
    HdmiWa_PayloadData     payload;

    System_printf("HdmiWa_taskFxn Entered...:\n");
    dstProc = MultiProc_getId("HOST");

    MessageQCopy_init(dstProc);

    /* Create the messageQ for receiving (and get our endpoint for sending). */
    handle = MessageQCopy_create(70, &myEndpoint);

    NameMap_register("rpmsg-hdmiwa", 70);

    while (TRUE) {
        /* Await a character message: */
        MessageQCopy_recv(handle, (Ptr)&payload, &len, &remoteEndpoint,
                MessageQCopy_FOREVER);

        if (!payload.trigger) {
            System_printf("HDMI payload received CTSInterval = %d, "
                          "ACRRate = %d, SysCLK = %d\n", payload.ctsInterval,
                           payload.acrRate, payload.sysClockFreq);
            if (!payload.ctsInterval || !payload.acrRate ||
                !payload.sysClockFreq) {
                System_printf("Stop ACR WA, set auto reload timer to "
                              "default periodicity\n");
                module->waEnable = FALSE;
                timerPeriod = Clock_tickPeriod;
                goto set_timer;
            }

            /* ACR period in microseconds */
            timerPeriod = 1000000 / payload.acrRate;
            ctsInterval = TIME_IN_NS_TO_GPT_CYCLES(payload.ctsInterval,
                                                   payload.sysClockFreq);
            /* Set 1ms interval*/
            temp = ONE_MILLISECOND % payload.ctsInterval;
            if (temp < TEN_MICROSECONDS) {
                module->ctsIntervalDelta = 1;
            }
            else {
                module->ctsIntervalDelta =
                           TIME_IN_NS_TO_GPT_CYCLES(temp, payload.sysClockFreq);
            }
            /* Set ACR in SW mode */
            REG32(HDMI_ACR_CTRL) = HDMI_CTS_SEL_SW;

            REG32(HDMI_INTR2) = HDMI_CTS_CHG_INT;

            REG32(HDMI_FREQ_SVAL) = HDMI_FREQ_SVAL_MCLK_IS_1024_FS;
            while (REG32(HDMI_FREQ_SVAL) != HDMI_FREQ_SVAL_MCLK_IS_1024_FS);

            temp = module->pTmr->tcrr;
            temp += module->ctsIntervalDelta;

            while (module->pTmr->tcrr < temp);

            REG32(HDMI_FREQ_SVAL) = HDMI_FREQ_SVAL_MCLK_IS_128_FS;

            for (temp = 0; (temp < 100000) &&
                ((REG32(HDMI_INTR2) & HDMI_CTS_CHG_INT) != HDMI_CTS_CHG_INT);
                temp++);

            REG32(HDMI_INTR2) = HDMI_CTS_CHG_INT;

            module->gptTcrrHalfCts = ctsInterval / 2;
            module->gptTcrrFullCts = ctsInterval;

set_timer:
            System_printf("Autoreload timer to %d\n", timerPeriod);
            Timer_setPeriodMicroSecs(SysM3TickTmr, timerPeriod);
            Timer_start(SysM3TickTmr);
            MessageQCopy_send(dstProc, remoteEndpoint, myEndpoint,
                                  (Ptr)&payload, len);
        }
        else {
            System_printf("Start HDMI ACRWA\n");
            module->waEnable = TRUE;
        }

    }
}

/*
 *  ======== HdmiWa_Module_startup ========
 */
Int HdmiWa_Module_startup(Int phase)
{
    /* This should be populated in the start/stop callback */
    Task_Params params;

    Task_Params_init(&params);
    params.instance->name = "HdmiWa_taskFxn";
    params.priority = 1;
    Task_create(HdmiWa_taskFxn, &params, NULL);

    return Startup_DONE;
}

/*
 *  ======== HdmiWa_matchTmrIntClr ========
 *  Clear the match interrupt for the timer.
 */
static inline Void HdmiWa_matchTmrIntClr(HdmiWa_TimerRegs* pTmr)
{
    pTmr->irqEnClr = GPT_IRQENABLE_CLR_MAT_EN_FLAG;
    pTmr->tier &= ~GPT_IRQSTATUS_MAT_IT_FLAG;
    pTmr->twer &= ~GPT_IRQWAKEEN_MAT_WUP_ENA;
    pTmr->tmar = 0x0;
    module->matchTmrEnable = FALSE;
}

/*
 *  ======== HdmiWa_matchTmrStart ========
 *  Start the match mode of the timer
 *  Timer will generate an interrupt when tcrr match tmar
 *  HdmiWa_matchTmrIsr should service this interrupt
 */
static inline Int32 HdmiWa_matchTmrStart(HdmiWa_TimerRegs* pTmr, UInt32 load)
{
    /* Return error if the load of match timer
     * is bigger that the periodic timer */
    if (pTmr->tldr  > 0xFFFFFFFF - load) {
        return HdmiWa_E_TIMER_FAIL;
    }

    pTmr->tmar = pTmr->tcrr + load;
    pTmr->tisr |= GPT_IRQSTATUS_MAT_IT_FLAG;
    pTmr->tclr |= GPT_TCLR_COMPARE_ENABLE;
    pTmr->irqEnClr = GPT_IRQENABLE_CLR_MAT_OVF_EN_FLAG;
    pTmr->tier = GPT_IRQSTATUS_MAT_IT_FLAG;
    pTmr->twer = GPT_IRQWAKEEN_MAT_WUP_ENA;
    module->matchTmrEnable = TRUE;

    return HdmiWa_S_SUCCESS;
}

/*
 *  ======== HdmiWa_autoRldTmrIsr ========
 *  Handle the 1ms periodic interrupt needed by HdmiWa
 */
static inline Void HdmiWa_autoRldTmrIsr()
{
    UInt32 volatile temp;
    Int ret;

    /* Return if HdmiWa is not running*/
    if (!module->waEnable) {
        return;
    }

    REG32(HDMI_FREQ_SVAL) = HDMI_FREQ_SVAL_MCLK_IS_1024_FS;
    while (REG32(HDMI_FREQ_SVAL) != HDMI_FREQ_SVAL_MCLK_IS_1024_FS);

    temp = module->pTmr->tcrr;
    temp += module->ctsIntervalDelta;

    while (module->pTmr->tcrr < temp);

    REG32(HDMI_FREQ_SVAL) = HDMI_FREQ_SVAL_MCLK_IS_128_FS;

    for (temp = 0; (temp < 100000) &&
                ((REG32(HDMI_INTR2) & HDMI_CTS_CHG_INT) != HDMI_CTS_CHG_INT);
                temp++);

    REG32(HDMI_INTR2) = HDMI_CTS_CHG_INT;

    ret = HdmiWa_matchTmrStart(module->pTmr, module->gptTcrrHalfCts);
    if (ret) {
        System_printf("Error starting Match timer halfcts\n");
    }
    module->acrOff = TRUE;
}

/*
 *  ======== HdmiWa_matchTmrIsr ========
 *  Handle the oneShot interrupt needed by HdmiWa
 */
static inline Void HdmiWa_matchTmrIsr()
{
    Int ret;

    HdmiWa_matchTmrIntClr(module->pTmr);

    if (module->acrOff) {
        ret = HdmiWa_matchTmrStart(module->pTmr, module->gptTcrrFullCts);
        if (ret) {
            System_printf("Error starting Match timer fullcts \n");
        }
        /* Enable ON time for ACR */
        REG32(HDMI_ACR_CTRL) = HDMI_CTS_SEL_SW | HDMI_NCTSPKT_EN;
        module->acrOff = FALSE;
    }
    else {
        module->pTmr->irqEnClr = GPT_IRQENABLE_CLR_MAT_OVF_EN_FLAG;
        module->pTmr->tier = GPT_IRQSTATUS_OVF_IT_FLAG;
        module->pTmr->twer = GPT_IRQWAKEEN_OVF_WUP_ENA;
        /* Disable ACR */
        REG32(HDMI_ACR_CTRL) = HDMI_CTS_SEL_SW;
        module->acrOff = TRUE;
    }
}

/*
 *  ======== HdmiWa_tick ========
 */
Void HdmiWa_tick(UArg arg)
{
    if (module->matchTmrEnable) {
        /* If match timer is enabled, call only its ISR */
        HdmiWa_matchTmrIsr();
        return;
    }

    /* Provide Tick and periodic HdmiWa ISR */
    Clock_tick();
    HdmiWa_autoRldTmrIsr();
}
