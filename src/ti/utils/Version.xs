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
 *  ======== Version.xs ========
 *
 */

var Version;
var BIOS;

/*
 *  ======== module$use ========
 */
function module$use()
{
    Version = this;
    BIOS = xdc.useModule('ti.sysbios.BIOS');

    /* Allocate .version section in the segment specified by loadSegment */
    Program.sectMap[".version"] = new Program.SectionSpec();
    Program.sectMap[".version"].type = "COPY";

    Version.imageInfo = getImageInfo();
    Version.branch = getBranch();
    Version.topCommit = getTopCommit();
    Version.tag = getTag();
    Version.tools = getTools(xdc.curPath());

    var rpmsgpath = getRpmsgPath();
    Version.isRpmsg = isRpmsg();
    if (rpmsgpath != "") {
        Version.branchRpmsg = getBranch(rpmsgpath);
        Version.topCommitRpmsg = getTopCommit(rpmsgpath);
        Version.tagRpmsg = getTag(rpmsgpath);
        Version.toolsRpmsg = getTools(getRpmsgDeps(rpmsgpath));
    }
}

/*
 *  ======== module$meta$init ========
 */
function module$static$init(obj, params)
{
    obj.pVersion = null;
}

function getImageInfo()
{
    var begin;
    var tool;
    var image;
    var targetDir = Program.build.target.rootDir;
    /*
     * Use the full path for image name for now. If there are any objections,
     * use just the Program name (with optional stripping out leading string
     * before last path separator.
     */
    var imageName = xdc.getPackageBase(xdc.om.$homepkg) + Program.name;

    begin = targetDir.lastIndexOf("/");
    if (begin != -1) {
        tool = targetDir.substr(begin + 1);
    }

    image = "\\nname     : ";
    image += imageName;
    image += "\\nplatform : ";
    image += Program.platformName;
    image += isSmp() ? " (SMP)" : " (UP)";
    image += "\\ncompiler : ";
    image += tool;

    return (image);
}

function getBranch(directory)
{
    var ret;
    var status = {};

    if (directory) {
        ret = xdc.exec("git --git-dir=" + directory + "/.git --work-tree=" +
                        directory + " branch", {merge: false}, status);
    }
    else {
        ret = xdc.exec("git branch", {merge: false}, status);
    }

    if (ret == -1 || !status.output) {
        print("WARNING: Version: no git-branch info available\n");
        return ("");
    }

    var lines = status.output.split('\n');
    for (var i = 0; i < lines.length - 1; i++) {
        var line = lines[i];
        if (line.indexOf('*') != -1){
            var branch = line.substr(2);
            if (branch.length) {
                return (branch);
            }
        }
    }

    return ("");
}

function getTopCommit(directory)
{
    var ret;
    var status = {};

    if (directory) {
        ret = xdc.exec("git --git-dir=" + directory + "/.git --work-tree=" +
                        directory + " log --pretty=\"%h - %s\" -n1",
                        {merge: false}, status);
    }
    else {
        ret = xdc.exec("git log --pretty=\"%h - %s\" -n1",
                        {merge: false}, status);
    }

    if (ret == -1 || !status.output) {
        print("WARNING: Version: no git-log info available\n");
        return ("");
    }

    var lines = status.output.split('\n');
    var commit = lines[0];
    if (commit.length) {
        return (commit)
    }

    return ("");
}

function getTag(directory)
{
    var ret;
    var status = {};

    if (directory) {
        ret = xdc.exec("git --git-dir=" + directory + "/.git --work-tree=" +
                        directory + " describe --tags --dirty",
                        {merge: false}, status);
    }
    else {
        ret = xdc.exec("git describe --tags --dirty", {merge: false}, status);
    }

    if (ret == -1 || !status.output) {
        print("WARNING: Version: no git-describe info available\n");
        return ("");
    }

    var lines = status.output.split('\n');
    var tag  = lines[0];
    if (tag.length) {
        return (tag);
    }

    return ("");
}

function getRpmsgDeps(rpmsgpath)
{
    var fname;
    var br;
    var dep;

    if (Program.build.target.name.match(/64T/)) {
        fname = rpmsgpath + "/src/ti/ipc/rpmsg/package/lib/release/"
                          + "ti.ipc.rpmsg/MessageQCopy.oe64T.dep"
    }
    else {
        if (isSmp()) {
            fname = rpmsgpath + "/src/ti/ipc/rpmsg/package/lib/release/"
                              + "ti.ipc.rpmsg_smp/MessageQCopy.oem3.dep"
        }
        else {
            fname = rpmsgpath + "/src/ti/ipc/rpmsg/package/lib/release/"
                              + "ti.ipc.rpmsg/MessageQCopy.oem3.dep"
        }
    }

    var file = new java.io.File(fname);
    br = new java.io.BufferedReader(new java.io.FileReader(file.toString()));
    if (file.isFile() && file.exists()) {
        dep = br.readLine();
    }

    return (dep);
}

function getTools(envpath)
{
    var begin, end;
    var tools = ["/include", "/packages"];
    var tool, foundTools = "";

    for (var i = 0; i < tools.length; i++) {
        while (envpath) {
            end = envpath.lastIndexOf(tools[i]);
            if (end == -1) {
                break;
            }
            tool = envpath.substr(0, end);
            begin = tool.lastIndexOf("/");
            if (begin != -1) {
                tool = tool.substr(begin + 1);
                envpath = envpath.substr(0, end);
            }
            if (!foundTools.match(tool)) {
                foundTools = foundTools + "\\n\\t   " + tool;
            }
        }
    }

    return (foundTools);
}

function getRpmsgPath()
{
    var curPkg = xdc.om.$curpkg;
    var path = xdc.getPackageRepository("" + curPkg + "");
    var end = path.search("/src");
    path = path.substr(0, end);

    return (path);
}

function isRpmsg()
{
    var curPkg = xdc.om.$curpkg;
    var homePkg = xdc.om.$homepkg;
    var curRepo = xdc.getPackageRepository("" + curPkg + "");
    var homeRepo = xdc.getPackageRepository("" + homePkg + "");

    if (curRepo == homeRepo) {
        return (true);
    }

    return (false);
}

function isSmp()
{
    if (BIOS.smpEnabled == true) {
        return (true);
    }

   return (false);
}
