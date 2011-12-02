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
 *  ======== IpcMemory.xdc ========
 */

/*!
 *  ======== IpcMemory ========
 *
 *  IpcMemory Module
 *
 */

@Template("./IpcMemory.xdt")
@ModuleStartup
module IpcMemory {

    /*!
     *  @def    IpcMemory_loadAddr
     *  @brief  Default load address for the IpcMemory table
     */
    metaonly config UInt loadAddr = 0x3000;

    /*!
     *  @def    IpcMemory_loadSegment
     *  @brief  If loadSegment is defined, loadAddr is overriden with the base
     *          address of the loadSegment
     */
    metaonly config String loadSegment;

    /*!
     *  @def    IpcMemory_S_SUCCESS
     *  @brief  Operation is successful.
     */
    const Int S_SUCCESS  = 0;

    /*!
     *  @def    IpcMemory_E_NOTFOUND
     *  @brief  Element was not found in table
     */
    const Int E_NOTFOUND = -1;

    /*!
     *  @def       IpcMemory_Resource
     *
     *  @brief     A Resource Table record
     */
    struct Resource {
        UInt32 type;
        UInt32 da_low;       /* Device Virtual Address */
        UInt32 da_high;
        UInt32 pa_low;       /* Physical Address */
        UInt32 pa_high;
        UInt32 size;
        UInt32 reserved;
        Char   name[48];
    };

    /*!
     *  @brief      Virtual to Physical address translation function
     *
     *  @sa         IpcMemory_physToVirt
     */
    @DirectCall
    Int virtToPhys(UInt32 da, UInt32 *pa);

    /*!
     *  @brief      Physical to Virtual address translation function
     *
     *  @sa         IpcMemory_virtToPhys
     */
    @DirectCall
    Int physToVirt(UInt32 pa, UInt32 *da);

internal:   /* not for client use */

    /*!
     *  @brief      Use resource and resourceLen so table could be properly
     *              allocated
     *
     */
    Void init();

    /*!
     *  @brief      Return the i-th entry in the resource table
     *
     */
    Resource *getEntry(UInt entry);

    struct Module_State {
        UInt32    *pSize;  /* Number of resources in the Resource Table */
        Resource  *pTable; /* IpcMemory Resource Table pointer */
    };
}
