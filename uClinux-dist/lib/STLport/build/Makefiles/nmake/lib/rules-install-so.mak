# Time-stamp: <03/10/26 16:42:14 ptr>
# $Id: rules-install-so.mak,v 1.1.2.3 2005/08/31 10:23:24 dums Exp $

!ifndef INSTALL_TAGS
INSTALL_TAGS= install-release-shared install-dbg-shared install-stldbg-shared install-release-static install-dbg-static install-stldbg-static
!endif

install:	$(INSTALL_TAGS)

install-shared: all-shared install-release-shared install-dbg-shared install-stldbg-shared

install-release-shared: release-shared $(INSTALL_BIN_DIR) $(INSTALL_LIB_DIR)
	$(INSTALL_SO) $(SO_NAME_OUT) $(INSTALL_BIN_DIR)
	$(INSTALL_SO) $(PDB_NAME_OUT) $(INSTALL_BIN_DIR)
	$(INSTALL_SO) $(LIB_NAME_OUT) $(INSTALL_LIB_DIR)
	$(INSTALL_SO) $(EXP_NAME_OUT) $(INSTALL_LIB_DIR)
!if "$(COMPILER_NAME)" == "vc8"
	$(INSTALL_SO) $(MANIFEST_NAME_OUT) $(INSTALL_BIN_DIR)
!endif

install-dbg-shared: dbg-shared $(INSTALL_BIN_DIR_DBG) $(INSTALL_LIB_DIR_DBG)
	$(INSTALL_SO) $(SO_NAME_OUT_DBG) $(INSTALL_BIN_DIR_DBG)
	$(INSTALL_SO) $(PDB_NAME_OUT_DBG) $(INSTALL_BIN_DIR_DBG)
	$(INSTALL_SO) $(LIB_NAME_OUT_DBG) $(INSTALL_LIB_DIR_DBG)
	$(INSTALL_SO) $(EXP_NAME_OUT_DBG) $(INSTALL_LIB_DIR_DBG)
!if "$(COMPILER_NAME)" == "vc8"
	$(INSTALL_SO) $(MANIFEST_NAME_OUT_DBG) $(INSTALL_BIN_DIR)
!endif

install-stldbg-shared: stldbg-shared $(INSTALL_BIN_DIR_STLDBG) $(INSTALL_LIB_DIR_STLDBG)
	$(INSTALL_SO) $(SO_NAME_OUT_STLDBG) $(INSTALL_BIN_DIR_STLDBG)
	$(INSTALL_SO) $(PDB_NAME_OUT_STLDBG) $(INSTALL_BIN_DIR_STLDBG)
	$(INSTALL_SO) $(LIB_NAME_OUT_STLDBG) $(INSTALL_LIB_DIR_STLDBG)
	$(INSTALL_SO) $(EXP_NAME_OUT_STLDBG) $(INSTALL_LIB_DIR_STLDBG)
!if "$(COMPILER_NAME)" == "vc8"
	$(INSTALL_SO) $(MANIFEST_NAME_OUT_STLDBG) $(INSTALL_BIN_DIR)
!endif
