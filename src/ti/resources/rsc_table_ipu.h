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
 *  ======== rsc_table_ipu.h ========
 *
 *  Define the resource table entries for all IPU cores. This will be
 *  incorporated into corresponding base images, and used by the remoteproc
 *  on the host-side to allocated/reserve resources.
 *
 */

#ifndef _RSC_TABLE_IPU_H_
#define _RSC_TABLE_IPU_H_

#include <ti/resources/rsc_types.h>

/* IPU Memory Map */
#define L4_44XX_BASE            0x4a000000

#define L4_PERIPHERAL_L4CFG     (L4_44XX_BASE)
#define IPU_PERIPHERAL_L4CFG    0xAA000000

#define L4_PERIPHERAL_L4PER     0x48000000
#define IPU_PERIPHERAL_L4PER    0xA8000000

#define L4_PERIPHERAL_L4EMU     0x54000000
#define IPU_PERIPHERAL_L4EMU    0xB4000000

#define L3_PERIPHERAL_DMM       0x4E000000
#define IPU_PERIPHERAL_DMM      0xAE000000

#define L3_IVAHD_CONFIG         0x5A000000
#define IPU_IVAHD_CONFIG        0xBA000000

#define L3_IVAHD_SL2            0x5B000000
#define IPU_IVAHD_SL2           0xBB000000

#define L3_TILER_MODE_0_1       0x60000000
#define IPU_TILER_MODE_0_1      0x60000000

#define L3_TILER_MODE_2         0x70000000
#define IPU_TILER_MODE_2        0x70000000

#define L3_TILER_MODE_3         0x78000000
#define IPU_TILER_MODE_3        0x78000000

#define L3_IVAHD_CONFIG         0x5A000000
#define IPU_IVAHD_CONFIG        0xBA000000

#define L3_IVAHD_SL2            0x5B000000
#define IPU_IVAHD_SL2           0xBB000000

#define IPU_MEM_TEXT            0x0
#define IPU_MEM_DATA            0x80000000

#ifdef OMAP5
#define IPU_MEM_IOBUFS          0x90000000
#else
#define IPU_MEM_IOBUFS          0x88000000
#endif

#define IPU_MEM_IPC_DATA        0x9F000000
#define IPU_MEM_IPC_VRING       0xA0000000

#define IPU_MEM_IPC_VRING_SIZE  SZ_1M
#define IPU_MEM_IPC_DATA_SIZE   SZ_1M
#define IPU_MEM_TEXT_SIZE       (SZ_1M * 6)
#ifdef OMAP5
#define IPU_MEM_DATA_SIZE       (SZ_1M * 156)
#else
#define IPU_MEM_DATA_SIZE       (SZ_1M * 96)
#endif
#define IPU_MEM_IOBUFS_SIZE     (SZ_1M * 90)

/*
 * Assign fixed RAM addresses to facilitate a fixed MMU table.
 * PHYS_MEM_IPC_VRING & PHYS_MEM_IPC_DATA MUST be together.
 */
#ifdef OMAP5
#define PHYS_MEM_IPC_VRING      0xAFE00000
#define PHYS_MEM_IPC_DATA       0xAFF00000
#define PHYS_MEM_TEXT           0xB0000000
#define PHYS_MEM_DATA           0xB0700000
#else
#define PHYS_MEM_IPC_VRING      0xB3A00000
#define PHYS_MEM_IPC_DATA       0xB3B00000
#define PHYS_MEM_TEXT           0xB3C00000
#define PHYS_MEM_DATA           0xB4300000
#endif

#define PHYS_MEM_IOBUFS         0xBA300000

#pragma DATA_SECTION(resourcesLen, ".resource_size")
#pragma DATA_SECTION(resources, ".resource_table")

ti_resources_IpcMemory_Resource resources[] = {
    { TYPE_TRACE, 0, 0, 0, 0, 0, 0, "0" },
#ifndef SMP
    { TYPE_TRACE, 1, 0, 0, 0, 0, 0, "1" },
#endif
    { TYPE_ENTRYPOINT, 0, 0, 0, 0, 0, 0, "0" },
#ifndef SMP
    { TYPE_ENTRYPOINT, 1, 0, 0, 0, 0, 0, "1" },
#endif
    { TYPE_CRASHDUMP, 0, 0, 0, 0, 0, 0, "0" },
#ifndef SMP
    { TYPE_CRASHDUMP, 1, 0, 0, 0, 0, 0, "1" },
#endif
    { TYPE_SUSPENDADDR, 0, 0, 0, 0, 0, 0, "0" },
    { TYPE_DEVMEM, IPU_TILER_MODE_0_1, 0, L3_TILER_MODE_0_1, 0, SZ_256M,
       0, "IPU_TILER_MODE_0_1" },
    { TYPE_DEVMEM, IPU_TILER_MODE_2, 0, L3_TILER_MODE_2, 0, SZ_128M,
       0, "IPU_TILER_MODE_2" },
    { TYPE_DEVMEM, IPU_TILER_MODE_3, 0, L3_TILER_MODE_3, 0, SZ_128M,
       0, "IPU_TILER_MODE_3" },
    { TYPE_DEVMEM, IPU_PERIPHERAL_L4CFG, 0, L4_PERIPHERAL_L4CFG, 0, SZ_16M,
       0, "IPU_PERIPHERAL_L4CFG" },
    { TYPE_DEVMEM, IPU_PERIPHERAL_L4PER, 0, L4_PERIPHERAL_L4PER, 0, SZ_16M,
       0,"IPU_PERIPHERAL_L4PER" },
    { TYPE_DEVMEM, IPU_PERIPHERAL_DMM, 0, L3_PERIPHERAL_DMM, 0, SZ_1M,
       0, "IPU_PERIPHERAL_DMM" },
    { TYPE_DEVMEM, IPU_IVAHD_CONFIG, 0, L3_IVAHD_CONFIG, 0, SZ_16M,
       0, "IPU_IVAHD_CONFIG" },
    { TYPE_DEVMEM, IPU_IVAHD_SL2, 0, L3_IVAHD_SL2, 0, SZ_16M,
       0, "IPU_IVAHD_SL2" },
    /* IPU_MEM_IPC_VRING should be first irrespective of static or dynamic
     * carveout. */
    { TYPE_CARVEOUT, IPU_MEM_IPC_VRING, 0, PHYS_MEM_IPC_VRING, 0,
       IPU_MEM_IPC_VRING_SIZE, 0, "IPU_MEM_IPC_VRING" },
    { TYPE_CARVEOUT, IPU_MEM_IPC_DATA, 0, PHYS_MEM_IPC_DATA, 0,
       IPU_MEM_IPC_DATA_SIZE, 0, "IPU_MEM_IPC_DATA" },
    { TYPE_CARVEOUT, IPU_MEM_TEXT, 0, PHYS_MEM_TEXT, 0, IPU_MEM_TEXT_SIZE,
       0, "IPU_MEM_TEXT" },
    { TYPE_CARVEOUT, IPU_MEM_DATA, 0, PHYS_MEM_DATA, 0, IPU_MEM_DATA_SIZE,
       0, "IPU_MEM_DATA" },
    { TYPE_CARVEOUT, IPU_MEM_IOBUFS, 0, PHYS_MEM_IOBUFS, 0, IPU_MEM_IOBUFS_SIZE,
       0, "IPU_MEM_IOBUFS" },
};

UInt32 resourcesLen = sizeof(resources) / sizeof(*resources);

#endif /* _RSC_TABLE_IPU_H_ */
