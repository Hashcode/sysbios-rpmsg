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
 *  ======== InterruptIpu.c ========
 *  IPU Interrupt Manger
 */

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/family/arm/ducati/Core.h>

#include <ti/ipc/MultiProc.h>

#include <ti/ipc/rpmsg/InterruptIpu.h>

/* Register access method. */
#define REG16(A)   (*(volatile UInt16 *) (A))
#define REG32(A)   (*(volatile UInt32 *) (A))

#define HOSTINT                 26
#define DSPINT                  55
#define M3INT_MBX               50
#define M3INT                   19

/* Assigned mailboxes */
#define SYSM3_MBX               0
#define HOST_MBX_0              1
#define HOST_MBX_1              2
#define APPM3_MBX               3
#define DSP_MBX                 4

#define MAILBOX_BASEADDR        (0xAA0F4000)

#define MAILBOX_MESSAGE(M)      (MAILBOX_BASEADDR + 0x040 + (0x4 * M))
#define MAILBOX_FIFOSTATUS(M)   (MAILBOX_BASEADDR + 0x080 + (0x4 * M))
#define MAILBOX_STATUS(M)       (MAILBOX_BASEADDR + 0x0C0 + (0x4 * M))
#define MAILBOX_REG_VAL(M)      (0x1 << (2 * M))

#define MAILBOX_IRQSTATUS_CLR_M3    (MAILBOX_BASEADDR + 0x124)
#define MAILBOX_IRQENABLE_SET_M3    (MAILBOX_BASEADDR + 0x128)
#define MAILBOX_IRQENABLE_CLR_M3    (MAILBOX_BASEADDR + 0x12C)

Hwi_FuncPtr userFxn = NULL;

Void InterruptIpu_isr(UArg arg);

/*
 *************************************************************************
 *                      Module functions
 *************************************************************************
 */

/*!
 *  ======== InterruptIpu_intEnable ========
 *  Enable remote processor interrupt
 */
Void InterruptIpu_intEnable()
{
    /*
     *  If the remote processor communicates via mailboxes, we should enable
     *  the Mailbox IRQ instead of enabling the Hwi because multiple mailboxes
     *  share the same Hwi
     */
    if (Core_getId() == 0) {
        REG32(MAILBOX_IRQENABLE_SET_M3) = MAILBOX_REG_VAL(SYSM3_MBX);
    }
    else {
        REG32(MAILBOX_IRQENABLE_SET_M3) = MAILBOX_REG_VAL(APPM3_MBX);
    }
    Hwi_enableInterrupt(M3INT);
}

/*!
 *  ======== InterruptIpu_intDisable ========
 *  Disables remote processor interrupt
 */
Void InterruptIpu_intDisable()
{
    /*
     *  If the remote processor communicates via mailboxes, we should disable
     *  the Mailbox IRQ instead of disabling the Hwi because multiple mailboxes
     *  share the same Hwi
     */
    if (Core_getId() == 0) {
        REG32(MAILBOX_IRQENABLE_CLR_M3) = MAILBOX_REG_VAL(SYSM3_MBX);
    }
    else {
        REG32(MAILBOX_IRQENABLE_CLR_M3) = MAILBOX_REG_VAL(APPM3_MBX);
    }
    Hwi_disableInterrupt(M3INT);
}

/*!
 *  ======== InterruptIpu_intRegister ========
 */
Void InterruptIpu_intRegister(Hwi_FuncPtr fxn)
{
    Hwi_Params  hwiAttrs;
    UInt        key;

    /* Disable global interrupts */
    key = Hwi_disable();

    userFxn = fxn;
    Hwi_Params_init(&hwiAttrs);
    hwiAttrs.maskSetting = Hwi_MaskingOption_LOWER;

    Hwi_create(M3INT,
               (Hwi_FuncPtr)InterruptIpu_isr,
               &hwiAttrs,
               NULL);

    Hwi_create(M3INT_MBX,
               (Hwi_FuncPtr)InterruptIpu_isr,
               &hwiAttrs,
               NULL);

    /* InterruptIpu_intEnable won't enable the Hwi */
    Hwi_enableInterrupt(M3INT_MBX);
    /* InterruptIpu_intEnable won't enable the Hwi */
    Hwi_enableInterrupt(M3INT);

    /* Enable the mailbox interrupt to the M3 core */
    InterruptIpu_intEnable();

    /* Restore global interrupts */
    Hwi_restore(key);
}

/*!
 *  ======== InterruptIpu_intSend ========
 *  Send interrupt to the remote processor
 */
Void InterruptIpu_intSend(UInt16 remoteProcId, UArg arg)
{
    Log_print2(Diags_USER1,
        "InterruptIpu_intSend: Sending interrupt with payload 0x%x to proc #%d",
        (IArg)arg, (IArg)remoteProcId);
    if (remoteProcId == MultiProc_getId("CORE0")) {
        while(REG32(MAILBOX_FIFOSTATUS(SYSM3_MBX)) == 1);
        REG32(MAILBOX_MESSAGE(SYSM3_MBX)) = arg;
    }
    else if (remoteProcId == MultiProc_getId("CORE1")) {
        while(REG32(MAILBOX_FIFOSTATUS(APPM3_MBX)) == 1);
        REG32(MAILBOX_MESSAGE(APPM3_MBX)) = arg;
    }
    else if (remoteProcId == MultiProc_getId("DSP")) {
        while(REG32(MAILBOX_FIFOSTATUS(DSP_MBX)) == 1);
        REG32(MAILBOX_MESSAGE(DSP_MBX)) = arg;
    }
    else { /* HOSTINT */
        if (Core_getId() == 0) {
            while(REG32(MAILBOX_FIFOSTATUS(HOST_MBX_0)) == 1);
            REG32(MAILBOX_MESSAGE(HOST_MBX_0)) = arg;
        }
        else {
            while(REG32(MAILBOX_FIFOSTATUS(HOST_MBX_1)) == 1);
            REG32(MAILBOX_MESSAGE(HOST_MBX_1)) = arg;
        }
    }
}

/*!
 *  ======== InterruptIpu_intClear ========
 *  Clear interrupt and return payload
 */
UInt InterruptIpu_intClear()
{
    UInt arg = INVALIDPAYLOAD;

    /* First check whether incoming mailbox has a message */
    if (Core_getId() == 0) {
        if (REG32(MAILBOX_STATUS(SYSM3_MBX)) == 0) {
            return (arg);
        }
        else {
            /* If there is a message, return the argument to the caller */
            arg = REG32(MAILBOX_MESSAGE(SYSM3_MBX));
            REG32(MAILBOX_IRQSTATUS_CLR_M3) = MAILBOX_REG_VAL(SYSM3_MBX);
        }
    }
    else {
        if (REG32(MAILBOX_STATUS(APPM3_MBX)) == 0) {
            return (arg);
        }
        else {
            /* If there is a message, return the argument to the caller */
            arg = REG32(MAILBOX_MESSAGE(APPM3_MBX));
            REG32(MAILBOX_IRQSTATUS_CLR_M3) = MAILBOX_REG_VAL(APPM3_MBX);
        }
    }

    return (arg);
}

/*!
 *  ======== InterruptIpu_isr ========
 *  Calls the function supplied by the user in intRegister
 */
Void InterruptIpu_isr(UArg arg)
{
    UArg payload;

    payload = InterruptIpu_intClear();
    if (payload != INVALIDPAYLOAD) {
        Log_print1(Diags_USER1,
            "InterruptIpu_isr: Interrupt received, payload = 0x%x\n",
            (IArg)payload);
        userFxn(payload);
    }
    else {
//        Log_print0(Diags_USER1, "InterruptIpu_isr: Interrupt ignored\n");
    }
}
