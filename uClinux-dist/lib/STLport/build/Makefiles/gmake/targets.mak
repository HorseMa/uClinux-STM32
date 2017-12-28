# Time-stamp: <03/10/27 18:22:08 ptr>
# $Id: targets.mak,v 1.1.2.3 2005/09/20 20:13:04 dums Exp $

# if sources disposed in several dirs, calculate
# appropriate rules; here is recursive call!

DIRS_UNIQUE_SRC := $(dir $(SRC_CPP) $(SRC_CC) $(SRC_C) )
ifeq (${OSNAME},cygming)
DIRS_UNIQUE_SRC += $(dir $(SRC_RC) )
endif
ifeq (${OSNAME},windows)
DIRS_UNIQUE_SRC += $(dir $(SRC_RC) )
endif
DIRS_UNIQUE_SRC := $(sort $(DIRS_UNIQUE_SRC) )
include ${RULESBASE}/${USE_MAKE}/dirsrc.mak

ALLBASE    := $(basename $(notdir $(SRC_CC) $(SRC_CPP) $(SRC_C)))
ifeq (${OSNAME},cygming)
RCBASE    += $(basename $(notdir $(SRC_RC)))
endif
ifeq (${OSNAME},windows)
RCBASE    += $(basename $(notdir $(SRC_RC)))
endif

ALLOBJS    := $(addsuffix .o,$(ALLBASE))
ALLDEPS    := $(addsuffix .d,$(ALLBASE))
ALLRESS    := $(addsuffix .res,$(RCBASE))

OBJ        := $(addprefix $(OUTPUT_DIR)/,$(ALLOBJS))
OBJ_DBG    := $(addprefix $(OUTPUT_DIR_DBG)/,$(ALLOBJS))
OBJ_STLDBG := $(addprefix $(OUTPUT_DIR_STLDBG)/,$(ALLOBJS))

DEP        := $(addprefix $(OUTPUT_DIR)/,$(ALLDEPS))
DEP_DBG    := $(addprefix $(OUTPUT_DIR_DBG)/,$(ALLDEPS))
DEP_STLDBG := $(addprefix $(OUTPUT_DIR_STLDBG)/,$(ALLDEPS))

RES        := $(addprefix $(OUTPUT_DIR)/,$(ALLRESS))
RES_DBG    := $(addprefix $(OUTPUT_DIR_DBG)/,$(ALLRESS))
RES_STLDBG := $(addprefix $(OUTPUT_DIR_STLDBG)/,$(ALLRESS))

ifeq ($(OUTPUT_DIR),$(OUTPUT_DIR_A))
OBJ_A      := $(OBJ)
DEP_A      := $(DEP)
else
OBJ_A      := $(addprefix $(OUTPUT_DIR_A)/,$(ALLOBJS))
DEP_A      := $(addprefix $(OUTPUT_DIR_A)/,$(ALLDEPS))
endif

ifeq ($(OUTPUT_DIR_DBG),$(OUTPUT_DIR_A_DBG))
OBJ_A_DBG  := $(OBJ_DBG)
DEP_A_DBG  := $(DEP_DBG)
else
OBJ_A_DBG  := $(addprefix $(OUTPUT_DIR_A_DBG)/,$(ALLOBJS))
DEP_A_DBG  := $(addprefix $(OUTPUT_DIR_A_DBG)/,$(ALLDEPS))
endif

ifeq ($(OUTPUT_DIR_STLDBG),$(OUTPUT_DIR_A_STLDBG))
OBJ_A_STLDBG := $(OBJ_STLDBG)
DEP_A_STLDBG := $(DEP_STLDBG)
else
OBJ_A_STLDBG := $(addprefix $(OUTPUT_DIR_A_STLDBG)/,$(ALLOBJS))
DEP_A_STLDBG := $(addprefix $(OUTPUT_DIR_A_STLDBG)/,$(ALLDEPS))
endif


