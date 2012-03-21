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
/** ============================================================================
 *  @file       _IpcResource.h
 *  ============================================================================
 */

#ifndef ti__IpcResource__include
#define ti__IpcResource__include

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 *  Structures & Definitions
 * =============================================================================
 */

#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(*(x)))

/*
 * Linux error codes that can be returned by
 * RM running on A9 side
 */
#define ENOENT                              2
#define ENOMEM                              12
#define EBUSY                               16
#define EINVAL                              22

typedef enum {
    IpcResource_ReqType_CONNECT,
    IpcResource_ReqType_DISCONNECT,
    IpcResource_ReqType_ALLOC,
    IpcResource_ReqType_FREE,
    IpcResource_ReqType_REQ_CONSTRAINTS,
    IpcResource_ReqType_REL_CONSTRAINTS
} IpcResource_ReqType;

typedef struct {
    Char   resName[16];
    Char   resParams[];
} IpcResource_AllocData;

typedef struct {
    UInt32 resHandle;
} IpcResource_FreeData;

typedef struct {
    UInt32                      resHandle;
    IpcResource_ConstraintData  cdata;
} IpcResource_Constraint;

typedef struct {
    UInt32 reqType;
    Char   data[];
} IpcResource_Req;

typedef struct {
    UInt32 resHandle;
    UInt32 base;
    Char   resParams[];
} IpcResource_AckData;

typedef struct {
    UInt32 reqType;
    UInt32 status;
    Char   data[];
} IpcResource_Ack;

typedef struct {
    Char name[16];
} IpcResource_Processor;

typedef struct {
    Char    name[24];
    UInt32  clkRate;
    Char    parentName[24];
    UInt32  parentSrcClkRate;
} _IpcResource_Auxclk;

typedef struct {
    Char    name[16];
    UInt32  minUV;
    UInt32  maxUV;
} _IpcResource_Regulator;

/*!
 *  @brief  IpcResource_Object type
 */
struct IpcResource_Object {
    UInt32              endPoint;
    UInt32              remote;
    UInt                timeout;
    Semaphore_Handle    sem;
    MessageQCopy_Handle msgq;
};

/* Names should match the IpcResource_Type definitions */
static Char * IpcResource_names[] = {
    "omap-gptimer",
    "iva",
    "iva_seq0",
    "iva_seq1",
    "l3bus",
    "omap-iss",
    "omap-fdif",
    "omap-sl2if",
    "omap-auxclk",
    "regulator",
    "gpio",
    "omap-sdma",
    "ipu_c0",
    "dsp_c0",
    "i2c"
};

/* Names to translate auxclk parent IDs */
static Char *auxclkSrcNames[] = {
    "sys_clkin_ck",
    "dpll_core_m3x2_ck",
    "dpll_per_m3x2_ck",
};

/* Names to translate regulator names */
#define NUM_REGULATORS   2
static Char *regulatorNames[] = {
    "cam2pwr",
    "cam2csi",
};

static inline Char *IpcResource_toName(UInt type)
{
    if (type < ARRAY_SIZE(IpcResource_names)) {
        return IpcResource_names[type];
    }
    return NULL;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

#endif /* ti_ipc__IpcResource__include */
