/*
 * Copyright (c) 2011-2012, Texas Instruments Incorporated
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
 *  ======== resmgr_task.c ========
 *
 *  Test for the rpmsg_resmgr Linux service.
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/resmgr/IpcResource.h>


/* Delay in between different IPC Resource requests */
#define SLEEP_TICKS  1000

/* #define GPT */

void IpcResourceTaskFxn(UArg arg0, UArg arg1)
{
    UInt32 status;
    IpcResource_Handle ipcResHandle;
#ifdef GPT
    IpcResource_Gpt gpt;
    IpcResource_ResHandle gptId;
#endif
    IpcResource_I2c i2c;
    IpcResource_Auxclk auxclk;
    IpcResource_Regulator regulator;
    IpcResource_Gpio gpio;
    IpcResource_Sdma sdma;
    IpcResource_ResHandle ivaId, ivasq0Id, ivasq1Id;
    IpcResource_ConstraintData ivaConstraints;
    IpcResource_ResHandle issId;
    IpcResource_ConstraintData issConstraints;
    IpcResource_ResHandle fdifId;
    IpcResource_ConstraintData fdifConstraints;
    IpcResource_ResHandle sl2ifId;
    IpcResource_ResHandle auxClkId;
    IpcResource_ResHandle i2cId;
    IpcResource_ResHandle regulatorId;
    IpcResource_ResHandle gpioId;
    IpcResource_ResHandle sdmaId;
#if defined(CORE0) || defined(CORE1)
    IpcResource_ResHandle ipuId;
    IpcResource_ConstraintData ipuConstraints;
#endif
#ifdef DSP
    IpcResource_ResHandle dspId;
    IpcResource_ConstraintData dspConstraints;
#endif
    Int loop = 1;

    Task_sleep(SLEEP_TICKS);
    System_printf("\nConnecting to resmgr server...\n");
    ipcResHandle = IpcResource_connect(0);
    if (!ipcResHandle) {
        System_printf("Failed to connect to the resmgr server\n");
        return;
    }
    System_printf("...connected to resmgr server\n");

    while (loop--) {
#if defined(CORE0) || defined(CORE1)
        System_printf("\nRequesting IPU...");
        status = IpcResource_request(ipcResHandle, &ipuId,
                                 IpcResource_TYPE_IPU, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        ipuConstraints.mask = IpcResource_C_LAT | IpcResource_C_BW;
        ipuConstraints.latency = 10;
        ipuConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for IPU: BW = %d LAT = %d...",
                        ipuConstraints.bandwidth, ipuConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, ipuId,
                                                &ipuConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        ipuConstraints.mask = IpcResource_C_FREQ | IpcResource_C_BW;
        ipuConstraints.frequency = 400000000;
        ipuConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for IPU: BW = %d FREQ = %d...",
                        ipuConstraints.bandwidth, ipuConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, ipuId,
                                                    &ipuConstraints);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        ipuConstraints.mask = IpcResource_C_FREQ | IpcResource_C_BW |
                                IpcResource_C_LAT;
        ipuConstraints.latency = 10;
        ipuConstraints.frequency = 400000000;
        ipuConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for IPU: BW = %d LAT = %d "
                      "FREQ = %d...", ipuConstraints.bandwidth,
                        ipuConstraints.latency, ipuConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, ipuId,
                                                &ipuConstraints);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        ipuConstraints.mask = IpcResource_C_BW | IpcResource_C_LAT;
        ipuConstraints.latency = -1;
        ipuConstraints.bandwidth = -1;
        System_printf("Releasing Constraints for IPU: BW = %d LAT = %d ",
                        ipuConstraints.bandwidth, ipuConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, ipuId,
                                                &ipuConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        ipuConstraints.mask = IpcResource_C_FREQ;
        ipuConstraints.frequency = -1;
        System_printf("Releasing Constraints for IPU: FREQ = %d...",
                        ipuConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, ipuId,
                                                &ipuConstraints);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing IPU...");
        status = IpcResource_release(ipcResHandle, ipuId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
#endif

#ifdef DSP
        System_printf("\nRequesting DSP...");
        status = IpcResource_request(ipcResHandle, &dspId,
                                        IpcResource_TYPE_DSP, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        dspConstraints.mask = IpcResource_C_LAT | IpcResource_C_BW;
        dspConstraints.latency = 10;
        dspConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for DSP: BW = %d LAT = %d...",
                        dspConstraints.bandwidth, dspConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, dspId,
                                                &dspConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        dspConstraints.mask = IpcResource_C_FREQ | IpcResource_C_LAT |
                                IpcResource_C_BW;
        dspConstraints.mask = IpcResource_C_FREQ | IpcResource_C_BW;
        dspConstraints.frequency = 465500000;
        dspConstraints.latency = 10;
        dspConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for DSP: BW = %d LAT = %d "
                      "FREQ = %d...", dspConstraints.bandwidth,
                        dspConstraints.latency, dspConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, dspId,
                                                &dspConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        dspConstraints.mask = IpcResource_C_FREQ | IpcResource_C_BW |
                                IpcResource_C_LAT;
        dspConstraints.latency = -1;
        dspConstraints.frequency = -1;
        dspConstraints.bandwidth = -1;
        System_printf("Releasing Constraints for DSP: BW = %d LAT = %d "
                      "FREQ = %d...", dspConstraints.bandwidth,
                        dspConstraints.latency, dspConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, dspId,
                                                &dspConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing DSP...");
        status = IpcResource_release(ipcResHandle, dspId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
#endif

#ifdef GPT
        gpt.id = 6;
        gpt.srcClk = 0;
        System_printf("\nRequesting GPT #%d...", gpt.id);
        status = IpcResource_request(ipcResHandle, &gptId,
                                 IpcResource_TYPE_GPTIMER, &gpt);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing GPT #%d...", gpt.id);
        status = IpcResource_release(ipcResHandle, gptId);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        gpt.id = 9;
        gpt.srcClk = 0;
        System_printf("\nRequesting GPT #%d...", gpt.id);
        status = IpcResource_request(ipcResHandle, &gptId,
                                 IpcResource_TYPE_GPTIMER, &gpt);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing GPT #%d...", gpt.id);
        status = IpcResource_release(ipcResHandle, gptId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
#endif

        auxclk.clkId = 1;
        auxclk.clkRate = 24;
        auxclk.parentSrcClk = 0x2;
        auxclk.parentSrcClkRate = 192;
        System_printf("\nRequesting AuxClk #%d...", auxclk.clkId);
        status = IpcResource_request(ipcResHandle, &auxClkId,
                                 IpcResource_TYPE_AUXCLK, &auxclk);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing AuxClk #%d...", auxclk.clkId);
        status = IpcResource_release(ipcResHandle, auxClkId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

#ifdef OMAP5xxx
        auxclk.clkId = 2;
        auxclk.clkRate = 24;
        auxclk.parentSrcClk = 0x2;
        auxclk.parentSrcClkRate = 192;
        System_printf("Requesting AuxClk #%d...", auxclk.clkId);
        status = IpcResource_request(ipcResHandle, &auxClkId,
                                 IpcResource_TYPE_AUXCLK, &auxclk);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing AuxClk #%d...", auxclk.clkId);
        status = IpcResource_release(ipcResHandle, auxClkId);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
#endif

        auxclk.clkId = 3;
        auxclk.clkRate = 24;
        auxclk.parentSrcClk = 0x2;
        auxclk.parentSrcClkRate = 192;
        System_printf("Requesting AuxClk #%d...", auxclk.clkId);
        status = IpcResource_request(ipcResHandle, &auxClkId,
                                 IpcResource_TYPE_AUXCLK, &auxclk);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing AuxClk #%d...", auxclk.clkId);
        status = IpcResource_release(ipcResHandle, auxClkId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

#if OMAP4xxx
        regulator.regulatorId = 0;
        regulator.minUV = 2000000;
        regulator.maxUV = 2500000;
        System_printf("\nRequesting regulator #%d...", regulator.regulatorId);
        status = IpcResource_request(ipcResHandle, &regulatorId,
                                 IpcResource_TYPE_REGULATOR, &regulator);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing regulator #%d...", regulator.regulatorId);
        status = IpcResource_release(ipcResHandle, regulatorId);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
#endif

        regulator.regulatorId = 1;
#ifdef OMAP5xxx
        regulator.minUV = 2800000;
        regulator.maxUV = 2800000;
#endif
#ifdef OMAP4xxx
        regulator.minUV = 2000000;
        regulator.maxUV = 2500000;
#endif
        System_printf("\nRequesting regulator #%d...", regulator.regulatorId);
        status = IpcResource_request(ipcResHandle, &regulatorId,
                                 IpcResource_TYPE_REGULATOR, &regulator);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        i2c.id = 2;

        System_printf("Requesting I2C #%d...", i2c.id);
        status = IpcResource_request(ipcResHandle, &i2cId,
                                 IpcResource_TYPE_I2C, &i2c);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing I2C #%d...", i2c.id);
        status = IpcResource_release(ipcResHandle, i2cId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing regulator #%d...", regulator.regulatorId);
        status = IpcResource_release(ipcResHandle, regulatorId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

#if OMAP5xxx
        regulator.regulatorId = 2;
        regulator.minUV = 1500000;
        regulator.maxUV = 1500000;
        System_printf("Requesting regulator #%d...", regulator.regulatorId);
        status = IpcResource_request(ipcResHandle, &regulatorId,
                                 IpcResource_TYPE_REGULATOR, &regulator);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing regulator #%d...", regulator.regulatorId);
        status = IpcResource_release(ipcResHandle, regulatorId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
#endif

        gpio.gpioId = 50;
        System_printf("\nRequesting GPIO #%d...", gpio.gpioId);
        status = IpcResource_request(ipcResHandle, &gpioId,
                                 IpcResource_TYPE_GPIO, &gpio);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing GPIO #%d...", gpio.gpioId);
        status = IpcResource_release(ipcResHandle, gpioId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        sdma.numCh = 5;
        System_printf("\nRequesting %d SDMA channels...", sdma.numCh);
        status = IpcResource_request(ipcResHandle, &sdmaId,
                                 IpcResource_TYPE_SDMA, &sdma);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing %d SDMA channels...", sdma.numCh);
        status = IpcResource_release(ipcResHandle, sdmaId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("\nRequesting IVAHD...");
        status = IpcResource_request(ipcResHandle, &ivaId,
                                 IpcResource_TYPE_IVAHD, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Requesting IVASEQ0...");
        status = IpcResource_request(ipcResHandle, &ivasq0Id,
                                 IpcResource_TYPE_IVASEQ0, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
        System_printf("Requesting IVASEQ1...");
        status = IpcResource_request(ipcResHandle, &ivasq1Id,
                                 IpcResource_TYPE_IVASEQ1, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Requesting SL2IF...");
        status = IpcResource_request(ipcResHandle, &sl2ifId,
                                 IpcResource_TYPE_SL2IF, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        ivaConstraints.mask = IpcResource_C_FREQ | IpcResource_C_LAT |
                                IpcResource_C_BW;
        ivaConstraints.frequency = 266100000;
        ivaConstraints.latency = 10;
        ivaConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for IVA: BW = %d LAT = %d "
                      "Frequency = %d...", ivaConstraints.bandwidth,
                        ivaConstraints.latency, ivaConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, ivaId,
                                                &ivaConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }
        Task_sleep(SLEEP_TICKS);

        ivaConstraints.mask = IpcResource_C_FREQ | IpcResource_C_BW |
                                IpcResource_C_LAT;
        ivaConstraints.latency = -1;
        ivaConstraints.frequency = -1;
        ivaConstraints.bandwidth = -1;
        System_printf("Releasing Constraints for IVA: BW = %d LAT = %d "
                      "Frequency = %d...", ivaConstraints.bandwidth,
                        ivaConstraints.latency, ivaConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, ivaId,
                                                &ivaConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing IVASEQ1...");
        status = IpcResource_release(ipcResHandle, ivasq1Id);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing IVASEQ0...");
        status = IpcResource_release(ipcResHandle, ivasq0Id);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing SL2IF...");
        status = IpcResource_release(ipcResHandle, sl2ifId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing IVAHD...");
        status = IpcResource_release(ipcResHandle, ivaId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("\nRequesting ISS...");
        status = IpcResource_request(ipcResHandle, &issId,
                                 IpcResource_TYPE_ISS, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        issConstraints.mask = IpcResource_C_LAT | IpcResource_C_BW;
        issConstraints.latency = 10;
        issConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for ISS: BW = %d LAT = %d...",
                        issConstraints.bandwidth, issConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, issId,
                                                &issConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        issConstraints.mask = IpcResource_C_FREQ | IpcResource_C_LAT |
                                IpcResource_C_BW;
        issConstraints.frequency = 400000000;
        issConstraints.latency = 10;
        issConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for ISS: BW = %d LAT = %d "
                      "FREQ = %d...", issConstraints.bandwidth,
                        issConstraints.latency, issConstraints.frequency);
        status = IpcResource_requestConstraints(ipcResHandle, issId,
                                                &issConstraints);
        if (status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        issConstraints.mask = IpcResource_C_LAT | IpcResource_C_BW;
        issConstraints.latency = -1;
        issConstraints.bandwidth = -1;
        System_printf("Releasing Constraints for ISS: BW = %d LAT = %d ...",
                        issConstraints.bandwidth, issConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, issId,
                                                &issConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing ISS...");
        status = IpcResource_release(ipcResHandle, issId);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("\nRequesting FDIF...");
        status = IpcResource_request(ipcResHandle, &fdifId,
                                 IpcResource_TYPE_FDIF, NULL);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        fdifConstraints.mask = IpcResource_C_BW;
        fdifConstraints.bandwidth = 1000;
        System_printf("Requesting Constraints for FDIF: BW = %d...",
                        fdifConstraints.bandwidth);
        status = IpcResource_requestConstraints(ipcResHandle, fdifId,
                                                    &fdifConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        fdifConstraints.mask = IpcResource_C_LAT;
        fdifConstraints.latency = 1000;
        System_printf("Requesting Constraints for FDIF: LAT = %d...",
                        fdifConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, fdifId,
                                                &fdifConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        fdifConstraints.mask = IpcResource_C_LAT;
        fdifConstraints.latency = -1;
        System_printf("Releasing Constraints for FDIF:: LAT = %d...",
                        fdifConstraints.latency);
        status = IpcResource_requestConstraints(ipcResHandle, fdifId,
                                                &fdifConstraints);
        if (!status) {
            System_printf("Succeeded, status = 0x%x\n", status);
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);

        System_printf("Releasing FDIF...");
        status = IpcResource_release(ipcResHandle, fdifId);
        if (!status) {
            System_printf("Succeeded\n");
        }
        else {
            System_printf("Failed, status = 0x%x\n", status);
        }

        Task_sleep(SLEEP_TICKS);
    }

    System_printf("\nDisconnecting From ResMgr...");
    IpcResource_disconnect(ipcResHandle);
    System_printf("ResMgr Sample Done!!!\n");

    return;
}

void start_resmgr_task()
{
    Task_Params params;

    Task_Params_init(&params);
    params.instance->name = "IpcResource";
    params.priority = 3;
    Task_create(IpcResourceTaskFxn, &params, NULL);
}
