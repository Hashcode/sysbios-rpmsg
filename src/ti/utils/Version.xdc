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
 *  ======== Version.xdc ========
 */

/*!
 *  ======== Version ========
 *  Version Module
 *
 *  Version module is used to get information regarding the tools used
 *  and the repository information from which a base-image is built.
 *  This is to aid in retrieving version information while debugging issues
 *  with a base-image.
 */

@Template("./Version.xdt")
@ModuleStartup
module Version {

    /*!
     *  ======== custom ========
     *  Overrides the default git-based version string.
     */
    config String custom = "";

    /*!
     *  ======== proc ========
     *  Name of the processor: ipu or dsp
     */
    metaonly config String imageInfo = "";

    /*!
     *  ======== topCommit ========
     *  Top commit information of the repository containing the base-image
     *  (commit id & commit message)
     */
    metaonly config String topCommit = "";

    /*!
     *  ======== branchName ========
     *  Name of the git branch of the repository containing the base-image
     */
    metaonly config String branch = "";

    /*!
     *  ======== tag ========
     *  Commit description info generated using git describe command on the
     *  repository containing the baseimage. If the latest commit is not tagged,
     *  it prints the number of additional commits on top of the last tagged
     *  commit
     */
    metaonly config String tag = "";

    /*!
     *  ======== tools ========
     *  Name and version of the XDC, BIOS and compilation tools used for
     *  building the base-image
     */
    metaonly config String tools = "";

    /*!
     *  ======== topCommitRpmsg ========
     *  Top commit information for sysbios-rpmsg (commit id & commit message)
     */
    metaonly config String topCommitRpmsg = "";

    /*!
     *  ======== branchRpmsg ========
     *  Name of the git branch of sysbios-rpmsg
     */
    metaonly config String branchRpmsg = "";

    /*!
     *  ======== tagRpmsg ========
     *  Commit description info generated using git describe command on the
     *  sysbios-rpmsg tree. If the latest commit is not tagged, it prints the
     *  number of additional commits on top of the last tagged commit.
     */
    metaonly config String tagRpmsg = "";

    /*!
     *  ======== toolsRpmsg ========
     *  Name and version of the XDC, BIOS and compilation tools used while
     *  building sysbios-rpmsg
     */
    metaonly config String toolsRpmsg = "";

    /*!
     *  ======== isRpmsg ========
     *  Flag indicating if base image is part of the sysbios-rpmsg tree. The
     *  sysbios-rpmsg and image version info are same, and duplication of info
     *  can be skipped.
     */
    metaonly config Bool isRpmsg = false;

internal:   /* not for client use */

    /*
     *  ============= init ============
     */
    Void init();

    struct Module_State {
        Char    *pVersion;  /* pointer to version section */
    };
}
