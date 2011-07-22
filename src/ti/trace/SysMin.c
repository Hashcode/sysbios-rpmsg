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
#include <ti/sysbios/knl/Clock.h>
#include <string.h>
#include <stdlib.h>

#include "package/internal/SysMin.xdc.h"

static const Char ti_trace_SysMin_lookup[] = "0123456789ABCDEF";

/*
 *  ======== SysMin_Module_startup ========
 */
Int SysMin_Module_startup(Int phase)
{
    if (SysMin_bufSize != 0) {
        memset(module->outbuf, 0, SysMin_bufSize);
    }
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
    if (module->outidx == SysMin_bufSize) {
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
    IArg   key;
    UInt32 tick;
    Int shift, index;

    if (SysMin_bufSize != 0) {

        key = Gate_enterSystem();

        if (module->getTime == TRUE) {
            tick = Clock_getTicks();
            SysMin_output('[');

            /* convert 32-bit timestamp -> 8 chars */
            shift = 28; /* 32 bit timestamp - 4 */
            do {
                index = (tick >> shift) & 0x0F; /* 4 bits */
                /* use lookup to convert to char */
                SysMin_output(ti_trace_SysMin_lookup[index]);
                shift -= 4; /* for next 4 bits */
            } while (shift >= 0);

            SysMin_output(']');
            SysMin_output(' ');
            module->getTime = FALSE;
        }

        SysMin_output(ch);
        if (ch == '\n') {
            module->getTime = TRUE;
        }

        Gate_leaveSystem(key);
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
