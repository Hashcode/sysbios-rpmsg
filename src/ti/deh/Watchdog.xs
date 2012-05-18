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
 *  ======== Watchdog.xs ========
 *
 */

var Watchdog = null;
var Core = null;
var MultiProc = null;
var Task = null;
var Swi  = null;
var taskHook = null;
var swiHook  = null;
var catalogName = null;

if (xdc.om.$name == "cfg" || typeof(genCdoc) != "undefined") {
    var deviceTable = {
        "ti.catalog.c6000": {
            "OMAP4430": {
                wdtimer: [
                   {
                        name: "WdTimerDsp",     /* GpTimer 6 */
                        baseAddr: 0x01d3A000,
                        clkCtrl: 0x4A004570,    /* Virtual address */
                        intNum:  15,
                        eventId: 52,
                    },
                ]
            },
        },
        "ti.catalog.arm.cortexm3": {
            "OMAP4430": {
                wdtimer: [
                    {
                        name: "WdTimerCore0",   /* GpTimer 9 */
                        baseAddr: 0xA803E000,   /* Virtual address */
                        clkCtrl: 0xAA009450,    /* Virtual address */
                        intNum:  55,
                        eventId: -1,
                    },
                    {
                        name: "WdTimerCore1",   /* GpTimer 11 */
                        baseAddr: 0xA8088000,   /* Virtual address */
                        clkCtrl: 0xAA009430,    /* Virtual address */
                        intNum:  56,
                        eventId: -1,
                    },
                ]
            },
        },
    };
}

/*
 *  ======== module$meta$init ========
 */
function module$meta$init()
{
    /* Only process during "cfg" phase */
    if (xdc.om.$name != "cfg") {
        return;
    }

    Watchdog = this;

    catalogName = Program.cpu.catalogName;

    /* Loop through the device table */
    /* TODO: This lookup table can be replaced to use the dmTimer module
     *       later on, and eliminate the need for this device table */
    for (deviceName in deviceTable[catalogName]) {
        if (deviceName == Program.cpu.deviceName) {
            var device = deviceTable[catalogName][deviceName].wdtimer;

            Watchdog.timerSettings.length = device.length;
            for (var i = 0; i < device.length; i++) {
                Watchdog.timerSettings[i].baseAddr = null;
                Watchdog.timerSettings[i].clkCtrl = null;
                Watchdog.timerSettings[i].intNum = device[i].intNum;
                Watchdog.timerSettings[i].eventId = device[i].eventId;
            }

            return;
        }
    }

    /* Falls through on failure */
    print("Watchdog Timer configuration is not found for the specified device ("
            + Program.cpu.deviceName + ").");

    for (device in deviceTable[catalogName]) {
        print("\t" + device);
    }

    throw new Error ("Watchdog Timer unsupported on device!");
}

/*
 *  ======== module$use ========
 */
function module$use()
{
    var Settings = xdc.module("ti.sysbios.family.Settings");
    var Hwi = xdc.useModule(Settings.getDefaultHwiDelegate());

    xdc.useModule('xdc.runtime.System');

    if (Program.platformName.match(/ipu/)) {
        Core = xdc.module("ti.sysbios.hal.Core");
    }
    else {
        MultiProc = xdc.module('ti.sdo.utils.MultiProc');
    }

    Task = xdc.useModule('ti.sysbios.knl.Task');
    taskHook = new Task.HookSet;

    Swi  = xdc.useModule('ti.sysbios.knl.Swi');
    swiHook  = new Swi.HookSet;
}

/*
 *  ======== module$static$init ========
 */
function module$static$init(mod, params)
{
    var device = deviceTable[catalogName][Program.cpu.deviceName].wdtimer;

    swiHook.beginFxn = this.swiPrehook;
    Swi.addHookSet(swiHook);

    taskHook.switchFxn = this.taskSwitch;
    Task.addHookSet(taskHook);

    /* Assign default values if not supplied through configuration */
    for (var i = 0; i < device.length; i++) {
        if (Watchdog.timerSettings[i].baseAddr == null) {
            Watchdog.timerSettings[i].baseAddr = $addr(device[i].baseAddr);
        }
        if (Watchdog.timerSettings[i].clkCtrl == null) {
            Watchdog.timerSettings[i].clkCtrl = $addr(device[i].clkCtrl);
        }
    }

    mod.device.length = 1;
    mod.wdtCores      = 1;
    if (Program.build.target.name.match(/C64T/)) {
        mod.device[0].baseAddr = Watchdog.timerSettings[0].baseAddr;
        mod.device[0].clkCtrl  = Watchdog.timerSettings[0].clkCtrl;
        mod.device[0].intNum   = Watchdog.timerSettings[0].intNum;
        mod.device[0].eventId  = Watchdog.timerSettings[0].eventId;
    }
    else {
        if (Program.platformName.match(/ipu/)) {
            mod.device.length       = Core.numCores;
            mod.wdtCores            = Core.numCores;

            mod.device[0].baseAddr  = Watchdog.timerSettings[0].baseAddr;
            mod.device[0].clkCtrl   = Watchdog.timerSettings[0].clkCtrl;
            mod.device[0].intNum    = Watchdog.timerSettings[0].intNum;
            mod.device[0].eventId   = Watchdog.timerSettings[0].eventId;

            mod.device[1].baseAddr  = Watchdog.timerSettings[1].baseAddr;
            mod.device[1].clkCtrl   = Watchdog.timerSettings[1].clkCtrl;
            mod.device[1].intNum    = Watchdog.timerSettings[1].intNum;
            mod.device[1].eventId   = Watchdog.timerSettings[1].eventId;
        }
        else {
            if (MultiProc.id == MultiProc.getIdMeta("CORE0")) {
                mod.device[0].baseAddr  = Watchdog.timerSettings[0].baseAddr;
                mod.device[0].clkCtrl   = Watchdog.timerSettings[0].clkCtrl;
                mod.device[0].intNum    = Watchdog.timerSettings[0].intNum;
                mod.device[0].eventId   = Watchdog.timerSettings[0].eventId;
            }
            else {
                mod.device[0].baseAddr  = Watchdog.timerSettings[1].baseAddr;
                mod.device[0].clkCtrl   = Watchdog.timerSettings[1].clkCtrl;
                mod.device[0].intNum    = Watchdog.timerSettings[1].intNum;
                mod.device[0].eventId   = Watchdog.timerSettings[1].eventId;
            }
        }
    }
}
