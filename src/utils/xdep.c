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
 *  ======== xdep.c ========
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

/*
 *  ======== main ========
 */
int main(int argc, char * argv[])
{
    FILE * fp;
    struct stat st;
    char * aftoken;
    char srcname[500];
    char fname[500];
    char sname[500];
    char tool[100];
    char tools[10][100];
    char line[100];
    char oext[10];
    int len, i, lastTool = 0;
    int found;

    if (!argv[1]) {
        exit(1);
    }

    strcpy(srcname, argv[1]);
    aftoken = strstr(srcname, ".x");
    /* Switch from 'x' to 'o' in extension */
    strcpy(oext, aftoken);
    strncpy(oext, ".o", 2);
    /* Replace the '.x' with '_p' */
    strncpy(aftoken, "_p", 2);
    /* Append .dep as an additional extension */
    sprintf(srcname, "%s%s.dep", srcname, oext);

    /* Iterate until the directory that contains the dependency file is found */
    /* Search from reverse by splitting the file pathname based on '/' */
    len = strlen(srcname);
    strcpy(sname, srcname);
    do {
        /* Terminate the search string to eliminate the trailing '/' */
        *(sname + len) = 0;
        aftoken = strrchr(sname, '/');
        len = aftoken - sname;
        /* Copy left_side */
        strncpy(fname, srcname, len);
        /* Append /package/cfg */
        strcpy(fname + len, "/package/cfg");
        /* Append right_side */
        sprintf(fname, "%s%s", fname, srcname + len);
        if (stat(fname, &st) == 0) {
            break;
        }
    } while (len && aftoken != NULL);

    if ((fp = fopen(fname, "r")) == NULL) {
        exit(1);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        aftoken = strstr(line, "/packages");
        if (!aftoken) {
            aftoken = strstr(line, "/include");
        }
        if (aftoken != NULL) {
            len = aftoken - line;
            /* Copy left_side */
            strncpy(tool, line, len);
            /* Add '\n' as well as '\0' */
            strcpy(tool + len, "\n");
            /* Find last '/', we only need the last name before /packages */
            aftoken = strrchr(tool, '/');
            if (aftoken != NULL) {
                found = 0;
                for (i = 0; i < lastTool; i++) {
                    /* Check if already in the tools table */
                    if (!strcmp(tools[i], aftoken + 1)) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    /* Add to the tools table */
                    strcpy(tools[lastTool], aftoken + 1);
                    printf("\t%s", tools[lastTool]);
                    lastTool++;
                }
            }
        }
    }
    fclose(fp);
    return 0;
}
