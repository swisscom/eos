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

$(call CLEAR_VARS)
CFLAGS:=$(DEF_CFLAGS)
CXXFLAGS:=$(DEF_CXXFLAGS)
TEST_LDFLAGS:=-pthread -l$(EOS_NAME) $(DEF_LDFLAGS)
LDFLAGS:=$(TEST_LDFLAGS)

TESTDIR = $(WORK_DIR)/test

CFLAGS += -I$(APIDIR)/
CFLAGS += -I./ext/lib/linenoise

SRCS += $(TESTDIR)/eos_test.c

$(call GENERATE_COMPILE_RULES,$(OBJDIR))
$(call GENERATE_EXECUTABLE_RULE,$(BINDIR),eos_test)

$(call CLEAR_VARS)
CFLAGS:=$(DEF_CFLAGS)
CXXFLAGS:=$(DEF_CXXFLAGS)
TEST_LDFLAGS:=-pthread -l$(EOS_NAME) $(DEF_LDFLAGS)
LDFLAGS:=$(TEST_LDFLAGS)

TESTDIR = $(WORK_DIR)/test

CFLAGS += -I$(APIDIR)/

SRCS += $(TESTDIR)/eos_cc_test.c

$(call GENERATE_COMPILE_RULES,$(OBJDIR))
$(call GENERATE_EXECUTABLE_RULE,$(BINDIR),eos_cc_test)

include $(TESTDIR)/system/osi/osi_test.mk
include $(TESTDIR)/system/util/util_test.mk
include $(TESTDIR)/stream/stream.mk
