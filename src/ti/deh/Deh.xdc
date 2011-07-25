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
 *  ======== Deh.xdc ========
 *
 */

import ti.sysbios.family.arm.m3.Hwi;

/*!
 *  ======== Deh ========
 *  Device Error Handler
 *
 *  Exceptions that are unrecoverable need to be communicated to
 *  the host processor so that it can print debug information, do
 *  resource cleanup and ultimately reload DSP. The notification
 *  mechanism for sending events to the host processor has been
 *  conolidated in this module.
 */

@Template("./Deh.xdt")
@ModuleStartup
module Deh {
    /*! crash-dump buffer size */
    config SizeT bufSize = 2048;

    /*!
     *  ======== sectionName ========
     *  Section where the internal character crashdump buffer is placed
     */
    metaonly config String sectionName = null;

    /*! The test function to plug into the hook exposed by BIOS */
    Void excHandler(UInt *excStack, UInt lr);

internal:
    /*! Functions for decoding exceptions */
    Void excMemFault();
    Void excBusFault();
    Void excUsageFault();
    Void excHardFault();

    struct Module_State {
        Char  outbuf[];  /* the output buffer */
        Int   index;
        SizeT isrStackSize;  /* stack info for ISR/SWI */
        Ptr   isrStackBase;
    }
}
