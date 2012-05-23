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
/** ============================================================================
 *  @file       IpcPower.c
 *
 *  @brief      A simple Power Managment which responses to the host commands.
 *
 *  TODO:
 *     - Add suspend/resume notifications
 *  ============================================================================
 */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/Main.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#ifndef SMP
#include <ti/sysbios/family/arm/ducati/Core.h>
#endif
#include <ti/sysbios/family/arm/ducati/omap4430/Power.h>

#ifndef SMP
#include <ti/ipc/MultiProc.h>
#endif
#include <ti/pm/IpcPower.h>
#include "_IpcPower.h"

#define MASTERCORE                      1
#define NO_MASTERCORE                   0
#define CPU_COPY                       -1
#define REG32(A)   (*(volatile UInt32 *) (A))

#define SET_DEEPSLEEP (REG32(M3_SCR_REG) | (1 << DEEPSLEEP_BIT))
#define CLR_DEEPSLEEP (REG32(M3_SCR_REG) & (~(1 << DEEPSLEEP_BIT)))

#ifndef SMP
#pragma DATA_SECTION(IpcPower_hibLocks, ".ipcpower_data")
UInt32 IpcPower_hibLocks[2]; /* One lock for each of the IPU cores */
#else
static UInt32 IpcPower_hibLocks; /* Only one lock in SMP mode */
#endif

static Power_SuspendArgs PowerSuspArgs;
static Swi_Handle suspendResumeSwi;
#ifndef SMP
static UInt16 sysm3ProcId;
static UInt16 appm3ProcId;
#endif
static Int32 refWakeLockCnt;

/* Module ref count: */
static Int curInit = 0;

typedef enum IpcPower_SleepMode {
    IpcPower_SLEEP_MODE_DEEPSLEEP,
    IpcPower_SLEEP_MODE_WAKELOCK,
    IpcPower_SLEEP_MODE_WAKEUNLOCK
} IpcPower_SleepMode;

/* Deep sleep state variable for IpcPower module */
static Bool IpcPower_deepSleep = TRUE;

static inline Void IpcPower_sleepMode(IpcPower_SleepMode opt)
{
    IArg hwiKey;

    /* Set/Restore the DeepSleep bit if no timer already in use */
    hwiKey = Hwi_disable();
    switch (opt) {
        case IpcPower_SLEEP_MODE_WAKEUNLOCK:
            if (refWakeLockCnt) {
                refWakeLockCnt--;
            }
        case IpcPower_SLEEP_MODE_DEEPSLEEP:
            if (!refWakeLockCnt) {
                IpcPower_deepSleep = TRUE;
            }
            break;
        case IpcPower_SLEEP_MODE_WAKELOCK:
            refWakeLockCnt++;
            IpcPower_deepSleep = FALSE;
            break;
    }
    Hwi_restore(hwiKey);
}

static inline Void IpcPower_setWugen()
{
    REG32(WUGEN_MEVT1) |= WUGEN_INT_MASK;
}

/*
 *  ======== IpcPower_suspendSwi ========
 */
#define FXNN "IpcPower_suspendSwi"
static Void IpcPower_suspendSwi(UArg arg0, UArg arg1)
{
#ifndef SMP
    if (MultiProc_self() == sysm3ProcId) {
#endif
        Log_print0(Diags_INFO, FXNN":Core0 Hibernation Swi");
#ifndef SMP
        PowerSuspArgs.pmMasterCore = MASTERCORE;
    }
    else if (MultiProc_self() == appm3ProcId) {
        Log_print0(Diags_INFO, FXNN":Core1 Hibernation Swi");
        PowerSuspArgs.pmMasterCore = NO_MASTERCORE;
    }
#endif
    if (refWakeLockCnt) {
        System_printf("Warning: Wake locks in use\n");
    }
    Power_suspend(&PowerSuspArgs);
#ifndef SMP
    IpcPower_sleepMode(IpcPower_SLEEP_MODE_DEEPSLEEP);
    IpcPower_setWugen();
    Log_print0(Diags_INFO, FXNN":Resume");
#endif
}
#undef FXNN

/* =============================================================================
 *  IpcPower Functions:
 * =============================================================================
 */

/*
 *  ======== IpcPower_init ========
 */
Void IpcPower_init()
{
    Swi_Params swiParams;
#ifndef SMP
    UInt coreIdx = Core_getId();
#endif

    if (curInit++) {
        return;
    }

#ifndef SMP
    IpcPower_hibLocks[coreIdx] = 0;

    sysm3ProcId = MultiProc_getId("CORE0");
    appm3ProcId = MultiProc_getId("CORE1");
#else
    IpcPower_hibLocks = 0;
#endif
    refWakeLockCnt = 0;

    Swi_Params_init(&swiParams);
    swiParams.priority = Swi_numPriorities - 1; /* Max Priority Swi */
    suspendResumeSwi = Swi_create(IpcPower_suspendSwi, &swiParams, NULL);

    /*Power settings for saving/restoring context */
    PowerSuspArgs.rendezvousResume = TRUE;
    PowerSuspArgs.dmaChannel = CPU_COPY;
    PowerSuspArgs.intMask31_0 = 0x0;
#ifndef SMP
    PowerSuspArgs.intMask63_32 = 0x0;
#else
    PowerSuspArgs.intMask63_32 = WUGEN_MAILBOX_BIT << 16;
#endif
    PowerSuspArgs.intMask79_64 = 0x0;
    IpcPower_sleepMode(IpcPower_SLEEP_MODE_DEEPSLEEP);
    IpcPower_setWugen();
}

/*
 *  ======== IpcPower_exit ========
 */
Void IpcPower_exit()
{
    --curInit;
}

/*
 *  ======== IpcPower_suspend ========
 */
Void IpcPower_suspend()
{
    Assert_isTrue((curInit > 0) , NULL);

    Swi_post(suspendResumeSwi);
}

/*
 *  ======== IpcPower_idle ========
 */
Void IpcPower_idle()
{
    REG32(M3_SCR_REG) = IpcPower_deepSleep ? SET_DEEPSLEEP : CLR_DEEPSLEEP;
    asm(" wfi");
}

/*
 *  ======== IpcPower_wakeLock ========
 */
Void IpcPower_wakeLock()
{
    IpcPower_sleepMode(IpcPower_SLEEP_MODE_WAKELOCK);
}

/*
 *  ======== IpcPower_wakeUnlock ========
 */
Void IpcPower_wakeUnlock()
{
    IpcPower_sleepMode(IpcPower_SLEEP_MODE_WAKEUNLOCK);
}

/*
 *  ======== IpcPower_hibernateLock ========
 */
UInt IpcPower_hibernateLock()
{
    IArg hwiKey;
#ifndef SMP
    UInt coreIdx = Core_getId();
#endif

    hwiKey = Hwi_disable();

#ifndef SMP
    IpcPower_hibLocks[coreIdx] += 1;
#else
    IpcPower_hibLocks++;
#endif

    Hwi_restore(hwiKey);

#ifndef SMP
    return (IpcPower_hibLocks[coreIdx]);
#else
    return IpcPower_hibLocks;
#endif
}

/*
 *  ======== IpcPower_hibernateUnlock ========
 */
UInt IpcPower_hibernateUnlock()
{
    IArg hwiKey;
#ifndef SMP
    UInt coreIdx = Core_getId();
#endif

    hwiKey = Hwi_disable();

#ifndef SMP
    if (IpcPower_hibLocks[coreIdx] > 0) {
        IpcPower_hibLocks[coreIdx] -= 1;
    }
#else
    if (IpcPower_hibLocks > 0) {
        IpcPower_hibLocks--;
    }
#endif

    Hwi_restore(hwiKey);

#ifndef SMP
    return (IpcPower_hibLocks[coreIdx]);
#else
    return (IpcPower_hibLocks);
#endif
}

/*
 *  ======== IpcPower_canHibernate ========
 */
Bool IpcPower_canHibernate()
{
#ifndef SMP
    if (IpcPower_hibLocks[0] || IpcPower_hibLocks[1]) {
#else
    if (IpcPower_hibLocks) {
#endif
        return (FALSE);
    }

    return (TRUE);
}
