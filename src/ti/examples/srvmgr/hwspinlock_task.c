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
 *  ======== hwspinlock_task.c ========
 *
 *  Test for the hw spinlocks
 */

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ti/gates/hwspinlock/HwSpinlock.h>
#include <ti/sysbios/hal/Hwi.h>

String HwSpinlockStatesName[] = {
    "FREE",
    "TAKEN",
    "TIMEOUT",
};

Void testHwSpinlock(HwSpinlock_Handle handle, HwSpinlock_PreemptGate lp,
                    Int timeout)
{
    Int id, status;
    UInt t = 0xFFFF;
    HwSpinlock_Key key;

    if (handle) {
        id = HwSpinlock_getId(handle);
        System_printf(" HwSpinlock id(%d) tsk(0x%x) pri(%d)\n",
                        id, Task_self(), Task_getPri(Task_self()));
    }
    else {
        System_printf("Error Handle is NULL tsk(0x%x) pri(%d)\n",
                      Task_self(), Task_getPri(Task_self()));
        return;
     }

    System_printf("Acquiring HwSpinlock(%d) ...tsk(0x%x) pri(%d) \n",
                   id, Task_self(), Task_getPri(Task_self()));
    status = HwSpinlock_enter(handle, lp, timeout, &key);
    System_printf("HwSpinlock(%d) state(%s) tsk(0x%x) pri(%d)\n", id,
                   HwSpinlockStatesName[HwSpinlock_getState(handle)],
                   Task_self(), Task_getPri(Task_self()));

    /* Keep the HwSpinlock during 2 secs*/
    if (status) {
        System_printf("TIMEOUT: HwSpinlock(%d) tsk(0x%x) pri(%d)\n",
                       id, Task_self(), Task_getPri(Task_self()));
        return;
    } else {
        if (lp == HwSpinlock_PreemptGate_NONE) {
            Task_sleep(1);
        }
        else {
            while(t--);
        }
    }

    System_printf("Releasing HwSpinlock(%d)...tsk(0x%x) pri(%d)\n",
                   id, Task_self(), Task_getPri(Task_self()));
    HwSpinlock_leave(handle, &key);
    System_printf("HwSpinlock(%d) state(%s) tsk(0x%x) pri(%d)\n", id,
               HwSpinlockStatesName[HwSpinlock_getState(handle)],
               Task_self(), Task_getPri(Task_self()));
}

Void deleteHwSpinlock(HwSpinlock_Handle handle)
{
    Int status, id;

    id = HwSpinlock_getId(handle);

    System_printf("Deleting HwSpinlock tsk(0x%x) pri(%d)\n",
                   Task_self(), Task_getPri(Task_self()));
    status = HwSpinlock_delete(handle);
    if (status) {
        System_printf("Del Error(%d) spinlock(%d) state(%s) tsk(0x%x) pri(%d)",
                      status, id,
                      HwSpinlockStatesName[HwSpinlock_getState(handle)],
                      Task_self(), Task_getPri(Task_self()));
    }
}

Void hwSpinlockTestTaskFxn(UArg arg0, UArg arg1)
{
    HwSpinlock_Params params;
    HwSpinlock_Handle handle1, handle2, handle3, handle4;

    HwSpinlock_Params_init(&params);
    System_printf("Create 4 HwSpinlock instances tsk(0x%x) pri(%d)\n",
                                Task_self(), Task_getPri(Task_self()));
    params.id = 0;
    handle1 = HwSpinlock_create(&params);

    params.id = 1;
    handle2 = HwSpinlock_create(&params);

    params.id = 2;
    handle3 = HwSpinlock_create(&params);

    params.id = 3;
    handle4 = HwSpinlock_create(&params);

    testHwSpinlock(handle1, HwSpinlock_PreemptGate_NONE,
                             HwSpinlock_WAIT_FOREVER);
    testHwSpinlock(handle1, HwSpinlock_PreemptGate_TASK,
                             HwSpinlock_WAIT_FOREVER);
    testHwSpinlock(handle1, HwSpinlock_PreemptGate_NONE,
                             HwSpinlock_WAIT_FOREVER);
    testHwSpinlock(handle2, HwSpinlock_PreemptGate_HWI, 200);
    testHwSpinlock(handle3, HwSpinlock_PreemptGate_TASK, 200);
    testHwSpinlock(handle4, HwSpinlock_PreemptGate_TASK, 200);
    testHwSpinlock(handle1, HwSpinlock_PreemptGate_TASK, 500);
    testHwSpinlock(handle2, HwSpinlock_PreemptGate_HWI, 200);
    testHwSpinlock(handle3, HwSpinlock_PreemptGate_TASK,500);
    testHwSpinlock(handle4, HwSpinlock_PreemptGate_NONE, 3000);

    System_printf("Delete 4 HwSpinlock instances tsk(0x%x) pri(%d)\n",
                                Task_self(), Task_getPri(Task_self()));

    deleteHwSpinlock(handle1);
    deleteHwSpinlock(handle2);
    deleteHwSpinlock(handle3);
    deleteHwSpinlock(handle4);

    System_printf("End of tsk(0x%x) pri(%d) ...\n",
                   Task_self(), Task_getPri(Task_self()));
    return;
}

Void start_hwSpinlock_task()
{
#ifdef CORE0
    Task_Params params;

    Task_Params_init(&params);
    params.instance->name = "HwSpinlockTest1";
    params.priority = 3;
    Task_create(hwSpinlockTestTaskFxn, &params, NULL);

    Task_Params_init(&params);
    params.instance->name = "HwSpinlockTest2";
    params.priority = 4;
    Task_create(hwSpinlockTestTaskFxn, &params, NULL);


    Task_Params_init(&params);
    params.instance->name = "HwSpinlockTest3";
    params.priority = 3;
    Task_create(hwSpinlockTestTaskFxn, &params, NULL);
#endif
}
