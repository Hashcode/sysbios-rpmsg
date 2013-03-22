/*
 *  Copyright (c) 2012-2013, Texas Instruments Incorporated
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
 *  ======== genextelf.c ========
 *  Version History:
 *  1.00 - Original Version
 *  1.01 - Fixed the ELF section offsets for alignment
 *  1.02 - Fixed the MMU TOC entries
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

/* Map types defined in rsc_types.h */
#define UInt32 unsigned int
#define Char char
#include "../ti/resources/rsc_types.h"

#include "rprcfmt.h"
#include "mmudefs.h"
#include "elfload/include/elf32.h"

#define VERSION               "1.02"

/* String definitions for ELF S-Section */
#define ELF_S_SECT_RESOURCE ".resource_table"
#define ELF_S_SECT_MMU      ".iommu_ttbr"
#define ELF_S_SECT_TTBRD    ".iommu_ttbr_updates"
#define ELF_S_SECT_TTBRS    ".secure_params"
#define ELF_S_SECT_CERT     ".certificate"
/* pad with additional NULL characters for alignment purposes */
#define ELF_S_SECT_ALL      \
        ".iommu_ttbr\0.iommu_ttbr_updates\0.secure_params\0.certificate"

/* Length of string definitions for ELF S-Section Strtab */
#define ELF_S_SECT_CERT_LEN     (strlen(ELF_S_SECT_CERT) + 1)
#define ELF_S_SECT_MMU_LEN      (strlen(ELF_S_SECT_MMU) + 1)
#define ELF_S_SECT_TTBRS_LEN    (strlen(ELF_S_SECT_TTBRS) + 1)
#define ELF_S_SECT_TTBRD_LEN    (strlen(ELF_S_SECT_TTBRD) + 1)
#define ELF_S_SECTS_LEN         (ELF_S_SECT_CERT_LEN + \
                                 ELF_S_SECT_MMU_LEN + \
                                 ELF_S_SECT_TTBRS_LEN + \
                                 ELF_S_SECT_TTBRD_LEN)

/* Alignment & size for reserved memory for secure params */
#define SECURE_DATA_ALIGN       0x1000
#define SECURE_DATA_SIZE        0x2000

/* Max values used for different types */
#define RPROC_MAX_MEM_ENTRIES   20
#define RPROC_MAX_CARVEOUTS     10
#define RPROC_MAX_FW_SECTIONS   30

/* Constants related to processing MMU entries */
#define RPROC_L1_ENTRIES        4096
#define MMU_DYNAMIC_FLAG        1

/* Description of boot region for ducati */
#define FW_BOOT_SIZE            0x4000

/* Certificate length in bytes not including TOC and alignment */
#define FW_CERTIFICATE_SIZE     0x148
#define FW_CERTIFICATE_ALIGN    4

/* Debug macros */
#define DEBUG_MMU_FILE          "mmu_ttb_dump.log"
#define DEBUG_MMU_UPDATE        "mmu_ttb_updates.log"
#define DEBUG_TOC_FILE          "sign_toc.log"
#define DEBUG_GENEXTELF         0

#if DEBUG_GENEXTELF
#define DEBUG_PRINT(fmt, ...)   printf(fmt, ## __VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif


/* Header of the resource table */
typedef struct resource_table {
    u32 ver;
    u32 num;
    u32 reserved[2];
    u32 offset[0];
} resource_table;

/* Resource header */
typedef struct {
    u32 type;
    u8  data[0];
} resource_header;

typedef struct {
    u32 type;
    u32 va;
    u32 pa;
    u32 len;
    u32 memregion;
} va_pa_entry;

typedef struct {
    u32 memregion;
    u32 offset;
    u32 sect_pa;
    u32 sect_len;
} fw_toc_entry;

typedef struct {
    u8  count;
    u8  region;
    u16 index;
    u32 value;
} mmu_dynamic_entry;

/*
 * Defined structure used for passing secure params
 * MUST MATCH definition in include/linux/rproc_drm.h
 */
typedef struct {
    u32 ducati_smem;
    u32 ducati_smem_size;
    u32 ducati_code;
    u32 ducati_code_size;
    u32 ducati_data;
    u32 ducati_data_size;
    u32 ducati_vring_smem;
    u32 ducati_vring_size;
    u32 secure_buffer_address;
    u32 secure_buffer_size;
    u32 decoded_buffer_address;
    u32 decoded_buffer_size;
    u32 ducati_base_address;
    u32 ducati_toc_address;
    u32 ducati_page_table_address;
} rproc_drm_sec_params;

static int resource_table_index = 0;
static int carveout_num = 0;
static struct fw_rsc_devmem *carveout[RPROC_MAX_CARVEOUTS];

va_pa_entry va_pa_table[RPROC_MAX_MEM_ENTRIES];
u32 mmu_l1_table[RPROC_L1_ENTRIES];

mmu_dynamic_entry mmu_dynamic_entries[RPROC_L1_ENTRIES];
int mmu_dynamic_cnt = 0;

/* Local functions */
static int check_image(void * data, int size);
static int process_image(FILE *iofp, void * data, int size);
static void print_secthdr_info(struct Elf32_Shdr *s1);
static void print_proghdr_info(struct Elf32_Phdr *p1);


static void print_help(void)
{
    fprintf(stderr, "\nInvalid arguments!\n"
                  "Usage: ./genextelf <[-i] <Unsigned input ELF image>> "
                  "[<[-o] <Presigned output ELF image>>]\n"
                  "Rules: - Output file is optional, if not given,\n"
                  "         input ELF file will be modified in place.\n"
                  "       - Arguments can be specified in any order as long\n"
                  "         as the corresponding option is specified.\n"
                  "       - All arguments not preceded by an option are \n"
                  "         applied as input binary and output binary in \n"
                  "         that order.\n");
    exit(1);
}

/*
 *  ======== main ========
 */
int main(int argc, char * argv[])
{
    FILE * ifp;
    FILE * ofp;
    struct stat st;
    char * infile;
    char * outfile;
    void * data;
    int size;
    int status = 0;
    int i, o;
    char *bin_files[] = {NULL, NULL};
    int num_files = sizeof(bin_files) / sizeof(bin_files[0]);

    printf("###############################################################\n");
    printf("                     GENEXTELF : %s    \n", VERSION);
    printf("###############################################################\n");

    /* process arguments */
    while ((o = getopt (argc, argv, ":i:o:")) != -1) {
        switch (o) {
            case 'i':
                bin_files[0] = optarg;
                break;
            case 'o':
                bin_files[1] = optarg;
                break;
            case ':':
                status = -1;
                printf("Option -%c requires an operand\n", optopt);
                break;
            case '?':
                status = -1;
                printf("Unrecognized option: -%c\n", optopt);
                break;
        }
    }

    for (i = 0; optind < argc; optind++) {
        while (i < num_files && bin_files[i]) {
            i++;
        }
        if (i == num_files) {
            print_help();
        }
        bin_files[i++] = argv[optind];
    }

    if (status || !bin_files[0] ||
            (bin_files[1] && (!strcmp(bin_files[0], bin_files[1])))) {
        print_help();
    }

    DEBUG_PRINT("\nInput File = %s, Output File = %s\n",
                                            bin_files[0], bin_files[1]);
    infile = bin_files[0];
    if ((ifp = fopen(infile, "r+b")) == NULL) {
        fprintf(stderr, "%s: could not open: %s\n", argv[0], infile);
        exit(2);
    }

    fstat(fileno(ifp), &st);
    size = st.st_size;
    if (!size) {
        fprintf(stderr, "%s: %s size is invalid.\n", argv[0], infile);
        fclose(ifp);
        exit(3);
    }

    outfile = bin_files[1];
    if (outfile && ((ofp = fopen(outfile, "w+b")) == NULL)) {
        fprintf(stderr, "%s: could not open: %s\n", argv[0], outfile);
        fclose(ifp);
        exit(2);
    }

    printf("\nPreparing Input ELF file: %s of size: %d\n", infile, size);
    data = malloc(size);
    fread(data, 1, size, ifp);

    status = check_image(data, size);
    if (status == 0) {
        if (!outfile) {
            ofp = ifp;
            rewind(ofp);
        }
        status = process_image(ofp, data, size);
        rewind(ofp);
        fstat(fileno(ofp), &st);
        size = st.st_size;
        printf("\nProcessed Output ELF file: %s of size: %d\n\n",
                            outfile ? outfile : infile, size);
    }
    else if (status == 1) {
        printf("\n** File was already processed and has certificate. **\n\n");
    }

    free(data);
    if (outfile) {
        fclose(ofp);
    }
    fclose(ifp);

    return status;
}

/*
 *  ======== add_mem_entry ========
 */
static int add_mem_entry(struct fw_rsc_devmem *rsc)
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
        /* memregion is relevant only for dynamic carveouts */
        if (rsc->pa == 0) {
            me->memregion = rsc->memregion;
        }
        me->len = rsc->len;
        DEBUG_PRINT("  Added entry %d: va 0x%.8x pa 0x%.8x len 0x%.8x "
                    "dynamic %d\n", i, me->va, me->pa, me->len, !rsc->pa);
    }

    return ret;
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
 *  ======== dump_ttb_dynamic ========
 */
static void dump_ttb_dynamic(void)
{
    FILE * fp;
    int i = 0;

    if ((fp = fopen(DEBUG_MMU_UPDATE, "w+b")) == NULL) {
        fprintf(stderr, "could not open: "DEBUG_MMU_UPDATE"\n");
        return;
    }

    fprintf(fp, "COUNT  REGION  INDEX  VALUE\n");
    for (i = 0; i < mmu_dynamic_cnt; i++) {
        fprintf(fp, "%3d      %d     0x%.3x  0x%.8x\n",
            mmu_dynamic_entries[i].count, mmu_dynamic_entries[i].region,
            mmu_dynamic_entries[i].index, mmu_dynamic_entries[i].value);
    }
    fprintf(fp, "Total Entries = %d\n", mmu_dynamic_cnt);

    fclose(fp);
}

/*
 *  ======== create_ttb ========
 *  TODO: Extend the MMU table generation to be able to do L2 tables as
 *        well (to provide large page or small page mapping granularity).
 */
static int create_ttb(void)
{
    int i, j;
    va_pa_entry *me = va_pa_table;
    u32 va, pa;
    u32 va_pa;
    u32 size;
    u32 mmu_sizes[4] = {MMU_SZ_16M, MMU_SZ_1M, MMU_SZ_64K, MMU_SZ_4K};
    u32 index, dval;

    memset(mmu_dynamic_entries, 0, sizeof(mmu_dynamic_entries));

    while (me->len) {
        size = me->len;
        va = me->va;
        pa = me->pa;
        dval = 0;

        DEBUG_PRINT("Preparing MMU entry for va = 0x%x pa = 0x%x len = 0x%x\n",
                  va, pa, size);
        while (size) {
            va_pa = va | pa;
            for (i = 0; i < 4; i++) {
                if ((size >= mmu_sizes[i]) &&
                    ((va_pa & (mmu_sizes[i] - 1)) == 0)) {
                      break;
                }
            }

            /*
             * Use 1MB entries for dynamic carveouts (pa == 0) as memory
             * allocation for these carveouts may not align on 16MB.
             */
            if (!me->pa) {
                i = MMU_L1_SECTION;
            }

            if (i == MMU_L1_SSECTION) {
                DEBUG_PRINT("    Programming region as 16M entry: 0x%x for "
                            "0x%x\n", va, pa);
                index = ((va & MMU_L1_SECTION_MASK) >> 20);
                dval = 0;
                /* redundant code, but added for future scalability */
                if (!me->pa) {
                    if (mmu_dynamic_entries[mmu_dynamic_cnt].count == 0) {
                        mmu_dynamic_entries[mmu_dynamic_cnt].value =
                            (pa & MMU_L1_SSECTION_MASK) | MMU_L1_SSECTION_FLAGS;
                        mmu_dynamic_entries[mmu_dynamic_cnt].index = index;
                        mmu_dynamic_entries[mmu_dynamic_cnt].region =
                                                                me->memregion;
                    }
                    mmu_dynamic_entries[mmu_dynamic_cnt].count += 16;
                    dval = MMU_DYNAMIC_FLAG;
                }
                for (j = 0; j < 16; j++) {
                    mmu_l1_table[index + j] = (pa & MMU_L1_SSECTION_MASK) |
                                                  MMU_L1_SSECTION_FLAGS | dval;
                }
            }
            else if (i == MMU_L1_SECTION) {
                DEBUG_PRINT("    Programming region as 1M entry: 0x%x for "
                            "0x%x\n", va, pa);
                index = ((va & MMU_L1_SECTION_MASK) >> 20);
                dval = 0;
                if (!me->pa) {
                    if (mmu_dynamic_entries[mmu_dynamic_cnt].count == 0) {
                        mmu_dynamic_entries[mmu_dynamic_cnt].value =
                            (pa & MMU_L1_SECTION_MASK) | MMU_L1_SECTION_FLAGS;
                        mmu_dynamic_entries[mmu_dynamic_cnt].index = index;
                        mmu_dynamic_entries[mmu_dynamic_cnt].region =
                                                                me->memregion;
                    }
                    mmu_dynamic_entries[mmu_dynamic_cnt].count += 1;
                    dval = MMU_DYNAMIC_FLAG;
                }
                mmu_l1_table[index] = (pa & MMU_L1_SECTION_MASK) |
                                                  MMU_L1_SECTION_FLAGS | dval;
            }
            else if (i == MMU_L2_LPAGE || i == MMU_L2_SPAGE) {
                printf("    Invalid memory entry. All entries should either be "
                       "    a 16M or 1M entry only\n");
            }

            size -= mmu_sizes[i];
            va += mmu_sizes[i];
            pa += mmu_sizes[i];
        }
        if (!me->pa) {
            DEBUG_PRINT("  Adding dynamic entry: %d index: 0x%x va: 0x%x "
                        "pa: 0x%x count: %d\n", mmu_dynamic_cnt,
                        mmu_dynamic_entries[mmu_dynamic_cnt].index, me->va,
                        me->pa, mmu_dynamic_entries[mmu_dynamic_cnt].count);
            mmu_dynamic_cnt++;
        }
        me += 1;
    }

    dump_ttb();
    if (mmu_dynamic_cnt) {
        dump_ttb_dynamic();
    }

    return (mmu_dynamic_cnt * sizeof(mmu_dynamic_entry));
}

/*
 *  ======== process_resources ========
 */
static int process_resources(void *s, int len)
{
    resource_table *t = (resource_table *) s;
    struct fw_rsc_devmem *res;
    int i;
    u32 *offset = &(t->offset[0]);

    DEBUG_PRINT("\nProcess resource table....\n");
    if (t->ver != 1) {
        fprintf(stderr, "Invalid Resource Table version: %d\n", t->ver);
        return -1;
    }

    if (t->reserved[0] || t->reserved[1]) {
        fprintf(stderr, "Non zero reserved bytes in resource table\n");
        return -1;
    }

    for (i = 0; i < t->num; i++) {
        res =  (struct fw_rsc_devmem *) (*offset++ + s);
        switch (res->type) {
            case RSC_CARVEOUT:
                /* store carveout information */
                carveout[carveout_num++] = res;
            case RSC_DEVMEM:
                DEBUG_PRINT("resource: %d, da: 0x%.8x, pa: 0x%.8x, len: 0x%.8x,"
                            " name: %s\n", res->type, res->da, res->pa,
                            res->len, res->name);
                add_mem_entry(res);
                break;
            default:
                DEBUG_PRINT("resource: %d, nothing to do for this resource\n",
                            res->type);
                break;
        }
    }
    DEBUG_PRINT("resource table processed...\n");

    DEBUG_PRINT("\nNumber of Carveout sections = %d\n", carveout_num);
    if (carveout_num == 0) {
        fprintf(stderr, "No carveout sections in resource table\n");
        return -1;
    }
    for (i = 0; i < carveout_num; i++) {
        DEBUG_PRINT("%d: type: %d da: 0x%.8x pa: 0x%.8x len: 0x%.8x "
                    "memregion: 0x%.8x name: %s\n", i, carveout[i]->type,
                    carveout[i]->da, carveout[i]->pa, carveout[i]->len,
                    carveout[i]->memregion, carveout[i]->name);
    }

    return 0;
}

/*
 *  ======== check_image ========
 */
static int check_image(void * data, int size)
{
    struct Elf32_Ehdr *hdr;
    struct Elf32_Shdr *s;
    int i, offset;

    DEBUG_PRINT("\nChecking header....");
    hdr = (struct Elf32_Ehdr *)data;

    /* check for correct ELF magic numbers in file header */
    if ((!hdr->e_ident[EI_MAG0] == ELFMAG0) ||
        (!hdr->e_ident[EI_MAG1] == ELFMAG1) ||
        (!hdr->e_ident[EI_MAG2] == ELFMAG2) ||
        (!hdr->e_ident[EI_MAG3] == ELFMAG3)) {
        fprintf(stderr, "Invalid ELF magic number.\n");
        return -1;
    }

    if (hdr->e_type != ET_EXEC) {
        fprintf(stderr, "Invalid ELF file number.\n");
        return -1;
    }
    DEBUG_PRINT("done.\n");

    DEBUG_PRINT("Checking file size....");
    if ((size < (hdr->e_phoff + (hdr->e_phentsize * hdr->e_phnum))) ||
        (size < (hdr->e_shoff + (hdr->e_shentsize * hdr->e_shnum)))) {
        fprintf(stderr, "File size Invalid.\n");
        return -1;
    }
    DEBUG_PRINT("done.\n");

    /* get the string name section offset */
    s = (struct Elf32_Shdr *)(data + hdr->e_shoff);
    s += hdr->e_shstrndx;
    offset = s->sh_offset;

    /* check for resource table section */
    DEBUG_PRINT("Checking for %s section....", ELF_S_SECT_RESOURCE);
    s = (struct Elf32_Shdr *)(data + hdr->e_shoff);
    for (i = 0; i < hdr->e_shnum; i++, s++) {
        if (strcmp((data + offset + s->sh_name), ELF_S_SECT_RESOURCE) == 0) {
            resource_table_index = i;
            DEBUG_PRINT("found at index %d, done.\n", i);
            DEBUG_PRINT("  Details of %s section:\n  ", ELF_S_SECT_RESOURCE);
            print_secthdr_info(s);
            break;
        }
    }
    if (i == hdr->e_shnum) {
        fprintf(stderr, "\nFile does not have %s section.\n",
                                                        ELF_S_SECT_RESOURCE);
        return -1;
    }

    /* check if certificate section is already present  */
    DEBUG_PRINT("Checking for %s section....", ELF_S_SECT_CERT);
    s = (struct Elf32_Shdr *) (data + hdr->e_shoff);
    for (i = 0; i < hdr->e_shnum; i++, s++) {
        if (strcmp((data + offset + s->sh_name), ELF_S_SECT_CERT) == 0) {
            DEBUG_PRINT("found, done.\n");
            DEBUG_PRINT("  Details of %s section:\n  ", ELF_S_SECT_CERT);
            print_secthdr_info(s);
            return 1;
        }
    }
    DEBUG_PRINT("done.\n");

    return 0;
}

/*
 *  ======== find_carveout_offset =======
 */
static int find_carveout_offset(void *data, struct Elf32_Phdr *p, u32 *sect)
{
    int i, j, offset, c_offset;

    for (c_offset = -1, i = 0, j = 0; i < carveout_num; i++) {
        offset = p->p_paddr - carveout[i]->da;

        if (offset < 0) {
            j += carveout[i]->len;
            continue;
        }

        if ((offset + p->p_filesz) > carveout[i]->len) {
            j += carveout[i]->len;
            continue;
        }

        *sect = carveout[i]->memregion;
        c_offset = j + offset;
        break;
    }

    DEBUG_PRINT("File Offset = 0x%.8x VA = 0x%.8x Carveout No = %d "
                "Carveout Size = 0x%.8x Offset = 0x%.8x Type = %d\n",
                p->p_offset, p->p_paddr, i, carveout[i]->len, c_offset, *sect);

    if (offset == -1) {
        fprintf(stderr,"Carveout memory too small mapping sections\n");
    }

    return c_offset;
}

/*
 * ============ map_mem_for_secure_ttb ============
 */
int map_mem_for_secure_ttb(struct Elf32_Phdr *p_mmu, struct Elf32_Phdr *p,
                           int num)
{
    int i;
    struct fw_rsc_devmem *c_code = NULL;
    u32 c_offset = 0;

    if (carveout_num == 0) {
        return -1;
    }

    /* find the carveout for code region */
    for (i = 0; i < carveout_num; i++) {
        if (carveout[i]->memregion == RPROC_MEMREGION_CODE) {
            c_code = carveout[i];
            break;
        }
    }
    if (c_code == NULL) {
        fprintf(stderr, "Code carveout region not found\n");
        return -1;
    }

    /* find the last section that is mapped to code region */
    for (i = 0; i < num; i++, p++) {
        if (p->p_paddr < (c_code->da + c_code->len)) {
            if ((p->p_paddr + p->p_memsz) > c_offset) {
                c_offset = p->p_paddr + p->p_memsz;
            }
        }
    }
    if (c_offset == 0) {
        fprintf(stderr, "Unable to map TTBR table\n");
        return -1;
    }

    /* check if enough space is available for secure region */
    if (c_offset & (SECURE_DATA_ALIGN - 1)) {
        c_offset += SECURE_DATA_ALIGN - (c_offset & (SECURE_DATA_ALIGN - 1));
    }

    if ((c_offset + SECURE_DATA_SIZE) > (c_code->da + c_code->len)) {
        fprintf(stderr, "Not enough space in code section for ");
        fprintf(stderr, "SECURE TTBR table and params. Increase");
        fprintf(stderr, "code carveout region by 0x%x\n",
            (c_offset + p_mmu->p_filesz) - (c_code->da + c_code->len));
        fprintf(stderr, "\n Unable to pre-sign Image \n");
        return -1;
    }

    printf(" (TTBR @ 0x%x)....", c_code->da + c_offset);
    p_mmu->p_paddr = c_offset;
    p_mmu->p_vaddr = c_offset;

    return 0;
}

/*
 *  ======== print_toc_file ========
 */
static int print_toc_file(fw_toc_entry *toc, int len, int offset)
{
    FILE *dfp;
    int i;

    if ((dfp = fopen(DEBUG_TOC_FILE, "w+b")) == NULL) {
        fprintf(stderr, "could not open: "DEBUG_TOC_FILE"\n");
        return -1;
    }

    for (i = 0; i < len; toc++, i++) {
        fprintf(dfp, "0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", toc->memregion,
            toc->offset, toc->sect_pa, toc->sect_len);
    }

    fprintf(dfp, "TOC Offset = 0x%.8x\n", offset);

    fclose(dfp);

    return 0;
}

/*
 *  ======== fill_zero ========
 */
static int fill_zero(int offset, int align, FILE *fp)
{
    int fill;
    int count = 0;

    fill = align - (offset % align);
    if (fill != align) {
        DEBUG_PRINT("\n  fill = 0x%x co = 0x%x align = 0x%x\n", fill, offset, align);
        while (fill--) {
            fputc(0, fp);
            count++;
        }
        DEBUG_PRINT("  fill done = 0x%x co = 0x%x align = 0x%x\n", fill,
                                                        offset + count, align);
    }

    return count;
}

/*
 *  ======== process_image ========
 */
static int process_image(FILE * iofp, void * data, int size)
{
    struct Elf32_Ehdr *hdr;
    struct Elf32_Shdr *shdr, *s, *str_sect;
    struct Elf32_Phdr *phdr, *p, p_mmu, p_cert;
    struct Elf32_Shdr *res_table, s_mmu, s_cert, sttbr1, sttbr2;
    int i, j, sect_pa, d_num, data_len;
    int phdr_num, shdr_num, co;
    rproc_drm_sec_params sec_params;
    int sectno = 0;
    fw_toc_entry fw_toc_table[RPROC_MAX_FW_SECTIONS];

    co = 0;
    memset(mmu_l1_table, 0, sizeof(mmu_l1_table));
    memset(fw_toc_table, 0, sizeof(fw_toc_table));
    memset(&sec_params, 0, sizeof(rproc_drm_sec_params));

    /* pointer to ELF header */
    hdr = (struct Elf32_Ehdr *) data;
    phdr = (struct Elf32_Phdr *) (data + hdr->e_phoff);
    shdr = (struct Elf32_Shdr *) (data + hdr->e_shoff);

    /* store current header variables for reusing */
    data_len   = hdr->e_phoff - hdr->e_ehsize;
    phdr_num   = (int) hdr->e_phnum;
    shdr_num   = (int) hdr->e_shnum;
    str_sect   = shdr + hdr->e_shstrndx;

    /* process resource table to lookup memory information */
    res_table = shdr + resource_table_index;
    if (process_resources(data + res_table->sh_offset, res_table->sh_size)) {
        return -1;
    }

    /* write new ELF header - adding 2 new prog hdrs and 4 new sections */
    printf("\nWriting ELF header....");
    hdr->e_shnum += 4;
    hdr->e_phnum += 2;

    /* extend the offset to account for new string names and boot section */
    hdr->e_phoff += ELF_S_SECTS_LEN;
    hdr->e_shoff += ELF_S_SECTS_LEN + (2 * hdr->e_phentsize);

    fwrite(hdr, 1, hdr->e_ehsize, iofp);
    co += hdr->e_ehsize;
    printf("done.\n");

    DEBUG_PRINT("Program Offset = 0x%x, Program & Section length = 0x%x\n",
                                        hdr->e_ehsize, data_len);
    DEBUG_PRINT("P-Header Count = %d, P-Header Offset = 0x%x\n\n",
                                        hdr->e_phnum, hdr->e_phoff);

    /* write program data */
    printf("Writing all the ELF program and section data....");
    p = phdr;
    fwrite((data + p->p_offset), 1, (data_len - p->p_offset + hdr->e_ehsize),
                                                                        iofp);
    co += (data_len - p->p_offset + hdr->e_ehsize);

    /* write the new section names */
    fwrite(ELF_S_SECT_ALL, 1, ELF_S_SECTS_LEN, iofp);
    co += ELF_S_SECTS_LEN;
    printf("done.\n");

    /* write program headers */
    printf("Writing program headers....");
    for (i = 0, p = phdr; i < phdr_num; i++, p++) {
        fwrite(p, 1, hdr->e_phentsize, iofp);
        co += hdr->e_phentsize;
    }
    printf("done.\n");

    /* prepare TOC table by processing program sections with non-zero size. */
    DEBUG_PRINT("\nPreparing TOC contents....\n");
    for (i = 0, p = phdr; i < (hdr->e_phnum - 2); i++, p++) {
        if (p->p_filesz > 0) {
            fw_toc_table[sectno].offset = (u32) p->p_offset;
            fw_toc_table[sectno].sect_pa = find_carveout_offset(
                                data, p, &fw_toc_table[sectno].memregion);
            if (fw_toc_table[sectno].sect_pa == -1) {
                fprintf(stderr, "Unable to find pa offset for p_vaddr = 0x%x\n",
                                p->p_vaddr);
                return -1;
            }
            fw_toc_table[sectno++].sect_len = (u32) p->p_filesz;
        }
    }
    DEBUG_PRINT("done.\n");

    /* generate ttb entries */
    DEBUG_PRINT("\nPrepare TTB table....\n");
    d_num = create_ttb();
    DEBUG_PRINT("TTB table created, size of dynamic entry region (x 2) "
                "= 0x%x\n\n", (d_num * 2));

    /* create and write MMU table section Header */
    printf("Writing program header for %s section", ELF_S_SECT_MMU);
    p_mmu.p_type   = PT_LOAD;
    p_mmu.p_offset = hdr->e_ehsize + data_len + ELF_S_SECTS_LEN +
                        (hdr->e_phnum * hdr->e_phentsize) +
                        (hdr->e_shnum * hdr->e_shentsize);
    p_mmu.p_align  = SECURE_DATA_ALIGN;
    p_mmu.p_offset += p_mmu.p_align - (p_mmu.p_offset % p_mmu.p_align);
    p_mmu.p_filesz = d_num + sizeof(mmu_l1_table) +
                                sizeof(rproc_drm_sec_params);
    p_mmu.p_memsz  = p_mmu.p_filesz;
    p_mmu.p_flags  = PF_R;

    if (map_mem_for_secure_ttb(&p_mmu, phdr, (hdr->e_phnum - 1))) {
        return -1;
    }

    fwrite(&p_mmu, 1, hdr->e_phentsize, iofp);
    co += hdr->e_phentsize;
    printf("done.\n");

    /* update the TOC entries for static entries in MMU table */
    fw_toc_table[sectno].offset   = p_mmu.p_offset;
    fw_toc_table[sectno].sect_pa  = find_carveout_offset(data, &p_mmu,
                                              &fw_toc_table[sectno].memregion);
    fw_toc_table[sectno].sect_len = 0;
    sect_pa = fw_toc_table[sectno].sect_pa;
    for (i = 0, j = 0; i < RPROC_L1_ENTRIES; i++, j += 4) {
        if (fw_toc_table[sectno].sect_len == 0) {
            if ((mmu_l1_table[i] & MMU_DYNAMIC_FLAG) == MMU_DYNAMIC_FLAG) {
                mmu_l1_table[i] &= (~MMU_DYNAMIC_FLAG);
                fw_toc_table[sectno].offset += sizeof(u32);
                fw_toc_table[sectno].sect_pa += sizeof(u32);
            }
            else {
                fw_toc_table[sectno].sect_len += sizeof(u32);
            }
        }
        else {
            if ((mmu_l1_table[i] & MMU_DYNAMIC_FLAG) == MMU_DYNAMIC_FLAG) {
                mmu_l1_table[i] &= (~MMU_DYNAMIC_FLAG);
                sectno++;
                fw_toc_table[sectno].memregion =
                                            fw_toc_table[sectno - 1].memregion;
                fw_toc_table[sectno].offset = p_mmu.p_offset + j + sizeof(u32);
                fw_toc_table[sectno].sect_pa = sect_pa + j + sizeof(u32);
                fw_toc_table[sectno].sect_len = 0;
            }
            else {
                fw_toc_table[sectno].sect_len += sizeof(u32);
            }
        }
    }
    if (fw_toc_table[sectno].sect_len) {
        sectno++;
    }

    /* add another TOC entry for mmu_dynamic entries */
    fw_toc_table[sectno].memregion = fw_toc_table[sectno - 1].memregion;
    fw_toc_table[sectno].offset = p_mmu.p_offset + sizeof(mmu_l1_table);
    fw_toc_table[sectno].sect_pa = sect_pa + sizeof(mmu_l1_table);
    fw_toc_table[sectno].sect_len = d_num;
    sectno++;

    /* add the end of section marker to TOC */
    fw_toc_table[sectno].memregion = 0;
    fw_toc_table[sectno].offset = -1;
    fw_toc_table[sectno].sect_pa = -1;
    fw_toc_table[sectno].sect_len = -1;
    sectno++;

    /* create and write TOC+certificate section header */
    printf("Writing program header for %s section....", ELF_S_SECT_CERT);
    p_cert.p_type   = PT_LOAD;
    p_cert.p_align  = FW_CERTIFICATE_ALIGN;
    p_cert.p_vaddr  = p_mmu.p_vaddr + p_mmu.p_filesz;
    p_cert.p_paddr  = p_mmu.p_paddr + p_mmu.p_filesz;
    p_cert.p_offset = p_mmu.p_offset + p_mmu.p_filesz;
    p_cert.p_filesz = FW_CERTIFICATE_SIZE + (sizeof(fw_toc_entry) * sectno);
    p_cert.p_memsz  = p_cert.p_filesz;
    p_cert.p_flags  = PF_R;
    fwrite(&p_cert, 1, hdr->e_phentsize, iofp);
    co += hdr->e_phentsize;
    printf("done.\n");

    DEBUG_PRINT("\nProcessed P-sections info for %d entries:\n", hdr->e_phnum);
    DEBUG_PRINT("Offset     VA         PA         FileSz     MemSz      Align"
        "    Type\n");
    for (i = 0, p = phdr; i < (hdr->e_phnum - 2); i++, p++)  {
        print_proghdr_info(p);
    }
    /* last 2 are the newly added program hdr sections - MMU & Cert */
    print_proghdr_info(&p_mmu);
    print_proghdr_info(&p_cert);
    DEBUG_PRINT("\n");

    /* write S-section headers */
    printf("Writing original S-section headers....");
    str_sect->sh_size += ELF_S_SECTS_LEN;
    for (i = 0, s = shdr; i < shdr_num; i++, s++) {
        if ((s->sh_type == SHT_PROGBITS) || (s->sh_type == SHT_NOBITS)) {
            if (s->sh_offset == 0) {
                fwrite(s, 1, hdr->e_shentsize, iofp);
                co += hdr->e_shentsize;
            }
            else {
                    fwrite(s, 1, hdr->e_shentsize, iofp);
                    co += hdr->e_shentsize;
            }
        }
        else {
            fwrite(s, 1, hdr->e_shentsize, iofp);
            co += hdr->e_shentsize;
        }
    }
    printf("done.\n");
    DEBUG_PRINT("\n");

    printf("Writing S-section header for %s section....", ELF_S_SECT_MMU);
    s_mmu.sh_name = str_sect->sh_name + strlen(".shstrtab") + 1;
    s_mmu.sh_type = SHT_PROGBITS;
    s_mmu.sh_flags = SHF_ALLOC;
    s_mmu.sh_addr = p_mmu.p_paddr;
    s_mmu.sh_offset = p_mmu.p_offset;
    s_mmu.sh_size = sizeof(mmu_l1_table);
    s_mmu.sh_link = 0;
    s_mmu.sh_info = 0;
    s_mmu.sh_addralign = p_mmu.p_align;
    s_mmu.sh_entsize = 0;
    fwrite(&s_mmu, 1, hdr->e_shentsize, iofp);
    co += hdr->e_shentsize;
    printf("done.\n");
    DEBUG_PRINT("Details of S-section %s:\n", ELF_S_SECT_MMU);
    print_secthdr_info(&s_mmu);

    printf("Writing S-section header for %s section....", ELF_S_SECT_TTBRD);
    sttbr1.sh_name = s_mmu.sh_name + ELF_S_SECT_MMU_LEN;
    sttbr1.sh_type = SHT_PROGBITS;
    sttbr1.sh_flags = SHF_ALLOC;
    sttbr1.sh_addr = p_mmu.p_paddr + sizeof(mmu_l1_table);
    sttbr1.sh_offset = p_mmu.p_offset + sizeof(mmu_l1_table);
    sttbr1.sh_size = d_num;
    sttbr1.sh_link = 0;
    sttbr1.sh_info = 0;
    sttbr1.sh_addralign = p_cert.p_align;
    sttbr1.sh_entsize = 0;
    fwrite(&sttbr1, 1, hdr->e_shentsize, iofp);
    co += hdr->e_shentsize;
    printf("done.\n");
    DEBUG_PRINT("Details of S-section %s:\n", ELF_S_SECT_TTBRD);
    print_secthdr_info(&sttbr1);

    printf("Writing S-section header for %s section....",
        ELF_S_SECT_TTBRS);
    sttbr2.sh_name = sttbr1.sh_name + ELF_S_SECT_TTBRD_LEN;
    sttbr2.sh_type = SHT_PROGBITS;
    sttbr2.sh_flags = SHF_ALLOC;
    sttbr2.sh_addr = p_mmu.p_paddr + sizeof(mmu_l1_table) + d_num;
    sttbr2.sh_offset = p_mmu.p_offset + sizeof(mmu_l1_table) + d_num;
    sttbr2.sh_size = sizeof(rproc_drm_sec_params);
    sttbr2.sh_link = 0;
    sttbr2.sh_info = 0;
    sttbr2.sh_addralign = p_cert.p_align;
    sttbr2.sh_entsize = 0;
    fwrite(&sttbr2, 1, hdr->e_shentsize, iofp);
    co += hdr->e_shentsize;
    printf("done.\n");
    DEBUG_PRINT("Details of S-section %s:\n", ELF_S_SECT_TTBRS);
    print_secthdr_info(&sttbr2);

    printf("Writing S-section header for .certificate section....");
    s_cert.sh_name = sttbr2.sh_name + ELF_S_SECT_TTBRS_LEN;
    s_cert.sh_type = SHT_PROGBITS;
    s_cert.sh_flags = SHF_ALLOC;
    s_cert.sh_addr = p_cert.p_paddr;
    s_cert.sh_offset = p_cert.p_offset;
    s_cert.sh_size = p_cert.p_memsz;
    s_cert.sh_link = 0;
    s_cert.sh_info = 0;
    s_cert.sh_addralign = p_cert.p_align;
    s_cert.sh_entsize = 0;
    fwrite(&s_cert, 1, hdr->e_shentsize, iofp);
    co += hdr->e_shentsize;
    printf("done.\n");
    DEBUG_PRINT("Details of S-section %s:\n", ELF_S_SECT_CERT);
    print_secthdr_info(&s_cert);

    /* write the MMU table */
    printf("Writing program data for sections %s", ELF_S_SECT_MMU);
    co += fill_zero(co, p_mmu.p_align, iofp);
    fwrite(mmu_l1_table, 1, sizeof(mmu_l1_table), iofp);
    co += sizeof(mmu_l1_table);

    /* write the dynamic entries twice */
    if (mmu_dynamic_cnt) {
        printf(", %s", ELF_S_SECT_TTBRS);
        fwrite(mmu_dynamic_entries, 1, d_num, iofp);
        co += d_num;
    }

    printf(" & %s", ELF_S_SECT_TTBRD);
    fwrite(&sec_params, 1, sizeof(rproc_drm_sec_params), iofp);
    co += sizeof(rproc_drm_sec_params);
    printf("....done.\n");

    /* write TOC table */
    printf("Writing TOC....");
    fwrite(fw_toc_table, 1, (sizeof(fw_toc_entry) * sectno), iofp);

    /* write the offset to TOC section at end of file */
    fwrite(&co, 1, 4, iofp);
    printf("done.\n");

#if 0 /* dummy certificate */
    for (i = 0; i < FW_CERTIFICATE_SIZE; i++) {
       fwrite("\0", 1, 1, iofp);
    }
#endif

    print_toc_file(fw_toc_table, sectno, co);

    return 0;
}

/*
 *  ======== print_secthdr_info ========
 */
static void print_secthdr_info(struct Elf32_Shdr *s1)
{
#if DEBUG_GENEXTELF
    printf("Offset = 0x%.8x Size = 0x%.8x Name-Offset = %.2d Align = 0x%.4x ",
                s1->sh_offset, s1->sh_size, s1->sh_name, s1->sh_addralign);

    if (s1->sh_flags & SHF_WRITE) printf("W");
    if (s1->sh_flags & SHF_ALLOC) printf("A");
    if (s1->sh_flags & SHF_EXECINSTR) printf("E");
    if (s1->sh_flags & SHF_MERGE) printf("M");
    if (s1->sh_flags & SHF_STRINGS) printf("S");
    if (s1->sh_flags & SHF_INFO_LINK) printf("IL");
    if (s1->sh_flags & SHF_LINK_ORDER) printf("LO");
    if (s1->sh_flags & SHF_OS_NONCONFORMING) printf("OS");
    if (s1->sh_flags & SHF_GROUP) printf("TLS");
    if (s1->sh_flags & SHF_MASKOS) printf("M OS");
    if (s1->sh_flags & SHF_MASKPROC) printf("Proc");

    switch (s1->sh_type) {
        case SHT_NULL:
            printf(" NULL\n");
            break;
        case SHT_PROGBITS:
            printf(" PROGBITS\n");
            break;
        case SHT_SYMTAB:
            printf(" SYMTAB\n");
            break;
        case SHT_STRTAB:
            printf(" STRTAB\n");
            break;
        case SHT_RELA:
            printf(" RELA\n");
            break;
        case SHT_HASH:
            printf(" HASH\n");
            break;
        case SHT_DYNAMIC:
            printf(" DYNAMIC\n");
            break;
        case SHT_NOTE:
            printf(" NOTE\n");
            break;
        case SHT_NOBITS:
            printf(" NOBITS\n");
            break;
        case SHT_REL:
            printf(" REL\n");
            break;
        case SHT_SHLIB:
            printf(" SHLIB\n");
            break;
        case SHT_DYNSYM:
            printf(" DYNSYM\n");
            break;
        case SHT_INIT_ARRAY:
            printf(" INIT_ARRAY\n");
            break;
        case SHT_FINI_ARRAY:
            printf(" FINI_ARRAY\n");
            break;
        case SHT_PREINIT_ARRAY:
            printf(" PREINIT_ARRAY\n");
            break;
        case SHT_GROUP:
            printf(" GROUP\n");
            break;
        case SHT_SYMTAB_SHNDX:
            printf(" SYMTAB_SHNDX\n");
            break;
        default:
            printf(" Other:%x\n", s1->sh_type);
            break;
    }
    printf("\n");
#endif
}

/*
 *  ======== print_proghdr_info ========
 */
static void print_proghdr_info(struct Elf32_Phdr *p)
{
#if DEBUG_GENEXTELF
    printf("0x%.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x 0x%.6x ", p->p_offset,
                p->p_vaddr, p->p_paddr, p->p_filesz, p->p_memsz, p->p_align);

    (p->p_flags & PF_R) ? printf("R") : printf(" ");
    (p->p_flags & PF_W) ? printf("W") : printf(" ");
    (p->p_flags & PF_X) ? printf("X") : printf(" ");

    switch (p->p_type) {
        case PT_NULL:
            printf(" NULL\n");
            break;
        case PT_LOAD:
            printf(" LOAD\n");
            break;
        case PT_DYNAMIC:
            printf(" DYNAMIC\n");
            break;
        case PT_NOTE:
            printf(" NOTE\n");
            break;
        case PT_SHLIB:
            printf(" SHLIB\n");
            break;
        case PT_PHDR:
            printf(" PHDR\n");
            break;
        case PT_TLS:
            printf(" TLS\n");
            break;
        default:
            printf(" Other:%d\n", p->p_type);
            break;
    }
#endif
}
