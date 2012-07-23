#
# Copyright (c) 2011-2012, Texas Instruments Incorporated
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# *  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# *  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# *  Neither the name of Texas Instruments Incorporated nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Repo
BIOSTOOLSROOT   ?= /opt/ti
REPO            := $(BIOSTOOLSROOT)

# Customizable version variables - export them or pass as arguments to make
XDCVERSION      ?= xdctools_3_23_03_53
BIOSVERSION     ?= bios_6_33_06_50
IPCVERSION      ?= ipc_1_24_02_27

ifeq (bldcfg.mk,$(wildcard bldcfg.mk))
include bldcfg.mk
endif

BIOSPROD        = $(REPO)/$(BIOSVERSION)
IPCPROD         = $(REPO)/$(IPCVERSION)
XDCPROD         = $(REPO)/$(XDCVERSION)

BUILD_SMP       ?= 0

export XDCROOT  = $(XDCPROD)
export XDCPATH  = $(BIOSPROD)/packages;$(IPCPROD)/packages;./src;

all:
	$(XDCROOT)/xdc -k -j $(j) BUILD_SMP=$(BUILD_SMP) -P `$(XDCROOT)/bin/xdcpkg src/ti |  egrep -v -e "/tests|/apps" | xargs`

clean:
	$(XDCROOT)/xdc clean BUILD_SMP=$(BUILD_SMP) -Pr src

smp_config: unconfig
	@echo BIOSVERSION=smpbios_1_00_00_25_eng >> bldcfg.mk
	@echo BUILD_SMP=1 >> bldcfg.mk

unconfig:
ifeq (bldcfg.mk,$(wildcard bldcfg.mk))
	@rm bldcfg.mk
endif

info: tools
tools:
	@echo "REPO   := $(REPO)"
	@echo "XDC    := $(XDCPROD)"
	@echo "BIOS   := $(BIOSPROD)"
	@echo "IPC    := $(IPCPROD)"
	@echo "ARMCGT := $(TMS470CGTOOLPATH)"
	@echo "C6xCGT := $(C6000CGTOOLPATH)"
