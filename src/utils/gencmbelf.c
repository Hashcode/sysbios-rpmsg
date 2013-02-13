/*
 *  Copyright (c) 2013, Texas Instruments Incorporated
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
 *  ======== gencmbelf.c ========
 *  Version History:
 *  1.00 - Original Version
 *  1.01 - Updated to fixup Core1 trace entry in resource table
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

/* Map types defined in rsc_types.h */
#define UInt32 unsigned int
#define Char char
#include "../ti/resources/rsc_types.h"
#include "rprcfmt.h"

#include "elfload/include/elf32.h"

#define VERSION             "1.01"
#define MAX_TAGS            10

/* String definitions for ELF S-Section */
#define ELF_S_SECT_RESOURCE ".resource_table"
#define ELF_SHDR_STACK      ".stack"
#define ELF_SHDR_VERSION    ".version"
#define ELF_SHDR_SYMTAB     ".symtab"
#define ELF_SHDR_STRTAB     ".strtab"
#define ELF_SHDR_SHSTRTAB   ".shstrtab"

/* Core1 resource entry name for trace */
#define CORE1_TRACENAME     "trace:appm3"

/* Define the ARM attributes section type */
#define SHT_ARMATTRIBUTES  0x70000003

/* Debug macros */
#define DEBUG_LOG_FILE          "elf_combine_dbg.log"
#define DEBUG_GENCMBELF         0

#if DEBUG_GENCMBELF
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
    FILE *fp;
    u32 size;
    struct Elf32_Ehdr *e_hdr;
    struct Elf32_Phdr *p_hdrs;
    struct Elf32_Shdr *s_hdrs;
    u32 last_phoffset;
    u32 last_shoffset;
    u32 symtab_size;
    u32 strtab_size;
    u32 shstrtab_size;
    u32 version_size;
    u32 rsc_tbl_section;
    void *data;

    /* special sections */
    void *version;
    void *symtab;
    void *strtab;
    void *shstrtab;
    void *rsc_table;
} elf_file_info;

typedef enum {
    INPUT_FILE,
    OUTPUT_FILE,
} file_type;

static elf_file_info core0_info;
static elf_file_info core1_info;
static elf_file_info cores_info;

static unsigned int tag_addr[MAX_TAGS];
static unsigned int tag_size[MAX_TAGS];
static char *tag_name[MAX_TAGS];
static int num_tags;

/* Local functions */
static int prepare_file(char * filename, file_type type, elf_file_info *f_info);
static void finalize_file(elf_file_info *f_info, int status);
static int check_image(void * data, int size);
static int read_headers(elf_file_info *f_info);
static int allocate_headers(elf_file_info *f_info);
static int process_image();
static void print_symbol_info(struct Elf32_Sym *sym);
static void print_elfhdr_info(struct Elf32_Ehdr *h);
static void print_secthdr_info(struct Elf32_Shdr *s1);
static void print_proghdr_info(struct Elf32_Phdr *p1);


static void print_help_and_exit(void)
{
    fprintf(stderr, "\nInvalid arguments!\n"
                  "Usage: ./gencmbelf <[-s] <Core0 ELF image>> "
                  "<[-a] <Core1 ELF image>> <[-o] <Output ELF image>> "
                  "[<arg1:addr:size> .. <arg10:addr:size>]\n"
                  "Rules: - All files are mandatory, and need to be unique.\n"
                  "       - Arguments can be specified in any order as long\n"
                  "         as the corresponding option is specified.\n"
                  "       - All arguments not preceded by an option are \n"
                  "         applied as Core0, Core1 and output images in that\n"
                  "         order if they have .xem3 extension, otherwise\n"
                  "         they are considered as special token arguments.\n"
                  "       - The special token arguments need to be in the\n"
                  "         format of <token_name>:<token_addr>:<token_size>.\n"
                  "       - Upto 10 additional token arguments can be passed.\n"
                  "Note : - The gencmbelf utility is provided to combine two\n"
                  "         ARM ELF files into a single semi-standard ARM ELF\n"
                  "         file for the M3/M4 processors. The input files for\n"
                  "         the tool are expected to be stripped of any debug\n"
                  "         symbols, and has an utmost minimal symbol table.\n"
                  "         The generated ELF file may have duplicate section\n"
                  "         names, and the symbol table may have inaccurate.\n"
                  "         Please continue to use the individual ELF files\n"
                  "         with a IDE debugger.\n");
    exit(1);
}

/*
 *  ======== main ========
 */
int main(int argc, char * argv[])
{
    int status = 0;
    struct stat st;
    u32 size = 0;
    int i, j, o;
    char *elf_files[] = {NULL, NULL, NULL};
    int num_files = sizeof(elf_files) / sizeof(elf_files[0]);
    char *tokenstr;

    printf("###############################################################\n");
    printf("                     GENCMBELF : %s    \n", VERSION);
    printf("###############################################################\n");

    /* process arguments */
    while ((o = getopt (argc, argv, ":s:a:o:")) != -1) {
        switch (o) {
            case 's':
                elf_files[0] = optarg;
                break;
            case 'a':
                elf_files[1] = optarg;
                break;
            case 'o':
                elf_files[2] = optarg;
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

    for (i = 0, j = optind; j < argc; j++) {
        while (i < num_files && elf_files[i]) {
            i++;
        }
        if (strstr(argv[j], ".xem3")) {
            if (i == num_files) {
                print_help_and_exit();
            }
            elf_files[i++] = argv[j];
        }
        else {
            if (num_tags == MAX_TAGS) {
                print_help_and_exit();
            }
            tag_name[num_tags] = strtok(argv[j], ":");
            tokenstr = strtok(NULL, ":");
            if (!tokenstr) {
                print_help_and_exit();
            }
            tag_addr[num_tags] = strtoll(tokenstr, NULL, 16);
            tokenstr = strtok(NULL, ":");
            if (!tokenstr) {
                print_help_and_exit();
            }
            tag_size[num_tags] = strtoll(tokenstr, NULL, 16);

            DEBUG_PRINT("found tag %d: name '%s' addr 0x%x size %d\n", num_tags,
                    tag_name[num_tags], tag_addr[num_tags], tag_size[num_tags]);
            num_tags++;
        }
    }

    if (status || !elf_files[0] || !elf_files[1] || !elf_files[2]) {
        print_help_and_exit();
    }

    if ((!strcmp(elf_files[0], elf_files[1])) ||
        (!strcmp(elf_files[0], elf_files[2])) ||
        (!strcmp(elf_files[1], elf_files[2]))) {
        print_help_and_exit();
    }

    DEBUG_PRINT("\nCore0 File: %s, Core1 File: %s, Output File: %s\n",
                                    elf_files[0], elf_files[1], elf_files[2]);
    status = prepare_file(elf_files[0], INPUT_FILE, &core0_info);
    if (status) {
        printf("\nError preparing file: %s\n", elf_files[0]);
        goto finish0;
    }

    status = prepare_file(elf_files[1], INPUT_FILE, &core1_info);
    if (status) {
        printf("\nError preparing file: %s\n", elf_files[1]);
        goto finish1;
    }

    status = prepare_file(elf_files[2], OUTPUT_FILE, &cores_info);
    if (status) {
        printf("\nError preparing file: %s\n", elf_files[2]);
        goto done;
    }

    status = process_image();
    if (status) {
        printf("\nError generating output file: %s\n", elf_files[2]);
        goto done;
    }
    rewind(cores_info.fp);
    fstat(fileno(cores_info.fp), &st);
    size = st.st_size;

done:
    fclose(cores_info.fp);
    if (cores_info.data) {
        free(cores_info.data);
    }
    cores_info.fp = NULL;
    cores_info.data = NULL;
finish1:
    printf("\nFinalizing input ELF file: %s of size: %d\n", elf_files[1],
                core1_info.size);
    finalize_file(&core1_info, status);
    status = 0;
finish0:
    printf("Finalizing input ELF file: %s of size: %d\n", elf_files[0],
                core0_info.size);
    finalize_file(&core0_info, status);

    if (size) {
        printf("\nProcessed Output ELF file: %s of size: %d\n\n", elf_files[2],
                        size);
    }

    return status;
}

/*
 *  ======== prepare_file ========
 */
static int prepare_file(char * file_name, file_type type, elf_file_info *f_info)
{
    struct stat st;
    int status = 0;
    char *access = ((type == INPUT_FILE) ? "r+b" : "w+b");

    if ((f_info->fp = fopen(file_name, access)) == NULL) {
        printf("could not open: %s\n", file_name);
        status = 2;
        goto exit;
    }

    if (type == INPUT_FILE) {
        fstat(fileno(f_info->fp), &st);
        f_info->size = st.st_size;
        if (!f_info->size) {
            printf("size is invalid: %s\n", file_name);
            status = 3;
            goto exit;
        }

        printf("\nPreparing Input ELF file: %s of size: %d\n", file_name,
                    f_info->size);
        f_info->data = malloc(f_info->size);
        if (!f_info->data) {
            printf("allocation failed for reading %s\n", file_name);
            status = 4;
            goto exit;
        }
        fread(f_info->data, 1, f_info->size, f_info->fp);

        status = check_image(f_info->data, f_info->size);
        if (status) {
            printf("Image check failed for %s\n", file_name);
            status = 5;
            goto exit;
        }

        status = read_headers(f_info);
        if (status) {
            printf("read_headers failed for %s\n", file_name);
            status = 6;
            goto exit;
        }
        printf("Preparation complete for %s\n", file_name);
    }
    else if (type == OUTPUT_FILE) {
        printf("\nPreparing Output ELF file: %s\n", file_name);
        status = allocate_headers(f_info);
        if (status) {
            printf("allocate_headers failed for %s\n", file_name);
            status = 7;
            goto exit;
        }
        printf("Preparation complete for %s\n", file_name);
    }

exit:
    return status;
}

/*
 *  ======== finalize_file ========
 */
static void finalize_file(elf_file_info *f_info, int status)
{
    switch (status) {
        case 0:
        case 5:
            free(f_info->data);
        case 4:
        case 3:
            fclose(f_info->fp);
        case 2:
            f_info->data = NULL;
            f_info->fp = NULL;
            break;
    }
}

/*
 *  ======== check_image ========
 */
static int check_image(void * data, int size)
{
    struct Elf32_Ehdr *hdr;
    struct Elf32_Shdr *s;
    int i;
    int offset;
    static int count = 0;

    printf("Check image....\n");
    printf("Checking header....");
    hdr = (struct Elf32_Ehdr *)data;

    /* check for correct ELF magic numbers in file header */
    if ((!hdr->e_ident[EI_MAG0] == ELFMAG0) ||
        (!hdr->e_ident[EI_MAG1] == ELFMAG1) ||
        (!hdr->e_ident[EI_MAG2] == ELFMAG2) ||
        (!hdr->e_ident[EI_MAG3] == ELFMAG3)) {
        printf("invalid ELF magic number\n");
        return -1;
    }

    if (hdr->e_type != ET_EXEC) {
        printf("invalid ELF file type\n");
        return -1;
    }

    if (hdr->e_machine != EM_ARM) {
        printf("ELF file is not for ARM processor\n");
        return -1;
    }
    printf("done.\n");

    printf("Checking file size....");
    if ((size < (hdr->e_phoff + (hdr->e_phentsize * hdr->e_phnum))) ||
        (size < (hdr->e_shoff + (hdr->e_shentsize * hdr->e_shnum)))) {
        printf("file size is invalid\n");
        return -1;
    }
    printf("done.\n");

    core0_info.rsc_tbl_section = (u32)(-1);
    if (!count) {
        printf("Checking for %s section....", ELF_S_SECT_RESOURCE);
        /* get the string name section offset */
        s = (struct Elf32_Shdr *)(data + hdr->e_shoff);
        s += hdr->e_shstrndx;
        offset = s->sh_offset;

        s = (struct Elf32_Shdr *)(data + hdr->e_shoff);
        for (i = 0; i < hdr->e_shnum; i++, s++) {
            if (strcmp((data + offset + s->sh_name), ELF_S_SECT_RESOURCE) == 0) {
                core0_info.rsc_tbl_section = i;
                break;
            }
        }
        if (i == hdr->e_shnum) {
            printf("file does not have %s section\n", ELF_S_SECT_RESOURCE);
            return -1;
        }
        printf("done.\n");
    }
    count++;
    printf("Image check complete.\n");

    return 0;
}

/*
 *  ======== read_headers ========
 */
static int read_headers(elf_file_info *f_info)
{
    int i;
    struct Elf32_Ehdr *hdr;
    struct Elf32_Phdr *p;
    struct Elf32_Shdr *s;
    int shstr_offset = 0;

    printf("\nReading headers....");
    hdr = f_info->e_hdr = (struct Elf32_Ehdr *)f_info->data;
    print_elfhdr_info(hdr);
    f_info->p_hdrs = (struct Elf32_Phdr *)
                        calloc(hdr->e_phnum, sizeof(struct Elf32_Phdr));
    if (!f_info->p_hdrs) {
        printf("allocation of program headers failed\n");
        return -1;
    }
    memcpy((void *)f_info->p_hdrs, f_info->data + hdr->e_phoff,
                hdr->e_phnum * (sizeof(struct Elf32_Phdr)));

    p = (struct Elf32_Phdr *)(f_info->data + hdr->e_phoff) + hdr->e_phnum - 1;
    for (i = hdr->e_phnum; i > 0; i--, p--) {
        if (p->p_filesz) {
            f_info->last_phoffset = p->p_offset + p->p_filesz;
            DEBUG_PRINT("program section data end offset = 0x%x\n",
                                                f_info->last_phoffset);
            break;
        }
    }

/*
    p = (struct Elf32_Phdr *)(f_info->data + hdr->e_phoff);
    for (i = 0; ; i < hdr->e_phnum; i++, p++) {
        print_proghdr_info(p);
    }

    for (i = 0; i < hdr->e_phnum; i++) {
        print_proghdr_info(f_info->p_hdrs + i);
    }
*/

    f_info->s_hdrs = (struct Elf32_Shdr *)
                        calloc(hdr->e_shnum, sizeof(struct Elf32_Shdr));
    if (!f_info->s_hdrs) {
        printf("allocation of section headers failed.\n");
        return -1;
    }
    memcpy((void *)f_info->s_hdrs, f_info->data + hdr->e_shoff,
                hdr->e_shnum * (sizeof(struct Elf32_Shdr)));

    f_info->version_size = 0;
    f_info->version = NULL;

    s = (struct Elf32_Shdr *)(f_info->data + hdr->e_shoff);
    s += f_info->e_hdr->e_shstrndx;
    shstr_offset = s->sh_offset;
    s = (struct Elf32_Shdr *)(f_info->data + hdr->e_shoff);
    for (i = 0; i < hdr->e_shnum; i++) {
        if (i == f_info->rsc_tbl_section) {
            f_info->rsc_table = f_info->data + s->sh_offset;
        }
        if ((s->sh_type == SHT_PROGBITS) &&
            (!strcmp(f_info->data + shstr_offset + s->sh_name,
                                                        ELF_SHDR_VERSION))) {
            f_info->version_size = s->sh_size;
            f_info->version = f_info->data + s->sh_offset;
        }
        else if (s->sh_type == SHT_SYMTAB) {
            f_info->symtab_size = s->sh_size;
            f_info->symtab = f_info->data + s->sh_offset;
        }
        else if (s->sh_type == SHT_STRTAB) {
            if (!strcmp(f_info->data + shstr_offset + s->sh_name,
                                                        ELF_SHDR_STRTAB)) {
                f_info->strtab_size = s->sh_size;
                f_info->strtab = f_info->data + s->sh_offset;
            }
            else if (!strcmp(f_info->data + shstr_offset + s->sh_name,
                                                        ELF_SHDR_SHSTRTAB)) {
                f_info->shstrtab_size = s->sh_size;
                f_info->shstrtab = f_info->data + s->sh_offset;
            }
        }

        s++;
    }

/*
    s = (struct Elf32_Shdr *)(f_info->data + hdr->e_shoff);
    for (i = 0; i < hdr->e_shnum; i++, s++) {
        print_secthdr_info(s);
    }

    for (i = 0; i < hdr->e_shnum; i++) {
        print_secthdr_info(f_info->s_hdrs + i);
    }
*/

    printf("done.\n");

    return 0;
}

/*
 *  ======== allocate_headers ========
 */
static int allocate_headers(elf_file_info *f_info)
{
    printf("Allocating headers....");
    f_info->e_hdr = (struct Elf32_Ehdr *)calloc(sizeof(struct Elf32_Ehdr), 1);
    if (!f_info->e_hdr) {
        printf("allocation failed for output header.\n");
        return -1;
    }

    f_info->p_hdrs = (struct Elf32_Phdr *)calloc(core0_info.e_hdr->e_phnum +
                        core1_info.e_hdr->e_phnum, sizeof(struct Elf32_Phdr));
    if (!f_info->p_hdrs) {
        printf("allocation of program headers failed.\n");
        return -1;
    }

    f_info->s_hdrs = (struct Elf32_Shdr *)calloc(core0_info.e_hdr->e_shnum +
                        core1_info.e_hdr->e_shnum, sizeof(struct Elf32_Shdr));
    if (!f_info->s_hdrs) {
        printf("allocation of section headers failed.\n");
        return -1;
    }

    f_info->version_size = 0;
    f_info->version = calloc(core0_info.version_size +
                                    core1_info.version_size, 1);
    if (!f_info->version) {
        printf("allocation of version section memory failed.\n");
        return -1;
    }

    f_info->symtab_size = 0;
    f_info->symtab = calloc(core0_info.symtab_size + core1_info.symtab_size, 1);
    if (!f_info->symtab) {
        printf("allocation of symbol table memory failed.\n");
        return -1;
    }

    f_info->strtab_size = 0;
    f_info->strtab = calloc(core0_info.strtab_size + core1_info.strtab_size, 1);
    if (!f_info->strtab) {
        printf("allocation of string table memory failed.\n");
        return -1;
    }

    f_info->shstrtab_size = 0;
    f_info->shstrtab = calloc(core0_info.shstrtab_size +
                                    core1_info.shstrtab_size, 1);
    if (!f_info->shstrtab) {
        printf("allocation of section header symbol table memory failed.\n");
        return -1;
    }
    printf("done.\n");

    return 0;
}

/*
 *  ======== get_tag_index ========
 */
static int get_tag_index(char *name)
{
    int i, index = -1;

    for (i = 0; i < num_tags; i++) {
        if (!strcmp(tag_name[i], name)) {
                index = i;
                break;
        }
    }

    return index;
}

/*
 *  ======== fixup_rsc_table ========
 */
static void fixup_rsc_table(void *data)
{
    struct resource_table *tbl = (struct resource_table *)data;
    struct fw_rsc_trace *trace;
    bool processed = true;
    u32 *rsc_type;
    int i, tag;
#if DEBUG_GENCMBELF
    /* current resource type names as per rsc_types.h */
    char *rsc_names[6] =
            { "carveOut", "devmem", "trace", "vdev", "crashdump", "custom" };
#endif

    DEBUG_PRINT("  fixup resource table entries\n");
    for (i = 0; i < tbl->num; i++) {
        rsc_type = (u32 *)((u32)tbl + tbl->offset[i]);
        if (*rsc_type < 0 || *rsc_type > 6) {
            DEBUG_PRINT("    resource #%d is invalid\n", i);
            continue;
        }

        DEBUG_PRINT("    resource #%d of type_%s ", i, rsc_names[*rsc_type]);
        processed = true;
        if (*rsc_type == TYPE_TRACE) {
            trace = (struct fw_rsc_trace *)rsc_type;
            if (!strcmp(trace->name, CORE1_TRACENAME)) {
                processed = false;
                tag = get_tag_index("trace1");
                if (tag >= 0) {
                    DEBUG_PRINT("fixed up (updated from 0x%x:0x%x to ",
                                    trace->da, trace->len);
                    trace->da = tag_addr[tag];
                    trace->len = tag_size[tag];
                    DEBUG_PRINT("0x%x:0x%x)\n", trace->da, trace->len);
                }
                else {
                    DEBUG_PRINT("fixup failed\n");
                }
            }
        }
        if (processed) {
            DEBUG_PRINT("processed\n");
        }
    }
    DEBUG_PRINT("  resource table entries for Core1 fixed up\n");
}

/*
 *  ======== fill_zero ========
 */
static int fill_zero(int offset, int align)
{
    int fill;
    int count = 0;

    fill = align - (offset % align);
    if (fill != align) {
        DEBUG_PRINT("  fill = 0x%x co = 0x%x align = 0x%x\n", fill, offset, align);
        while (fill--) {
            fputc(0, cores_info.fp);
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
static int process_image()
{
    int i;
    struct Elf32_Ehdr *hdr;
    struct Elf32_Shdr *shdr, *s, *s1, *s2;
    struct Elf32_Shdr vers_shdr;
    struct Elf32_Phdr *phdr, *p;
    struct Elf32_Sym *sym, *sm;
    int co = 0;
    int scnt = 0;
    int len = 0;

    printf("\nStart combining images....\n\n");

    /* copy the header */
    if (core0_info.e_hdr->e_entry != core1_info.e_hdr->e_entry) {
        printf("entry points are different in both the images.\n");
        return -1;
    }
    hdr = core0_info.e_hdr;
    memcpy(cores_info.e_hdr, core0_info.e_hdr, sizeof(struct Elf32_Ehdr));
    cores_info.e_hdr->e_phnum = core0_info.e_hdr->e_phnum +
                                    core1_info.e_hdr->e_phnum;
    fwrite(cores_info.e_hdr, 1, sizeof(struct Elf32_Ehdr), cores_info.fp);
    co += sizeof(struct Elf32_Ehdr);

    /* process the program data of Core0 ELF file */
    printf("Processing Core0 program data sections....\n");
    p = core0_info.p_hdrs;
    phdr = cores_info.p_hdrs;
    for (i = 0; i < core0_info.e_hdr->e_phnum; i++, p++, phdr++) {
        DEBUG_PRINT("Program Section #%d\n", i);
        DEBUG_PRINT("  input ");
        print_proghdr_info(p);
        memcpy((void *)phdr, p, sizeof(struct Elf32_Phdr));

        /* copy the program data only if file size is non-zero */
        co += fill_zero(co, p->p_align);
        if (p->p_filesz) {
            if (core0_info.data + p->p_offset == core0_info.rsc_table) {
                fixup_rsc_table(core0_info.data + p->p_offset);
            }
            fwrite(core0_info.data + p->p_offset, 1, p->p_filesz,
                                                            cores_info.fp);
        }
        phdr->p_offset = co;
        co += p->p_filesz;
        DEBUG_PRINT("  output");
        print_proghdr_info(phdr);
        DEBUG_PRINT("  Phdr_offset = 0x%x co = 0x%x\n", phdr->p_offset, co);
    }
    printf("Core0 program data processed for %d sections.\n", i);

    /* process the program data of Core1 ELF file */
    printf("\nProcessing Core1 program data sections....\n");
    p = core1_info.p_hdrs;
    for (i = 0; i < core1_info.e_hdr->e_phnum; i++, p++, phdr++) {
        DEBUG_PRINT("Program Section #%d\n", i);
        DEBUG_PRINT("  input ");
        print_proghdr_info(p);
        memcpy((void *)phdr, p, sizeof(struct Elf32_Phdr));

        /* copy the program data only if file size is non-zero */
        co += fill_zero(co, p->p_align);
        if (p->p_filesz) {
            fwrite(core1_info.data + p->p_offset, 1, p->p_filesz,
                                                            cores_info.fp);
        }
        phdr->p_offset = co;
        co += p->p_filesz;
        DEBUG_PRINT("  output");
        print_proghdr_info(phdr);
        DEBUG_PRINT("  Phdr_offset = 0x%x co = 0x%x\n", phdr->p_offset, co);
    }
    cores_info.last_phoffset = co;
    printf("Core1 program data processed for %d sections.\n", i);

    printf("\nProcessing Core0 section data....\n");
    /* process the section data of Core0 ELF file */
    shdr = cores_info.s_hdrs;
    s = core0_info.s_hdrs;
    /* Skip the last 5 sections, these need to be combined */
    /*
     * TODO: Add check to ensure if these sections are different between the
     *       two images
     */
    for (i = 0; i < core0_info.e_hdr->e_shnum - 5; i++, s++) {
        /* skip the ".version" section, will write at the end */
        if (!strcmp(core0_info.shstrtab + s->sh_name, ELF_SHDR_VERSION)) {
            memcpy(&vers_shdr, s, sizeof(struct Elf32_Shdr));
            len = strlen(core0_info.shstrtab + s->sh_name);
            DEBUG_PRINT("[%d: %s] section strlen = %d\n",
                            i, (char *)((u32)core0_info.shstrtab + s->sh_name),
                            len);
            len = strlen(core0_info.version);
            memcpy(cores_info.version + cores_info.version_size,
                        core0_info.version, len);
            cores_info.version_size += len;
            DEBUG_PRINT("  skip processing of this section, size = %d\n",
                            s->sh_size);
            continue;
        }

        memcpy((void *)shdr, s, sizeof(struct Elf32_Shdr));
        scnt++;

        /* copy the section name into the new section string table */
        shdr->sh_name = cores_info.shstrtab_size; /* doesn't matter */
        len = strlen(core0_info.shstrtab + s->sh_name);
        DEBUG_PRINT("[%d: %s] section strlen = %d\n",
                        i, (char *)((u32)core0_info.shstrtab + s->sh_name), len);
        memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                    core0_info.shstrtab + s->sh_name, len + 1);
        DEBUG_PRINT("  check copied name [%s] new offset = %d \n",
                (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
                cores_info.shstrtab_size);
        cores_info.shstrtab_size += (len + 1);

        /* process the ".stack" section specially - no offset adjustment */
        if (!strcmp(core0_info.shstrtab + s->sh_name, ELF_SHDR_STACK)) {
            DEBUG_PRINT("  input ");
            print_secthdr_info(s);
            DEBUG_PRINT("  output");
            print_secthdr_info(shdr);
            shdr++;
            continue;
        }

        if (s->sh_offset >= core0_info.last_phoffset) {
            if ((s->sh_type == SHT_PROGBITS && s->sh_size) ||
                 (s->sh_type == SHT_ARMATTRIBUTES) ||
                 (s->sh_type == SHT_STRTAB)) {
                fwrite(core0_info.data + s->sh_offset, 1, s->sh_size,
                                                            cores_info.fp);
                shdr->sh_offset = co;
                co += s->sh_size;
            }
            else if (s->sh_type == SHT_SYMTAB) {
                co += fill_zero(co, 4);
                fwrite(core0_info.data + s->sh_offset, 1, s->sh_size,
                                                            cores_info.fp);
                shdr->sh_offset = co;
                co += s->sh_size;
            }
            else if (s->sh_type == SHT_NOBITS) {
                DEBUG_PRINT("    offsets = 0x%x 0x%x 0x%x\n",
                            cores_info.last_phoffset,
                            core0_info.last_phoffset, s->sh_offset);
                shdr->sh_offset = cores_info.last_phoffset
                                    - core0_info.last_phoffset + s->sh_offset;
            }
            DEBUG_PRINT("    sh_offset = 0x%x co = 0x%x\n", shdr->sh_offset, co);
        }

        DEBUG_PRINT("  input ");
        print_secthdr_info(s);
        DEBUG_PRINT("  output");
        print_secthdr_info(shdr);
        shdr++;
    }
    s1 = s;
    printf("Core0 section data processed for %d sections.\n", scnt);

    printf("\nProcessing Core1 section data....\n");
    /* process the section data of Core1 ELF file */
    s = core1_info.s_hdrs;

    /* Skip the last 5 sections, these need to be combined */
    /*
     * TODO: Add check to ensure if these sections are different between the
     *       two images
     */
    for (i = 0; i < core1_info.e_hdr->e_shnum - 5; i++, s++) {
        /* skip the first null section */
        if (s->sh_type == SHT_NULL) {
            continue;
        }

        /* skip the ".version" section, will write at the end */
        if (!strcmp(core1_info.shstrtab + s->sh_name, ELF_SHDR_VERSION)) {
            len = strlen(core1_info.shstrtab + s->sh_name);
            DEBUG_PRINT("[%d: %s] section strlen = %d\n",
                            i, (char *)((u32)core1_info.shstrtab + s->sh_name),
                            len);
            len = strlen(core1_info.version);
            memcpy(cores_info.version + cores_info.version_size,
                        core1_info.version, len);
            cores_info.version_size += len;
            DEBUG_PRINT("  skip processing of this section, size = %d\n",
                            s->sh_size);
            continue;
        }

        memcpy((void *)shdr, s, sizeof(struct Elf32_Shdr));
        scnt++;

        /* copy the section name into the new section string table */
        shdr->sh_name = cores_info.shstrtab_size;
        len = strlen(core1_info.shstrtab + s->sh_name);
        DEBUG_PRINT("[%d: %s] section strlen = %d\n", i,
                        (char *)((u32)core1_info.shstrtab + s->sh_name), len);
        memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                    core1_info.shstrtab + s->sh_name, len + 1);
        DEBUG_PRINT("  check copied name [%s] new offset = %d\n",
                (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
                cores_info.shstrtab_size);
        cores_info.shstrtab_size += (len + 1);

        /* process the ".stack" section specially - no offset adjustment */
        if (!strcmp(core1_info.shstrtab + s->sh_name, ELF_SHDR_STACK)) {
            DEBUG_PRINT("  input ");
            print_secthdr_info(s);
            DEBUG_PRINT("  output");
            print_secthdr_info(shdr);
            shdr++;
            continue;
        }

        if (s->sh_offset && s->sh_offset < core1_info.last_phoffset) {
            shdr->sh_offset = s->sh_offset + core0_info.last_phoffset;
        }
        else if (s->sh_offset >= core1_info.last_phoffset) {
            if ((s->sh_type == SHT_PROGBITS && s->sh_size) ||
                 (s->sh_type == SHT_ARMATTRIBUTES) ||
                 (s->sh_type == SHT_STRTAB)) {
                fwrite(core1_info.data + s->sh_offset, 1, s->sh_size,
                                                            cores_info.fp);
                shdr->sh_offset = co;
                co += s->sh_size;
            }
            else if (s->sh_type == SHT_SYMTAB) {
                co += fill_zero(co, 4);
                fwrite(core1_info.data + s->sh_offset, 1, s->sh_size,
                                                            cores_info.fp);
                shdr->sh_offset = co;
                co += s->sh_size;
            }
            else if (s->sh_type == SHT_NOBITS) {
                DEBUG_PRINT("    offsets = 0x%x 0x%x 0x%x\n",
                            cores_info.last_phoffset,
                            core1_info.last_phoffset, s->sh_offset);
                shdr->sh_offset = cores_info.last_phoffset
                                    - core1_info.last_phoffset + s->sh_offset;
            }
            DEBUG_PRINT("    sh_offset = 0x%x co = 0x%x\n", shdr->sh_offset, co);
        }

        DEBUG_PRINT("  input ");
        print_secthdr_info(s);
        DEBUG_PRINT("  output");
        print_secthdr_info(shdr);
        shdr++;
    }
    s2 = s;
    printf("Core1 section data processed for %d sections.\n", scnt);

    printf("\nWriting combined section data....\n");
    /* write the version section */
    if (cores_info.version_size) {
        DEBUG_PRINT("Process version section....\n");
        memcpy((void *)shdr, &vers_shdr, sizeof(struct Elf32_Shdr));
        scnt++;

        shdr->sh_name = cores_info.shstrtab_size;
        len = strlen(core0_info.shstrtab + vers_shdr.sh_name);
        DEBUG_PRINT("[%d: %s] section strlen = %d\n", scnt - 1,
                (char *)core0_info.shstrtab + vers_shdr.sh_name, len);
        memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                    core0_info.shstrtab + vers_shdr.sh_name, len + 1);
        DEBUG_PRINT("  check copied name [%s] new offset = %d\n",
            (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
            cores_info.shstrtab_size);
        cores_info.shstrtab_size += (len + 1);

        *(char *)(cores_info.version + cores_info.version_size) = 0;
        cores_info.version_size++;
        fwrite(cores_info.version, 1, cores_info.version_size, cores_info.fp);
        shdr->sh_offset = co;
        shdr->sh_size = cores_info.version_size;
        co += cores_info.version_size;
        shdr++;
        DEBUG_PRINT("  co = 0x%x\n", co);
    }
    printf("Version section processed.\n");

    /* write the ARM attributes section */
    if ((s1->sh_type == SHT_ARMATTRIBUTES) &&
        (s2->sh_type == SHT_ARMATTRIBUTES)/* && (s1->sh_size == s2->sh_size) &&
        (!memcmp(core0_info.data + s1->sh_offset,
                    core1_info.data + s2->sh_offset, s1->sh_size)) &&
        (!strcmp(core0_info.shstrtab + s1->sh_name,
                    core1_info.shstrtab + s2->sh_name))*/) {
        DEBUG_PRINT("\nProcess ARM attributes section....\n");
        memcpy((void *)shdr, s1, sizeof(struct Elf32_Shdr));
        scnt++;

        shdr->sh_name = cores_info.shstrtab_size;
        len = strlen(core0_info.shstrtab + s1->sh_name);
        DEBUG_PRINT("[%d: %s] section strlen = %d\n", scnt - 1,
                (char *)core0_info.shstrtab + s1->sh_name, len);
        memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                    core0_info.shstrtab + s1->sh_name, len + 1);
        DEBUG_PRINT("  check copied name [%s] new offset = %d \n",
            (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
            cores_info.shstrtab_size);
        cores_info.shstrtab_size += (len + 1);

        fwrite(core0_info.data + s1->sh_offset, 1, s1->sh_size,
                                                    cores_info.fp);
        shdr->sh_offset = co;
        co += s1->sh_size;
        shdr++;
        DEBUG_PRINT("  co = 0x%x\n", co);
    }
    else {
        printf("ARM attributes section is mismatched between the two ELF"
                "images.\n");
        return -1;
    }
    printf("ARM attributes section processed.\n");

    /* write the symtab section */
    DEBUG_PRINT("\nProcess Symbol Table section....\n");
    s1++; s2++;
    if (s1->sh_type != SHT_SYMTAB && s2->sh_type != SHT_SYMTAB &&
            (!strcmp(core0_info.shstrtab + s1->sh_name,
                        core1_info.shstrtab + s2->sh_name))) {
        printf("Expected symbol table section not found in the two ELF "
                "images.\n");
        return -1;
    }

    memcpy((void *)shdr, s1, sizeof(struct Elf32_Shdr));
    scnt++;
    co += fill_zero(co, 4);
    shdr->sh_offset = co;
    shdr->sh_name = cores_info.shstrtab_size;
    shdr->sh_link = scnt + 1;
    len = strlen(core0_info.shstrtab + s1->sh_name);
    DEBUG_PRINT("[%d: %s] section strlen = %d\n", scnt - 1,
            (char *)core0_info.shstrtab + s1->sh_name, len);
    memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                core0_info.shstrtab + s1->sh_name, len + 1);
    DEBUG_PRINT("  check copied name [%s] new offset = %d\n",
        (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
        cores_info.shstrtab_size);
    cores_info.shstrtab_size += (len + 1);

    /* combine the symtab section */
    DEBUG_PRINT("  Processing Core0 symbols....\n");
    sym = cores_info.symtab;
    sm = (struct Elf32_Sym *) (core0_info.data + s1->sh_offset);
    for (i = 0; i < s1->sh_size/s1->sh_entsize; i++, sm++) {
        memcpy(sym, sm, sizeof(struct Elf32_Sym));

        /* copy the symbol name into the new string table */
        sym->st_name = cores_info.strtab_size;
        len = strlen(core0_info.strtab + sm->st_name);
        DEBUG_PRINT("    [%d] symbol string length = %d new = %d\n", i, len,
                                                cores_info.strtab_size);
        memcpy(cores_info.strtab + cores_info.strtab_size,
                    core0_info.strtab + sm->st_name, len + 1);
        cores_info.strtab_size += (len + 1);

        DEBUG_PRINT("      input ");
        print_symbol_info(sm);
        DEBUG_PRINT("      output");
        print_symbol_info(sym);
        sym++;
    }
    DEBUG_PRINT("  Core0 symbols processed.\n");
    DEBUG_PRINT("  Processing Core1 symbols\n");
    sm = (struct Elf32_Sym *) (core1_info.data + s2->sh_offset);
    for (i = 0; i < s2->sh_size/s2->sh_entsize; i++, sm++) {
        if (sm->st_shndx == STN_UNDEF) {
            continue;
        }

        memcpy(sym, sm, sizeof(struct Elf32_Sym));

        /* copy the symbol name into the new string table */
        sym->st_name = cores_info.strtab_size;
        len = strlen(core1_info.strtab + sm->st_name);
        DEBUG_PRINT("    [%d] symbol string length = %d new = %d\n", i, len,
                                                cores_info.strtab_size);
        memcpy(cores_info.strtab + cores_info.strtab_size,
                    core1_info.strtab + sm->st_name, len + 1);
        cores_info.strtab_size += (len + 1);

        /* adjust the symbol section */
        if (sm->st_shndx < (core1_info.e_hdr->e_shnum - 5)) {
            sym->st_shndx += (core0_info.e_hdr->e_shnum - 5);
        }

        DEBUG_PRINT("      input ");
        print_symbol_info(sm);
        DEBUG_PRINT("      output");
        print_symbol_info(sym);
        shdr->sh_size += s2->sh_entsize;
        sym++;
    }
    DEBUG_PRINT("  Core1 symbols processed.\n");
    DEBUG_PRINT("  Writing symbols to file....\n");
    DEBUG_PRINT("    output strtab size = 0x%x scnt = %d\n",
                    cores_info.strtab_size, scnt);
    fwrite(cores_info.symtab, 1, shdr->sh_size, cores_info.fp);
    co += shdr->sh_size;
    shdr++;
    DEBUG_PRINT("    co = 0x%x\n", co);
    DEBUG_PRINT("  Symbols written.\n");
    printf("Symbol Table section processed.\n");

    /* write the TI section flags section */
    DEBUG_PRINT("\nProcess TI section flags section....\n");
    s1++; s2++;
    if ((s1->sh_type == SHT_PROGBITS) &&
        (s2->sh_type == SHT_PROGBITS) && (s1->sh_size == s2->sh_size) &&
        (!memcmp(core0_info.data + s1->sh_offset,
                    core1_info.data + s2->sh_offset, s1->sh_size)) &&
        (!strcmp(core0_info.shstrtab + s1->sh_name,
                    core1_info.shstrtab + s2->sh_name))) {
            memcpy((void *)shdr, s1, sizeof(struct Elf32_Shdr));
            scnt++;

            shdr->sh_name = cores_info.shstrtab_size;
            len = strlen(core0_info.shstrtab + s1->sh_name);
            DEBUG_PRINT("[%d: %s] section string length = %d\n",
                        scnt - 1, (char *)core0_info.shstrtab + s1->sh_name,
                                    len);
            memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                        core0_info.shstrtab + s1->sh_name, len + 1);
            DEBUG_PRINT("  check copied name [%s] new offset = %d \n",
                (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
                cores_info.shstrtab_size);
            cores_info.shstrtab_size += (len + 1);

            fwrite(core0_info.data + s1->sh_offset, 1, s1->sh_size,
                                                        cores_info.fp);
            shdr->sh_offset = co;
            co += s1->sh_size;
            shdr++;
            DEBUG_PRINT("  co = 0x%x\n", co);
    }
    else {
        printf("TI Section Flags section is mismatched between the two ELF"
                "images.\n");
        return -1;
    }
    printf("TI Section Flags section processed.\n");

    /* write the strtab section */
    DEBUG_PRINT("\nProcess String table section....\n");
    s1++; s2++;
    if ((s1->sh_type == SHT_STRTAB) && (s2->sh_type == SHT_STRTAB) &&
        (!strcmp(core0_info.shstrtab + s1->sh_name,
                    core1_info.shstrtab + s2->sh_name))) {
            memcpy((void *)shdr, s1, sizeof(struct Elf32_Shdr));
            scnt++;

            shdr->sh_name = cores_info.shstrtab_size;
            len = strlen(core0_info.shstrtab + s1->sh_name);
            DEBUG_PRINT("[%d: %s] section strlen = %d\n",
                scnt - 1, (char *)core0_info.shstrtab + s1->sh_name, len);
            memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                        core0_info.shstrtab + s1->sh_name, len + 1);
            DEBUG_PRINT("check copied name [%s] new offset = %d\n",
                (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
                cores_info.shstrtab_size);
            cores_info.shstrtab_size += (len + 1);

            fwrite(cores_info.strtab, 1, cores_info.strtab_size, cores_info.fp);
            shdr->sh_offset = co;
            shdr->sh_size = cores_info.strtab_size;
            co +=  cores_info.strtab_size;
            shdr++;
            DEBUG_PRINT("  co = 0x%x\n", co);
    }
    else {
        printf("String Table section is mismatched between the two ELF "
                "images.\n");
        return -1;
    }
    printf("String Table section processed.\n");

    /* write the shstrtab section */
    DEBUG_PRINT("\nProcess Section header string table section....\n");
    s1++; s2++;
    if ((s1->sh_type == SHT_STRTAB) && (s2->sh_type == SHT_STRTAB) &&
        (!strcmp(core0_info.shstrtab + s1->sh_name,
                    core1_info.shstrtab + s2->sh_name))) {
            memcpy((void *)shdr, s1, sizeof(struct Elf32_Shdr));
            scnt++;

            shdr->sh_name = cores_info.shstrtab_size;
            len = strlen(core0_info.shstrtab + s1->sh_name);
            DEBUG_PRINT("[%d: %s] section strlen = %d\n", scnt - 1,
               (char *)core0_info.shstrtab + s1->sh_name, len);
            memcpy(cores_info.shstrtab + cores_info.shstrtab_size,
                        core0_info.shstrtab + s1->sh_name, len + 1);
            DEBUG_PRINT("check copied name [%s] new offset = %d\n",
                (char *)((u32)cores_info.shstrtab + cores_info.shstrtab_size),
                cores_info.shstrtab_size);
            cores_info.shstrtab_size += (len + 1);

            fwrite(cores_info.shstrtab, 1, cores_info.shstrtab_size,
                                                                cores_info.fp);
            shdr->sh_offset = co;
            shdr->sh_size = cores_info.shstrtab_size;
            co +=  cores_info.shstrtab_size;
            DEBUG_PRINT("  co = 0x%x\n", co);
    }
    else {
        printf("String Table section is mismatched between the two ELF "
                "images.\n");
        return -1;
    }
    printf("Section Header string table processed.\n");
    printf("Combined sections data processed.\n");

    /* write the program headers */
    printf("\nWriting new program headers....");
    DEBUG_PRINT("\n  prev phoffset = 0x%x co = 0x%x\n",
                                                cores_info.e_hdr->e_phoff, co);
    cores_info.e_hdr->e_phoff = co;
    cores_info.e_hdr->e_shnum = scnt;
    cores_info.e_hdr->e_shstrndx = scnt - 1;
    fwrite(cores_info.p_hdrs, 1,
            cores_info.e_hdr->e_phnum * cores_info.e_hdr->e_phentsize,
            cores_info.fp);
    co += cores_info.e_hdr->e_phnum * cores_info.e_hdr->e_phentsize;
    printf("done.\n");

    /* write the section headers */
    printf("Writing new section headers....");
    DEBUG_PRINT("\n  prev shoffset = 0x%x co = 0x%x\n",
                                                cores_info.e_hdr->e_shoff, co);
    cores_info.e_hdr->e_shoff = co;
    fwrite(cores_info.s_hdrs, 1,
            cores_info.e_hdr->e_shnum * cores_info.e_hdr->e_shentsize,
            cores_info.fp);
    co += cores_info.e_hdr->e_shnum * cores_info.e_hdr->e_shentsize;
    printf("done.\n");

    /* rewrite the new ELF header */
    printf("Writing new ELF header....");
    fflush(cores_info.fp);
    rewind(cores_info.fp);
    fwrite(cores_info.e_hdr, 1, sizeof(struct Elf32_Ehdr), cores_info.fp);
    fflush(cores_info.fp);
    printf("done.\n");
    printf("\nCombined Output image created!\n");

    return 0;
}

/*
 *  ======== print_symbol_info ========
 */
static void print_symbol_info(struct Elf32_Sym *sym)
{
    DEBUG_PRINT("Name-Offset = 0x%.8x Value = 0x%.8x Size = 0x%.8x "
                "Bind = %d Type = %d Visibility = %d Index = %d\n",
                sym->st_name, sym->st_value, sym->st_size,
                ELF32_ST_BIND(sym->st_info), ELF32_ST_TYPE(sym->st_info),
                sym->st_other, sym->st_shndx);
}

/*
 *  ======== print_elfhdr_info ========
 */
static void print_elfhdr_info(struct Elf32_Ehdr *hdr)
{
    DEBUG_PRINT("\nMagic Number : %c%c%c%c\n", hdr->e_ident[1], hdr->e_ident[2],
                hdr->e_ident[3], hdr->e_ident[0]);
    DEBUG_PRINT("Object Type  : %d\n", hdr->e_type);
    DEBUG_PRINT("Target Proc  : %d\n", hdr->e_machine);
    DEBUG_PRINT("Object Vers  : 0x%x\n", hdr->e_version);
    DEBUG_PRINT("Header Size  : 0x%x\n", hdr->e_ehsize);
    DEBUG_PRINT("Entry Point  : 0x%x\n", hdr->e_entry);
    DEBUG_PRINT("Proc Flags   : 0x%x\n", hdr->e_type);
    DEBUG_PRINT("Entry Sizes  : %d %d\n", hdr->e_phentsize, hdr->e_shentsize);
    DEBUG_PRINT("PHdr Info    : 0x%x %d\n", hdr->e_phoff, hdr->e_phnum);
    DEBUG_PRINT("SHdr Info    : 0x%x %d\n", hdr->e_shoff, hdr->e_shnum);
    DEBUG_PRINT("StrTable Idx : %d\n", hdr->e_shstrndx);
}

/*
 *  ======== print_secthdr_info ========
 */
static void print_secthdr_info(struct Elf32_Shdr *s1)
{
    DEBUG_PRINT(" Offset = 0x%.8x Size = 0x%.8x Name-Offset = %.2d "
                "Align = 0x%.4x ", s1->sh_offset, s1->sh_size, s1->sh_name,
                                    s1->sh_addralign);

    if (s1->sh_flags & SHF_WRITE) DEBUG_PRINT("W");
    if (s1->sh_flags & SHF_ALLOC) DEBUG_PRINT("A");
    if (s1->sh_flags & SHF_EXECINSTR) DEBUG_PRINT("E");
    if (s1->sh_flags & SHF_MERGE) DEBUG_PRINT("M");
    if (s1->sh_flags & SHF_STRINGS) DEBUG_PRINT("S");
    if (s1->sh_flags & SHF_INFO_LINK) DEBUG_PRINT("IL");
    if (s1->sh_flags & SHF_LINK_ORDER) DEBUG_PRINT("LO");
    if (s1->sh_flags & SHF_OS_NONCONFORMING) DEBUG_PRINT("OS");
    if (s1->sh_flags & SHF_GROUP) DEBUG_PRINT("TLS");
    if (s1->sh_flags & SHF_MASKOS) DEBUG_PRINT("M OS");
    if (s1->sh_flags & SHF_MASKPROC) DEBUG_PRINT("Proc");

    switch (s1->sh_type) {
        case SHT_NULL:
            DEBUG_PRINT(" NULL\n");
            break;
        case SHT_PROGBITS:
            DEBUG_PRINT(" PROGBITS\n");
            break;
        case SHT_SYMTAB:
            DEBUG_PRINT(" SYMTAB\n");
            break;
        case SHT_STRTAB:
            DEBUG_PRINT(" STRTAB\n");
            break;
        case SHT_RELA:
            DEBUG_PRINT(" RELA\n");
            break;
        case SHT_HASH:
            DEBUG_PRINT(" HASH\n");
            break;
        case SHT_DYNAMIC:
            DEBUG_PRINT(" DYNAMIC\n");
            break;
        case SHT_NOTE:
            DEBUG_PRINT(" NOTE\n");
            break;
        case SHT_NOBITS:
            DEBUG_PRINT(" NOBITS\n");
            break;
        case SHT_REL:
            DEBUG_PRINT(" REL\n");
            break;
        case SHT_SHLIB:
            DEBUG_PRINT(" SHLIB\n");
            break;
        case SHT_DYNSYM:
            DEBUG_PRINT(" DYNSYM\n");
            break;
        case SHT_INIT_ARRAY:
            DEBUG_PRINT(" INIT_ARRAY\n");
            break;
        case SHT_FINI_ARRAY:
            DEBUG_PRINT(" FINI_ARRAY\n");
            break;
        case SHT_PREINIT_ARRAY:
            DEBUG_PRINT(" PREINIT_ARRAY\n");
            break;
        case SHT_GROUP:
            DEBUG_PRINT(" GROUP\n");
            break;
        case SHT_SYMTAB_SHNDX:
            DEBUG_PRINT(" SYMTAB_SHNDX\n");
            break;
        default:
            DEBUG_PRINT(" Other:%x\n", s1->sh_type);
            break;
    }
}

/*
 *  ======== print_proghdr_info ========
 */
static void print_proghdr_info(struct Elf32_Phdr *p)
{
#if DEBUG_GENCMBELF
    DEBUG_PRINT("  Offset: 0x%.8x VA: 0x%.8x PA: 0x%.8x FileSz: 0x%.8x"
                " MemSz:0x%.8x Align: 0x%.6x ", p->p_offset, p->p_vaddr,
                    p->p_paddr, p->p_filesz, p->p_memsz, p->p_align);

    (p->p_flags & PF_R) ? DEBUG_PRINT("R") : DEBUG_PRINT(" ");
    (p->p_flags & PF_W) ? DEBUG_PRINT("W") : DEBUG_PRINT(" ");
    (p->p_flags & PF_X) ? DEBUG_PRINT("X") : DEBUG_PRINT(" ");

    switch (p->p_type) {
        case PT_NULL:
            DEBUG_PRINT(" NULL\n");
            break;
        case PT_LOAD:
            DEBUG_PRINT(" LOAD\n");
            break;
        case PT_DYNAMIC:
            DEBUG_PRINT(" DYNAMIC\n");
            break;
        case PT_NOTE:
            DEBUG_PRINT(" NOTE\n");
            break;
        case PT_SHLIB:
            DEBUG_PRINT(" SHLIB\n");
            break;
        case PT_PHDR:
            DEBUG_PRINT(" PHDR\n");
            break;
        case PT_TLS:
            DEBUG_PRINT(" TLS\n");
            break;
        default:
            DEBUG_PRINT(" Other:%d\n", p->p_type);
            break;
    }
#endif
}
