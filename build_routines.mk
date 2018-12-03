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

define CLRF


endef
define TAB
	
endef

define CXX_COMPILE_RULE
$(1)/$(notdir $(basename $(2))).o : $(2)
	@echo '[Compile] $$<'
	$$(AT)-$(MKDIR) "$$(@D)"
	$$(AT)$(CXX) $(CXXFLAGS) -c -fmessage-length=0 -MMD -MF"$$(@:%.o=%.d)" -MT"$$(@)"  "$$<" -o "$$@"
OBJS+=$(1)/$(notdir $(basename $(2))).o
DEPS+=$(1)/$(notdir $(basename $(2))).d
endef

define C_COMPILE_RULE
$(1)/$(notdir $(basename $(2))).o : $(2)
	@echo '[Compile] $$<'
	$$(AT)-$(MKDIR) "$$(@D)"
	$$(AT)$(CC) $(CFLAGS) -c -fmessage-length=0 -MMD -MF"$$(@:%.o=%.d)" -MT"$$(@)"  "$$<" -o "$$@"
OBJS+=$(1)/$(notdir $(basename $(2))).o
DEPS+=$(1)/$(notdir $(basename $(2))).d
endef

define GENERATE_COMPILE_RULES
$(foreach file,$(SRCS),$(if $(filter %.c,$(file)),$(eval 
$(call C_COMPILE_RULE,$(1),$(file))),$(if $(filter %.cpp,$(file)),$(eval 
$(call CXX_COMPILE_RULE,$(1),$(file))))))
endef


define GENERATE_SHARED_LIB_RULE
$(eval 
$(1)/$(2): $(OBJS) $(3)
	@echo [Build shared library]: $$(@F)
	$$(AT)-$(MKDIR) "$$(@D)"
	$$(AT)$$(CXX) -shared -o $$@ $(OBJS) $(LDFLAGS)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif
TARGETS+=$(1)/$(2)
)
endef


define GENERATE_EXECUTABLE_RULE
$(eval 
$(1)/$(2): $(OBJS) $(3)
	@echo [Build executable]: $$(@F)
	$$(AT)-$(MKDIR) "$$(@D)"
	$$(AT)$$(CXX) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $$@
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif
TARGETS+=$(1)/$(2)
)
endef

define CLEAR_VARS
$(eval 
SRCS=
OBJS=
DEPS=
CFLAGS=
CXXFLAGS=
LDFLAGS=
)
endef

define INSTALL_PREBUILT
$(eval 
$(BINDIR)/$(notdir $(1)):
	@echo [Copy prebuilt]: $$(@F)
	$$(AT)-$(MKDIR) "$$(@D)"
	$$(AT)$$(CP) $(1) $(BINDIR)/$(notdir $(1))

TARGETS+=$(BINDIR)/$(notdir $(1))
)
endef
