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
 *  ======== HdmiWa.xdc ================
 *
 */

package ti.hdmiwa

/*!
 *  ======== HdmiWa ========
 *  HDMI ACR Packet Reduction Solution for OMAP4430 (< ES 2.3)
 *
 *  Due to a HW bug, the HDMI-ACR pulses are generated at a higher frequency
 *  than expected. The idea of this WA is to let pass one and only one ACR
 *  pulse in a fix period window (usually 1ms). The frequency of the ACR pulse
 *  can not be controlled, but the generation of the pulse can be disabled or
 *  enabled by toggling the ACR_CTRL register.
 *
 *  Example for 1ms window:
 *
 *      |--- F ---|
 *       _         _         _         _         _         _
 *  ____/-\_______/+\_______/-\_______/-\_______/+\_______/-\         ACR_PULSES
 *               ________                      ________
 *  _ _ _ _ ____/ ACR ON \  _ _ _ _ _ _ _ ____/ ACR ON \       TOGGLING ACR_CTRL
 *
 *          |-H-|--- F ---|
 *    ______                        ______
 *  _/ AISR \______________________/ AISR \___________________   AUTO-RELOAD ISR
 *
 *  | ---------- 1 ms- ------------|
 *
 *  where:
 *   AISR = Auto Reload ISR (every 1ms)
 *   H = Half CTS Interval
 *   F = Full CTS Interval
 *   + = ACR PULSE PASSED
 *   - = ACR PULSE FILTERED
 *
 */

@ModuleStartup      /* generate a call to Task_Module_startup at startup */
module HdmiWa {

    /*! timer addr default to GPT3 */
    config UInt timerAddr = 0x48034000;

    /*! HDMI Configuration Register Base Address */
    config UInt hdmiBaseAddr = 0x48046000;

    /*! timer registers */
    struct TimerRegs {
        UInt tidr;
        UInt empty[3];
        UInt tiocpCfg;
        UInt empty1[3];
        UInt irqEoi;
        UInt irqStatRaw;
        UInt tisr;      /* irqstat   */
        UInt tier;      /* irqen_set */
        UInt irqEnClr;
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
    };

    /*!
     *  @def    HdmiWa_S_SUCCESS
     *  @brief  Operation is successful.
     */
    const Int S_SUCCESS  = 0;

    /*!
     *  @def    HdmiWa_E_TIMER_FAIL
     *  @brief  Timer Fail to start
     */
    const Int E_TIMER_FAIL = -1;

internal:
    Void tick(UArg arg);

    struct PayloadData {
        UInt32 ctsInterval;
        UInt32 acrRate;
        UInt32 sysClockFreq;
        UInt32 trigger;
    };

    struct Module_State {
        Bool        waEnable;
        Bool        matchTmrEnable;
        Bool        acrOff;
        UInt        gptTcrrHalfCts;
        UInt32      gptTcrrFullCts;
        UInt32      ctsIntervalDelta;
        TimerRegs*  pTmr;
    };
};
