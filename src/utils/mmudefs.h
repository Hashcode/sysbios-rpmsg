/*
 *  Copyright (c) 2011-2012, Texas Instruments Incorporated
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MMUREGS_H_
#define _MMUREGS_H_

/* MMU Entry Sizes */
#define MMU_SZ_16M              (1 << 24)
#define MMU_SZ_1M               (1 << 20)
#define MMU_SZ_64K              (1 << 16)
#define MMU_SZ_4K               (1 << 12)

#define MMU_L1_SSECTION         0
#define MMU_L1_SECTION          1
#define MMU_L2_LPAGE            2
#define MMU_L2_SPAGE            3

/* MMU PTE Flags */
#define MMU_L1_SECTION_FLAGS    (0x00002)
#define MMU_L1_SSECTION_FLAGS   (0x40002)
#define MMU_L1_PAGE_FLAGS       (0x01)
#define MMU_L2_LPAGE_FLAGS      (0x001)
#define MMU_L2_SPAGE_FLAGS      (0x002)

/* MMU PTE Masks */
#define MMU_L1_SECTION_MASK     (0xFFF00000)
#define MMU_L1_SSECTION_MASK    (0xFF000000)
#define MMU_L1_PAGE_MASK        (0xFFFFFC00)

#define MMU_L2_LPAGE_MASK       (0xFFFF0000)
#define MMU_L2_SPAGE_MASK       (0xFFFFF000)


#endif
