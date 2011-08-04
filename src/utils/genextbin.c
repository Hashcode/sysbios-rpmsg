/*
 *  Copyright (c) 2011, Texas Instruments Incorporated
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
/*
 *  ======== genextbin.c ========
 *  Version History:
 *  1.00 - Original Version
 *  1.01 - Updated final signature section length
 *  1.02 - Fixed addresses in TOC table
 *  1.03 - Updated extended section locations
 *  1.04 - Added TOC offset at EOF, Version traces
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "rprcfmt.h"
#include "mmudefs.h"

#define RPROC_MAX_MEM_ENTRIES 20

#define RPROC_MAX_FW_SECTIONS 30

#define RPROC_L1_ENTRIES      4096

#define DEBUG_TOC_FILE        "sign_toc.log"
#define DEBUG_MMU_FILE        "mmu_ttb_dump.log"

#define FW_MMU_BASE_PA        0xB4200000
#define FW_SIGNATURE_BASE_PA  (FW_MMU_BASE_PA + 0x80000)

#define VERSION               "1.04"

static int process_image(FILE *iofp, void * data, int size);

typedef struct {
    u64 va;
    u64 pa;
    u32 len;
} va_pa_entry;

typedef struct {
    u32 sect_id;
    u32 offset;
    u32 sect_pa;
    u32 sect_len;
} fw_toc_entry;

va_pa_entry va_pa_table[RPROC_MAX_MEM_ENTRIES];
u32 mmu_l1_table[RPROC_L1_ENTRIES];

/*
 *  ======== main ========
 */
int main(int argc, char * argv[])
{
    FILE * iofp;
    struct stat st;
    char * infile;
    void * data;
    int size;
    int status;

    printf("#######################################\n");
    printf("            GENEXTBIN : %s    \n", VERSION);
    printf("#######################################\n");
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ducati binary>\n", argv[0]);
        exit(1);
    }

    infile = argv[1];
    if ((iofp = fopen(infile, "a+b")) == NULL) {
        fprintf(stderr, "%s: could not open: %s\n", argv[0], infile);
        exit(2);
    }

    fstat(fileno(iofp), &st);
    size = st.st_size;
    printf("Preparing binary: %s of size: 0x%x\n", infile, size);
    data = malloc(size);
    fread(data, 1, size, iofp);

    status = process_image(iofp, data, size);

    free(data);
    fclose(iofp);

    return status;
}

/*
 *  ======== add_mem_entry ========
 */
static int add_mem_entry(struct rproc_fw_resource *rsc)
{
    va_pa_entry *me = va_pa_table;
    int i = 0;
    int ret = 0;

    while (me->va || me->pa || me->len) {
            me += 1;
            i++;
            if (i == RPROC_MAX_MEM_ENTRIES) {
                printf("  No more space in VA->PA Table\n");
                ret = -1;
                break;
            }
    }

    if (!ret) {
        me->va = rsc->da;
        me->pa = rsc->pa;
        me->len = rsc->len;
        printf("  Added entry %d: va 0x%.8llx pa 0x%.8llx len 0x%.8x\n",
            i, me->va, me->pa, me->len);
    }

    return ret;
}

/*
 *  ======== va_to_pa ========
 */
static int va_to_pa(u64 va, u64 *pa)
{
    int i;
    u64 offset;

    for (i = 0; va_pa_table[i].len; i++) {
        const va_pa_entry *me = &va_pa_table[i];

        if (va >= me->va && va < (me->va + me->len)) {
            offset = va - me->va;
            printf("  %s: matched mem entry no. %d\n", __func__, i);
            *pa = me->pa + offset;
            return 0;
        }
    }

    return -1;
}

/*
 *  ======== dump_ttb ========
 */
static void dump_ttb(void)
{
    FILE * fp;
    int i = 0;

    if ((fp = fopen(DEBUG_MMU_FILE, "w+b")) == NULL) {
        fprintf(stderr, "could not open: "DEBUG_MMU_FILE"\n");
        return;
    }

    fprintf(fp, "   VA        PA   \n");
    for (i = 0; i < RPROC_L1_ENTRIES; i++) {
        fprintf(fp, "0x%.8x 0x%.8x\n", i << 20, mmu_l1_table[i]);
    }

    fclose(fp);
}

/*
 *  ======== create_ttb ========
 *  TODO: Extend the MMU table generation to be able to do L2 tables as
 *        well (to provide large page or small page mapping granularity).
 */
static void create_ttb(void)
{
    int i, j;
    va_pa_entry *me = va_pa_table;
    u32 va, pa;
    u32 va_pa;
    u32 size;
    u32 mmu_sizes[4] = {SZ_16M, SZ_1M, SZ_64K, SZ_4K};
    u32 index;

    while (me->pa && me->len) {
        size = me->len;
        va = me->va;
        pa = me->pa;

        printf("Preparing MMU entry for va = 0x%x pa = 0x%x len = 0x%x\n",
                  va, pa, size);
        while (size) {
            va_pa = va | pa;
            for (i = 0; i < 4; i++) {
                if ((size >= mmu_sizes[i]) &&
                    ((va_pa & (mmu_sizes[i] - 1)) == 0)) {
                      break;
                }
            }

            if (i == MMU_L1_SSECTION) {
                printf("    Programming region as 16M entry: 0x%x for 0x%x\n",
                                va, pa);
                index = ((va & MMU_L1_SECTION_MASK) >> 20);
                for (j = 0; j < 16; j++) {
                    mmu_l1_table[index + j] = (pa & MMU_L1_SSECTION_MASK) |
                                                          MMU_L1_SSECTION_FLAGS;
                }
            }
            else if (i == MMU_L1_SECTION) {
                printf("    Programming region as 1M entry: 0x%x for 0x%x\n",
                                va, pa);
                index = ((va & MMU_L1_SECTION_MASK) >> 20);
                mmu_l1_table[index] = (pa & MMU_L1_SECTION_MASK) |
                                                           MMU_L1_SECTION_FLAGS;
            }
            else if (i == MMU_L2_LPAGE || i == MMU_L2_SPAGE) {
                printf("    Invalid memory entry. All entries should either be "
                       "    a 16M or 1M entry only\n");
            }

            size -= mmu_sizes[i];
            va += mmu_sizes[i];
            pa += mmu_sizes[i];
        }

        me += 1;
    }

    dump_ttb();
}

/*
 *  ======== process_resources ========
 */
static void process_resources(struct rproc_fw_section * s)
{
    struct rproc_fw_resource * res = (struct rproc_fw_resource * )s->content;
    int i;

    printf("processing resource table... len: 0x%x\n",
               sizeof(struct rproc_fw_resource));
    for (i = 0; i < s->len / sizeof(struct rproc_fw_resource); i++) {
        printf("resource: %d, da: 0x%.8llx, pa: 0x%.8llx, len: 0x%.8x, name: %s\n",
               res[i].type, res[i].da, res[i].pa, res[i].len, res[i].name);
        switch(res[i].type) {
        case RSC_DEVMEM:
        case RSC_CARVEOUT:
            if (res[i].pa) {
                add_mem_entry(&res[i]);
            }
            else {
                printf("  Entry with undefined PA (to be assigned!)\n");
            }
            break;
        default:
            printf("  Nothing to do for this resource\n");
            break;
        }
    }
    printf("resource table processed...\n");
}

/*
 *  ======== process_image ========
 */
static int process_image(FILE * iofp, void * data, int size)
{
    struct rproc_fw_header *hdr;
    struct rproc_fw_section *s;
    int f_offset = 0;
    int sectno = 0;
    u64 pa;
    fw_toc_entry fw_toc_table[RPROC_MAX_FW_SECTIONS];
    u32 toc_table_length = 0;
    u32 toc_offset = 0;
    struct rproc_fw_section fw_new_section;
    FILE *dfp;

    if ((dfp = fopen(DEBUG_TOC_FILE, "w+b")) == NULL) {
        fprintf(stderr, "could not open: "DEBUG_TOC_FILE"\n");
        return -1;
    }

    memset(mmu_l1_table, 0, sizeof(mmu_l1_table));
    memset(fw_toc_table, 0, sizeof(fw_toc_table));

    printf("\nProcessing header...\n");
    hdr = (struct rproc_fw_header *)data;

    /* check that magic is what we expect */
    if (memcmp(hdr->magic, RPROC_FW_MAGIC, sizeof(hdr->magic))) {
        fprintf(stderr, "invalid magic number: %.4s\n", hdr->magic);
        return -1;
    }

    /* baseimage information */
    printf("magic number %.4s\n", hdr->magic);
    printf("header version %d\n", hdr->version);
    printf("header size %d\n", hdr->header_len);
    printf("header data\n%s\n", hdr->header);

    /* get the first section */
    s = (struct rproc_fw_section *)(hdr->header + hdr->header_len);

    printf("Processing sections....\n");
    f_offset += (u8 *)s - (u8 *)data;
    while ((u8 *)s < (u8 *)(data + size)) {
        f_offset += (u8 *)s->content - (u8 *)s;
        printf("section: %d, offset:0x%x  address: 0x%llx, size: 0x%x\n",
                        s->type, f_offset, s->da, s->len);
        if (s->type == FW_RESOURCE) {
            process_resources(s);
        }

        if (!va_to_pa(s->da, &pa)) {
            fw_toc_table[sectno].sect_id = sectno + 1;
            fw_toc_table[sectno].offset = f_offset;
            fw_toc_table[sectno].sect_pa = (u32)pa;
            fw_toc_table[sectno].sect_len = s->len;
            toc_table_length += sizeof(fw_toc_entry);
            fprintf(dfp, "0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", ++sectno, f_offset,
                        (u32)pa, s->len);
        }
        else {
            printf("  Ignoring this section for now! PA to be assigned!!\n");
        }

        f_offset += s->len;
        s = ((void *)s->content) + s->len;
    }
    printf("sections processed...\n");

    printf("\nCurrent position of files = 0x%lx 0x%lx\n", ftell(iofp),
                                                          ftell(dfp));
    printf("TOC regular sections size = 0x%x\n", toc_table_length);

    /* create and append the MMU L1 Page Table section to the binary */
    create_ttb();
    fw_new_section.type = FW_MMU;
    /* this address located right after text section, but before data section */
    fw_new_section.da = FW_MMU_BASE_PA;
    fw_new_section.len = sizeof(mmu_l1_table);
    fwrite(&fw_new_section, 1, 16, iofp);
    f_offset += 16; /* for the new fw_section header */
    fwrite(&mmu_l1_table, 1, sizeof(mmu_l1_table), iofp);

    /* add the MMU section to TOC */
    fw_toc_table[sectno].sect_id = sectno + 1;
    fw_toc_table[sectno].offset = f_offset;
    fw_toc_table[sectno].sect_pa = (u32)fw_new_section.da;
    fw_toc_table[sectno].sect_len = fw_new_section.len;
    toc_table_length += sizeof(fw_toc_entry);
    fprintf(dfp, "0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", ++sectno, f_offset,
                        (u32)fw_new_section.da, fw_new_section.len);

    /* add the end of section marker to TOC */
    fw_toc_table[sectno].sect_id = sectno + 1;
    fw_toc_table[sectno].offset = -1;
    fw_toc_table[sectno].sect_pa = -1;
    fw_toc_table[sectno].sect_len = -1;
    toc_table_length += sizeof(fw_toc_entry);
    fprintf(dfp, "0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", ++sectno, -1, -1, -1);

    /* append the FW section TOC for signing tool to the binary */
    fw_new_section.type = FW_SIGNATURE;
    fw_new_section.da = FW_SIGNATURE_BASE_PA;
    fw_new_section.len = toc_table_length + 0x148;
    fwrite(&fw_new_section, 1, 16, iofp);
    fwrite(fw_toc_table, 1, toc_table_length, iofp);
    toc_offset = ftell(iofp) - toc_table_length;

    printf("\nCurrent position of files = 0x%lx 0x%lx\n", ftell(iofp),
                        ftell(dfp));
    fwrite(&toc_offset, 1, 4, iofp);

    printf("Total TOC size = 0x%x\n", toc_table_length);
    printf("TOC Offset = 0x%x\n", toc_offset);
    fprintf(dfp, "TOC Offset = 0x%.8x\n", toc_offset);

    fclose(dfp);

    return 0;
}
