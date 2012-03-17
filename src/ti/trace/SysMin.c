/*
 *  Copyright (c) 2011 Texas Instruments. All rights reserved.
 *  This program and the accompanying materials are made available under the
 *  terms of the Eclipse Public License v1.0 and Eclipse Distribution License
 *  v. 1.0 which accompanies this distribution. The Eclipse Public License is
 *  available at http://www.eclipse.org/legal/epl-v10.html and the Eclipse
 *  Distribution License is available at
 *  http://www.eclipse.org/org/documents/edl-v10.php.
 *
 *  Contributors:
 *      Texas Instruments - initial implementation
 * */
/*
 *  ======== SysMin.c ========
 */

#include <xdc/runtime/Startup.h>
#include <xdc/runtime/Gate.h>

#ifdef SMP
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Core.h>
#include <ti/sysbios/hal/Hwi.h>
#endif
#include <ti/sysbios/knl/Clock.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "package/internal/SysMin.xdc.h"

/*
 *  ======== SysMin_Module_startup ========
 */
Int SysMin_Module_startup(Int phase)
{
    if (SysMin_bufSize != 0) {
        memset(module->outbuf, 0, SysMin_bufSize);
    }

#ifndef SMP
    /* Initialize to TRUE so the first line get the timestamp */
    module->getTime = TRUE;
#endif

    return (Startup_DONE);
}

/*
 *  ======== SysMin_abort ========
 */
Void SysMin_abort(String str)
{
    Char ch;

    if (SysMin_bufSize != 0) {
        if (str != NULL) {
            while ((ch = *str++) != '\0') {
                SysMin_putch(ch);
            }
        }

        /* Only flush if configured to do so */
        if (SysMin_flushAtExit) {
            SysMin_flush();
        }
    }
}

/*
 *  ======== SysMin_exit ========
 */
Void SysMin_exit(Int stat)
{
    if ((SysMin_flushAtExit) && (SysMin_bufSize != 0)) {
        SysMin_flush();
    }
}

/*
 *  ======== SysMin_output ========
 *  Common output function to write a character
 *  into the circular buffer
 */
static inline Void SysMin_output(Char ch)
{
    module->outbuf[module->outidx++] = ch;
    /* Last 8 bytes are used for writeIdx/readIdx fields */
    if (module->outidx == SysMin_bufSize - 8) {
        module->outidx = 0;
    }
}

/*
 *  ======== SysMin_putch ========
 *  Custom implementation for using circular
 *  buffer without using flush
 */
Void SysMin_putch(Char ch)
{
    IArg        key;
    UInt        i;
#ifndef SMP
    static UInt coreId = 0;
#else
    UInt        coreId;
#endif
    UInt        lineIdx;
    Char        *lineBuf;
    Int         index;
    UInt64      uSec;
    static Bool configure = FALSE;
    static UInt startIdx;
    static UInt endIdx;
    static UInt timeStampSecCharLen;
    const  UInt minSecCharLen   = 4;  /* for 1 us tick period */
    const  UInt maxuSecCharLen  = 6; /* for 1 us tick period */
    /* Max characters for seconds would be 10 assuming 1 sec tick period,
     * so decimal point index will be 11, and maximum time stamp buffer
     * length would be 18 accounting maxuSecCharLen and a trailing NULL */
    const  UInt decPtIdx        = 11;
    const  UInt timeStampBufLen = 18;
    const  UInt leftSpaceIdx    = 10;
    Char        timeStamp[18]   = {"                 \0"};

    /* Configure the trace timestamp format */
    if (!configure) {
        Int i = 0, mod = 10;

        /* Find number of characters needes for seconds and sub-seconds,
         * tick periods are specified in microseconds */
        for (; i < maxuSecCharLen; i++) {
            if (Clock_tickPeriod % mod) {
                break;
            }
            mod = mod * 10;
        }
        timeStampSecCharLen = minSecCharLen + i;
        startIdx = decPtIdx - timeStampSecCharLen;
        endIdx = timeStampBufLen - (i + 1); /* Account for null character too */
        configure = TRUE;
    }

    if (SysMin_bufSize != 0) {

#ifndef SMP
        key = Gate_enterSystem();
#else
        /* Disable only local interrupts to place chars in local line buffer */
        key = (IArg)Hwi_disableCoreInts();
        coreId = Core_getCoreId();
#endif

        lineIdx = module->lineBuffers[coreId].lineidx;
        lineBuf = module->lineBuffers[coreId].linebuf;
        lineBuf[lineIdx++] = ch;
        module->lineBuffers[coreId].lineidx = lineIdx;

#ifdef SMP
        /* restore local interrupts */
        Hwi_restoreCoreInts((UInt)key);

        /* Copy line buffer to shared output buffer at EOL or when filled up */
        if ((ch == '\n') || (lineIdx >= SysMin_LINEBUFSIZE)) {
            key = Gate_enterSystem();

            /* Tag core number */
            SysMin_output('[');
            SysMin_output(0x30 + coreId);
            SysMin_output(']');
#else
        if (module->getTime == TRUE) {
#endif
            uSec  = Clock_getTicks() * Clock_tickPeriod;
            SysMin_output('[');
            if (uSec) {
                sprintf(timeStamp, "%17llu\0", uSec);
            }
            for (index = startIdx; index < endIdx; index++) {
                if (index == decPtIdx) {
                    SysMin_output('.');
                }
                if (timeStamp[index] == ' ' && index >= leftSpaceIdx) {
                    SysMin_output('0');
                }
                else {
                    SysMin_output(timeStamp[index]);
                }
            }
            SysMin_output(']');
            SysMin_output(' ');
#ifdef SMP
            for (i = 0; i < lineIdx; i++) {
                SysMin_output(lineBuf[i]);
            }
            module->writeidx[0] =  module->outidx;
            module->lineBuffers[coreId].lineidx = 0;

            Gate_leaveSystem(key);
        }
#else
            module->getTime = FALSE;
        }

        /* Copy line buffer to shared output buffer at EOL or when filled up */
        if ((ch == '\n') || (lineIdx >= SysMin_LINEBUFSIZE)) {
            for (i = 0; i < lineIdx; i++) {
                SysMin_output(lineBuf[i]);
            }
            module->lineBuffers[coreId].lineidx = 0;
            module->getTime = TRUE;
            module->writeidx[0] = module->outidx;
        }

        Gate_leaveSystem(key);
#endif

    }
}

/*
 *  ======== SysMin_ready ========
 */
Bool SysMin_ready()
{
    return (SysMin_bufSize != 0);
}

/*
 *  ======== SysMin_flush ========
 *  Called during SysMin_exit, System_exit or System_flush.
 */
Void SysMin_flush()
{
/* Using custom circular buffer implementation without resetting write ptr */
#if 0
    IArg key;

    key = Gate_enterSystem();

    /*
     *  If a wrap occured, we need to flush the "end" of the internal buffer
     *  first to maintain fifo character output order.
     */
    if (module->wrapped == TRUE) {
        SysMin_outputFunc(module->outbuf + module->outidx,
                          SysMin_bufSize - module->outidx);
    }

    SysMin_outputFunc(module->outbuf, module->outidx);
    module->outidx = 0;
    module->wrapped = FALSE;

    Gate_leaveSystem(key);
#endif
}
