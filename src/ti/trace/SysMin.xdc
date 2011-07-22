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
 *  ======== SysMin.xdc ========
 */

/*!
 *  ======== SysMin ========
 *
 *  Minimal implementation of `{@link ISystemSupport}`.
 *
 *  This implementation provides a fully functional implementation of
 *  all methods specified by `ISystemSupport`.
 *
 *  The module maintains an internal buffer (with a configurable size)
 *  that stores on the "output". When full, the data is over-written.  When
 *  `System_flush()` is called the characters in the internal buffer are
 *  "output" using the user configuratble `{@link #outputFxn}`.
 *
 *  As with all `ISystemSupport` modules, this module is the back-end for the
 *  `{@link System}` module; application code does not directly call these
 *  functions.
 */
@Template("./SysMin.xdt")
@ModuleStartup
module SysMin inherits xdc.runtime.ISystemSupport {

    metaonly struct ModuleView {
        Ptr outBuf;
        UInt outBufIndex;
        Bool getTime;   /* set to true for each new trace */
        Bool wrapped;    /* has the buffer wrapped */
    };

    metaonly struct BufferEntryView {
        String entry;
    }

    /*!
     *  ======== rovViewInfo ========
     *  @_nodoc
     */
    @Facet
    metaonly config xdc.rov.ViewInfo.Instance rovViewInfo =
        xdc.rov.ViewInfo.create({
            viewMap: [
                ['Module',
                    {
                        type: xdc.rov.ViewInfo.MODULE,
                        viewInitFxn: 'viewInitModule',
                        structName: 'ModuleView'
                    }
                ],
                ['OutputBuffer',
                    {
                        type: xdc.rov.ViewInfo.MODULE_DATA,
                        viewInitFxn: 'viewInitOutputBuffer',
                        structName: 'BufferEntryView'
                    }
                ]
            ]
        });

    /*!
     *  ======== bufSize ========
     *  Size (in MAUs) of the output.
     *
     *  An internal buffer of this size is allocated. All output is stored
     *  in this internal buffer.
     *
     *  If 0 is specified for the size, no buffer is created and ALL
     *  tracing is disabled.
     */
    config SizeT bufSize = 0x1000;

    /*!
     *  ======== sectionName ========
     *  Section where the internal character output buffer is placed
     *
     *  The default is to have no explicit placement; i.e., the linker is
     *  free to place it anywhere in the memory map.
     */
    metaonly config String sectionName = null;

    /*!
     *  ======== flushAtExit ========
     *  Flush the internal buffer during `{@link #exit}` or `{@link #abort}`.
     *
     *  If the application's target is a TI target, the internal buffer is
     *  flushed via the `HOSTwrite` function in the TI C Run Time Support
     *  (RTS) library.
     *
     *  If the application's target is not a TI target, the internal buffer
     *  is flushed to `stdout` via `fwrite(..., stdout)`.
     *
     *  Setting this parameter to `false` reduces the footprint of the
     *  application at the expense of not getting output when the application
     *  ends via a `System_exit()`, `System_abort()`, `exit()` or `abort()`.
     */
    config Bool flushAtExit = true;

    /*!
     *  ======== abort ========
     *  Backend for `{@link System#abort()}`
     *
     *  This abort function writes the string to the internal
     *  output buffer and then gives all internal output to the
     *  `{@link #outputFxn}` function if the `{@link #flushAtExit}`
     *  configuration parameter is true.
     *
     *  @param(str)  message to output just prior to aborting
     *
     *      If non-`NULL`, this string should be output just prior to
     *      terminating.
     *
     *  @see ISystemSupport#abort
     */
    override Void abort(String str);

    /*!
     *  ======== exit ========
     *  Backend for `{@link System#exit()}`
     *
     *  This exit function gives all internal output to the
     *  `{@link #outputFxn}` function if the `{@link #flushAtExit}`
     *  configuration parameter is true.
     *
     *  @see ISystemSupport#exit
     */
    override Void exit(Int stat);

    /*!
     *  ======== flush ========
     *  Backend for `{@link System#flush()}`
     *
     *  The `flush` writes the contents of the internal character buffer
     *  via the `{@link #outputFxn}` function.
     *
     *  @a(Warning)
     *  The `{@link System}` gate is used for thread safety during the
     *  entire flush operation, so care must be taken when flushing with
     *  this `ISystemSupport` module.  Depending on the nature of the
     *  `System` gate, your application's interrupt latency
     *  may become a function of the `bufSize` parameter!
     *
     *  @see ISystemSupport#flush
     */
    override Void flush();

    /*!
     *  ======== putch ========
     *  Backend for `{@link System#printf()}` and `{@link System#putch()}`
     *
     *  Places the character into an internal buffer. The `{@link #flush}`
     *  sends the internal buffer to the `{@link #outputFxn}` function.
     *  The internal buffer is also sent to the `SysMin_outputFxn`
     *  function by `{@link #exit}` and `{@link #abort}` if the
     *  `{@link #flushAtExit}` configuration parameter is true.
     *
     *  @see ISystemSupport#putch
     */
    override Void putch(Char ch);

    /*!
     *  ======== ready ========
     *  Test if character output can proceed
     *
     *  This function returns true if the internal buffer is non-zero.
     *
     *  @see ISystemSupport#ready
     */
    override Bool ready();

internal:

    struct Module_State {
        Char outbuf[];  /* the output buffer */
        UInt outidx;    /* index within outbuf to next Char to write */
        Bool getTime;   /* set to true for each new trace */
        Bool wrapped;   /* has the index (outidx) wrapped */
    }
}
