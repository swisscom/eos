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

define _CLEAR_VARS
$(eval 
SRCS:=
OBJS:=
DEPS:=
CFLAGS:=
CXXFLAGS:=
LDFLAGS:=
STATIC_LIBS:=
WHOLE_STATIC_LIBS:=
INCLUDE_DIRS:=
SHARED_LIBS:=
)
endef

LOCAL_PATH:= $(call my-dir)
include $(LOCAL_PATH)/EOS_BUILD.mk
include $(CLEAR_VARS)
$(call _CLEAR_VARS)

WORK_DIR := $(LOCAL_PATH)/
SRCDIR := $(LOCAL_PATH)/src
EXTDIR := $(WORK_DIR)/ext/

LOCAL_MODULE := libeos
LOCAL_MODULE_TAGS := optional

CPLUS_INCLUDE_PATH:=
C_INCLUDE_PATH:=

LOCAL_C_INCLUDES := $(SRCDIR)/common/
LOCAL_CFLAGS := -DEOS_ALLOW_UNUSED -DEOS_DEBUG -DEOS_LOG_ALL -DEOS_VERSION_COMMIT=$(EOS_VERSION_COMMIT)

LOCAL_CONLYFLAGS += -std=c99
LOCAL_LDFLAGS += -rdynamic -Wl,-E

include $(LOCAL_PATH)/config/config_android.mk
include $(EXTDIR)/ext.mk
include $(SRCDIR)/system/system.mk
include $(SRCDIR)/stream/stream.mk
include $(SRCDIR)/core/core.mk
include $(SRCDIR)/api/api.mk
LOCAL_SRC_FILES := $(SRCS:$(LOCAL_PATH)%=%)

LOCAL_SHARED_LIBRARIES += libcutils liblog

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
JAVADIR = $(LOCAL_PATH)/src/java

LOCAL_MODULE := libeos_jni
LOCAL_MODULE_TAGS := optional

include $(SRCDIR)/system/system.mk
LOCAL_C_INCLUDES += $(SRCDIR)/common/ $(APIDIR)/ $(LOCAL_PATH)/ext/lib/ 

LOCAL_SRC_FILES := src/java/com_swisscom_eos_EosNativeBridge.c

LOCAL_CFLAGS += -DEOS_ALLOW_UNUSED -DEOS_VERSION_COMMIT=$(EOS_VERSION_COMMIT)

LOCAL_LDFLAGS += -rdynamic -Wl,-E
LOCAL_SHARED_LIBRARIES := libeos

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
TESTDIR = $(LOCAL_PATH)/test

LOCAL_MODULE := eos_test
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES := $(SRCDIR)/common/ $(APIDIR)/ $(LOCAL_PATH)/ext/lib/ $(LOCAL_PATH)/ext/lib/linenoise

LOCAL_SRC_FILES := test/eos_test.c
SRCS += $(TESTDIR)/eos_test.c

LOCAL_CFLAGS += -DEOS_ALLOW_UNUSED -DEOS_VERSION_COMMIT=$(EOS_VERSION_COMMIT)

LOCAL_LDFLAGS += -rdynamic -Wl,-E
LOCAL_SHARED_LIBRARIES := libeos

include $(BUILD_EXECUTABLE)
