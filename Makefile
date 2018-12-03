# Copyright (c) 2015, Swisscom (Switzerland) Ltd.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Swisscom nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Architecture and development:
# Vladimir Maksovic <Vladimir.Maksovic@swisscom.com>
# Milenko Boric Herget <Milenko.BoricHerget@swisscom.com>
# Dario Vieceli <Dario.Vieceli@swisscom.com>


include EOS_BUILD.mk


START:=$(shell date +"%s")

#commands
RMFR := rm -rf
MKDIR := mkdir -p
CP := cp
AT := @
EOS_NAME ?= eos
LIB_PREFIX ?= lib
LIB_SUFIX ?= .so
EOS_LIB = $(LIB_PREFIX)$(EOS_NAME)$(LIB_SUFIX)

#directories

WORK_DIR := .
SRCDIR := $(WORK_DIR)/src
BINDIR = $(WORK_DIR)/bin
OBJDIR = $(WORK_DIR)/bin/obj
DESTDIR ?= $(BINDIR)/install
EXTDIR := $(WORK_DIR)/ext/
LIBDIR  ?= lib
INCDIR  ?= include

CC = $(CROSS_PREFIX)gcc
CXX = $(CROSS_PREFIX)g++
LD = $(CROSS_PREFIX)ld
AR = $(CROSS_PREFIX)ar
ARFLAGS ?= rcs

#default debug/release
DEBUG ?= 1

.PHONY: default_target all clean install showcommands debug

default_target: $(if $(filter-out showcommands debug,$(MAKECMDGOALS)),$(filter-out showcommands debug,$(MAKECMDGOALS)),all)
showcommands: AT=
showcommands: default_target
debug: DEBUG=1
debug: default_target

ifneq ($(filter showcommands,$(MAKECMDGOALS)),)
AT=
endif

ifneq ($(filter debug,$(MAKECMDGOALS)),)
DEBUG=1
endif

DEF_CFLAGS += -fPIC -Wall -Werror -Wextra -pedantic -std=c99 -DEOS_ALLOW_UNUSED -DEOS_DEBUG -DEOS_VERSION_COMMIT=$(EOS_VERSION_COMMIT)
DEF_CXXFLAGS += -fPIC -std=c++11 -DEOS_ALLOW_UNUSED -DEOS_DEBUG -DEOS_VERSION_COMMIT=$(EOS_VERSION_COMMIT)

ifeq ($(DEBUG),1)
DEF_CFLAGS  += -O0 -g3 -DEOS_LOG_ALL
DEF_CXXFLAGS  += -O0 -g3 -DEOS_LOG_ALL
else
DEF_CFLAGS  += -fPIC -O2
DEF_CXXFLAGS += -fPIC -O2
endif

DEF_LDFLAGS += -fPIC -L$(BINDIR)/

include ./build_routines.mk

LOCAL_C_INCLUDES := $(SRCDIR)/common

$(call CLEAR_VARS)
CFLAGS:=$(DEF_CFLAGS)
CXXFLAGS:=$(DEF_CXXFLAGS)
LDFLAGS:=$(DEF_LDFLAGS)
include $(WORK_DIR)/config/current.mk
include $(EXTDIR)/ext.mk
include $(SRCDIR)/system/system.mk
include $(SRCDIR)/stream/stream.mk
include $(SRCDIR)/core/core.mk
include $(SRCDIR)/api/api.mk

CFLAGS+= $(foreach include,$(LOCAL_C_INCLUDES), -I$(include) )
CXXFLAGS+= $(foreach include,$(LOCAL_C_INCLUDES), -I$(include) )

DEF_CFLAGS:=$(CFLAGS)
DEF_LDFLAGS:=$(LDFLAGS)
$(call GENERATE_COMPILE_RULES,$(OBJDIR))
$(call GENERATE_SHARED_LIB_RULE,$(BINDIR),$(EOS_LIB))

#JNI has its own lib, so add it later
ifeq ($(JAVA_BIND),1)
include $(SRCDIR)/java/java.mk
endif

#For now every test defines its own rules
include $(WORK_DIR)/test/test.mk

all: $(TARGETS)
	@echo ""
	@echo Available binaries:
	@echo $(TARGETS)
	@echo ""
	-@echo Build duration $(shell echo `date +"%s"` $(START) | awk '{print $$1 - $$2}') seconds

clean:
	$(AT)-$(RMFR) $(BINDIR)/*
	@echo Clean done

