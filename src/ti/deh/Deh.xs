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

var Deh = null;
var MultiProc = null;
var Hwi = null;

/*
 *  ======== module$use ========
 */
function module$use()
{
    var Swi = null;
    var Task = null;
    var Exception = null;
    var Settings = xdc.module("ti.sysbios.family.Settings");

    Deh = this;

    xdc.useModule('xdc.runtime.System');

    Hwi = xdc.useModule(Settings.getDefaultHwiDelegate());
    MultiProc = xdc.module('ti.sdo.utils.MultiProc');

    if (Program.build.target.name.match(/C64T/)) {
        Exception = xdc.useModule("ti.sysbios.family.c64p.Exception");
        Exception.exceptionHook = Deh.excHandlerDsp;
    }
    else {
        xdc.useModule('ti.trace.StackDbg');
    }

    Swi = xdc.useModule('ti.sysbios.knl.Swi');
    Task = xdc.useModule('ti.sysbios.knl.Task');

    xdc.useModule('ti.deh.Watchdog');
}

/*
 *  ======== module$static$init ========
 */
function module$static$init(mod, params)
{
    var segname = Program.sectMap[".errorbuf"];
    var segment = Program.cpu.memoryMap[segname];

    if (params.bufSize > segment.len) {
        this.$logError("bufSize 0x" + Number(params.bufSize).toString(16) +
                       " configured is too large, maximum bufSize allowed is " +
                       "0x" + Number(segment.len).toString(16) + ". Decrement" +
                       " the bufSize appropriately.", this);
    }

    mod.outbuf.length = params.bufSize;
    if (params.bufSize != 0) {
        var Memory = xdc.module('xdc.runtime.Memory');
        Memory.staticPlace(mod.outbuf, 0x1000, params.sectionName);
    }

    if (Program.build.target.name.match(/C64T/)) {
        mod.isrStackSize = null;
        mod.isrStackBase = null;
    }
    else {
        mod.isrStackSize = Program.stack;
        mod.isrStackBase = $externPtr('__TI_STACK_BASE');
        Hwi.excHandlerFunc = Deh.excHandler;
    }
}
