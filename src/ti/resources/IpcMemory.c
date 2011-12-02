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
 *  ======== IpcMemory.c ========
 *
 */

#include <xdc/runtime/System.h>
#include <xdc/runtime/Startup.h>

#include <ti/resources/rsc_table.h>
#include "package/internal/IpcMemory.xdc.h"


/*
 *  ======== IpcMemory_getEntry ========
 */
IpcMemory_Resource *IpcMemory_getEntry(UInt entry)
{
    if (entry >= *IpcMemory_module->pSize) {
        return (NULL);
    }
    return (IpcMemory_module->pTable + entry);
}

/*
 *************************************************************************
 *                      Module wide functions
 *************************************************************************
 */

/*
 *  ======== IpcMemory_Module_startup ========
 */
Int IpcMemory_Module_startup(Int phase)
{
    IpcMemory_init();
    return (Startup_DONE);
}

/*
 *  ======== IpcMemory_virtToPhys ========
 */
Int IpcMemory_virtToPhys(UInt32 va, UInt32 *pa)
{
    UInt32 i;
    UInt32 offset;
    IpcMemory_Resource *rsc;

    for (i = 0; i < *IpcMemory_module->pSize; i++) {
        rsc = IpcMemory_getEntry(i);
        if (rsc->type == TYPE_DEVMEM ||
            rsc->type == TYPE_CARVEOUT) {
            if (va >= rsc->da_low &&
                va < (rsc->da_low + rsc->size)) {
                offset = va - rsc->da_low;
                *pa = rsc->pa_low + offset;
                return (IpcMemory_S_SUCCESS);
            }
        }
    }
    *pa = NULL;
    return (IpcMemory_E_NOTFOUND);
}

/*
 *  ======== IpcMemory_physToVirt ========
 */
Int IpcMemory_physToVirt(UInt32 pa, UInt32 *va)
{
    UInt32 i;
    UInt32 offset;
    IpcMemory_Resource *rsc;

    for (i = 0; i < *IpcMemory_module->pSize; i++) {
        rsc = IpcMemory_getEntry(i);
        if (rsc->type == TYPE_DEVMEM ||
            rsc->type == TYPE_CARVEOUT) {
            if (pa >= rsc->pa_low &&
                pa < (rsc->pa_low + rsc->size)) {
                offset = pa - rsc->pa_low;
                *va = rsc->da_low + offset;
                return (IpcMemory_S_SUCCESS);
            }
        }
    }
    *va = NULL;
    return (IpcMemory_E_NOTFOUND);
}
