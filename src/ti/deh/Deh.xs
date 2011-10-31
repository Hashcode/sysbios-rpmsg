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
 *  ======== Deh.xs ========
 *
 */

var Deh;
var d_hwi;
var d_task = null;
var d_swi  = null;
var d_task_hook = null;
var d_swi_hook  = null;
var Exception = null;

function module$use()
{
    var Settings = xdc.module("ti.sysbios.family.Settings");

    xdc.useModule('xdc.runtime.System');
    d_hwi = xdc.useModule(Settings.getDefaultHwiDelegate());
    d_task = xdc.useModule('ti.sysbios.knl.Task');
    d_swi  = xdc.useModule('ti.sysbios.knl.Swi');
    d_task_hook = new d_task.HookSet;
    d_swi_hook  = new d_swi.HookSet;

    if (Program.build.target.name.match(/C64T/)) {
        Exception = xdc.useModule("ti.sysbios.family.c64p.Exception");
        Exception.exceptionHook = this.excHandlerDsp;
    }
    else {
        xdc.useModule('ti.trace.StackDbg');
    }

    Deh = this;
}

function module$static$init(obj, params)
{
    var segname = Program.sectMap[".errorbuf"];
    var segment = Program.cpu.memoryMap[segname];

    if (params.bufSize > segment.len) {
        this.$logError("bufSize 0x" + Number(params.bufSize).toString(16) +
                       " configured is too large, maximum bufSize allowed is " +
                       "0x" + Number(segment.len).toString(16) + ". Decrement" +
                       " the bufSize appropriately.", this);
    }

    obj.outbuf.length = params.bufSize;
    if (params.bufSize != 0) {
        var Memory = xdc.module('xdc.runtime.Memory');
        Memory.staticPlace(obj.outbuf, 0x1000, params.sectionName);
    }

    d_swi_hook.beginFxn = Deh.swiPrehook;
    d_swi.addHookSet(d_swi_hook);

    d_task_hook.switchFxn = Deh.taskSwitch;
    d_task.addHookSet(d_task_hook);

    obj.index = 0;
    if (Program.build.target.name.match(/C64T/)) {
        obj.isrStackSize = null;
        obj.isrStackBase = null;
    }
    else {
        obj.isrStackSize = Program.stack;
        obj.isrStackBase = $externPtr('__TI_STACK_BASE');
        d_hwi.excHandlerFunc = Deh.excHandler;
    }
    obj.wdt_base = null;
}
