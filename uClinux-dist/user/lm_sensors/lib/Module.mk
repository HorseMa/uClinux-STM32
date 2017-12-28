#  Module.mk - Makefile for a Linux module for reading sensor data.
#  Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Note that MODULE_DIR (the directory in which this file resides) is a
# 'simply expanded variable'. That means that its value is substituted
# verbatim in the rules, until it is redefined. 
MODULE_DIR := lib
LIB_DIR := $(MODULE_DIR)

# The manual dirs and files
LIBMAN3DIR := $(MANDIR)/man3
LIBMAN3FILES := $(MODULE_DIR)/libsensors.3
LIBMAN5DIR := $(MANDIR)/man5
LIBMAN5FILES := $(MODULE_DIR)/sensors.conf.5

# The main and minor version of the library
# The library soname (major number) must be changed if and only if the interface is
# changed in a backward incompatible way.  The interface is defined by
# the public header files - in this case they are error.h, sensors.h,
# chips.h.
LIBMAINVER := 3
LIBMINORVER := 1.0
LIBVER := $(LIBMAINVER).$(LIBMINORVER)

# The static lib name, the shared lib name, and the internal ('so') name of
# the shared lib.
LIBSHBASENAME := libsensors.so
LIBSHLIBNAME := libsensors.so.$(LIBVER)
LIBSTLIBNAME := libsensors.a
LIBSHSONAME := libsensors.so.$(LIBMAINVER)

LIBTARGETS := $(MODULE_DIR)/$(LIBSTLIBNAME) $(MODULE_DIR)/$(LIBSHLIBNAME) \
              $(MODULE_DIR)/$(LIBSHSONAME) $(MODULE_DIR)/$(LIBSHBASENAME)

LIBCSOURCES := $(MODULE_DIR)/data.c $(MODULE_DIR)/general.c \
               $(MODULE_DIR)/error.c $(MODULE_DIR)/chips.c \
               $(MODULE_DIR)/proc.c $(MODULE_DIR)/access.c \
               $(MODULE_DIR)/init.c
ifdef SYSFS_SUPPORT
	LIBCSOURCES := $(LIBCSOURCES) $(MODULE_DIR)/sysfs.c
endif

LIBOTHEROBJECTS := $(MODULE_DIR)/conf-parse.o $(MODULE_DIR)/conf-lex.o
LIBSHOBJECTS := $(LIBCSOURCES:.c=.lo) $(LIBOTHEROBJECTS:.o=.lo)
LIBSTOBJECTS := $(LIBCSOURCES:.c=.ao) $(LIBOTHEROBJECTS:.o=.ao)
LIBEXTRACLEAN := $(MODULE_DIR)/conf-parse.h $(MODULE_DIR)/conf-parse.c \
                 $(MODULE_DIR)/conf-lex.c

LIBHEADERFILES := $(MODULE_DIR)/error.h $(MODULE_DIR)/sensors.h \
                  $(MODULE_DIR)/chips.h

# How to create the shared library
ifdef SYSFS_SUPPORT
$(MODULE_DIR)/$(LIBSHLIBNAME): $(LIBSHOBJECTS)
	$(CC) -shared -Wl,-soname,$(LIBSHSONAME) -o $@ $^ -lc -lm -lsysfs
else
$(MODULE_DIR)/$(LIBSHLIBNAME): $(LIBSHOBJECTS)
	$(CC) -shared -Wl,-soname,$(LIBSHSONAME) -o $@ $^ -lc -lm
endif

$(MODULE_DIR)/$(LIBSHSONAME): $(MODULE_DIR)/$(LIBSHLIBNAME)
	$(RM) $@
	$(LN) $(LIBSHLIBNAME) $@

$(MODULE_DIR)/$(LIBSHBASENAME): $(MODULE_DIR)/$(LIBSHLIBNAME)
	$(RM) $@ 
	$(LN) $(LIBSHLIBNAME) $@

# And the static library
$(MODULE_DIR)/$(LIBSTLIBNAME): $(LIBSTOBJECTS)
	$(RM) $@
	$(AR) rcvs $@ $^

# Depencies for non-C sources
$(MODULE_DIR)/conf-lex.c: $(MODULE_DIR)/conf-lex.l $(MODULE_DIR)/general.h \
                          $(MODULE_DIR)/data.h $(MODULE_DIR)/conf-parse.h
$(MODULE_DIR)/conf-parse.c: $(MODULE_DIR)/conf-parse.y $(MODULE_DIR)/general.h \
                            $(MODULE_DIR)/data.h
$(MODULE_DIR)/conf-parse.h: $(MODULE_DIR)/conf-parse.c

# Include all dependency files
INCLUDEFILES += $(LIBCSOURCES:.c=.ld) $(LIBCSOURCES:.c=.ad)

# Special warning prevention for flex-generated files
$(MODULE_DIR)/conf-lex.ao: $(MODULE_DIR)/conf-lex.c
	$(CC) $(ARCPPFLAGS) $(ARCFLAGS) -Wno-unused -c $< -o $@
$(MODULE_DIR)/conf-lex.lo: $(MODULE_DIR)/conf-lex.c
	$(CC) $(LIBCPPFLAGS) $(LIBCFLAGS) -Wno-unused -c $< -o $@

REMOVELIBST := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBDIR)/%,$(LIB_DIR)/$(LIBSTLIBNAME))
REMOVELIBSH := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBDIR)/%,$(LIB_DIR)/$(LIBSHLIBNAME))
REMOVELNSO  := $(DESTDIR)$(LIBDIR)/$(LIBSHSONAME)
REMOVELNBS  := $(DESTDIR)$(LIBDIR)/$(LIBSHBASENAME)
REMOVELIBHF := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBINCLUDEDIR)/%,$(LIBHEADERFILES))
REMOVEMAN3  := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBMAN3DIR)/%,$(LIBMAN3FILES))
REMOVEMAN5  := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBMAN5DIR)/%,$(LIBMAN5FILES))

all-lib: $(LIBTARGETS)
user :: all-lib

# Generate warnings if the install directory isn't in /etc/ld.so.conf
# or if the library wasn't there before (which means ldconfig must be run).
# Note that some ld.so's put /usr/lib and /lib first, others put them last,
# so we can't make any assumptions.
install-lib: all-lib
	$(MKDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(LIBINCLUDEDIR) $(DESTDIR)$(LIBMAN3DIR) $(DESTDIR)$(LIBMAN5DIR)
	@if [ ! -e "$(DESTDIR)$(LIBDIR)/$(LIBSHSONAME)" ] ; then \
	     echo '******************************************************************************' ; \
	     echo 'Warning: This is the first installation of the $(LIBSHSONAME)*' ; \
	     echo '         library files in $(DESTDIR)$(LIBDIR)!' ; \
	     echo '         You must update the library cache or the userspace tools may fail' ; \
	     echo '         or have unpredictable results!' ; \
		 echo '         Run the following command: /sbin/ldconfig' ; \
	     echo '******************************************************************************' ; \
	fi
	$(INSTALL) -m 644 $(LIB_DIR)/$(LIBSTLIBNAME) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 755 $(LIB_DIR)/$(LIBSHLIBNAME) $(DESTDIR)$(LIBDIR)
	$(LN) $(LIBSHLIBNAME) $(DESTDIR)$(LIBDIR)/$(LIBSHSONAME)
	$(LN) $(LIBSHSONAME) $(DESTDIR)$(LIBDIR)/$(LIBSHBASENAME)
	@if [ "$(DESTDIR)$(LIBDIR)" != "/usr/lib" -a "$(DESTDIR)$(LIBDIR)" != "/lib" ] ; then \
	   if [ -e "/usr/lib/$(LIBSHSONAME)" -o -e "/usr/lib/$(LIBSHBASENAME)" ] ; then \
	     echo '******************************************************************************' ; \
	     echo 'Warning: You have at least one $(LIBSHBASENAME) library file in /usr/lib' ; \
	     echo '         and the new library files are in $(DESTDIR)$(LIBDIR)!' ; \
	     echo '         These old files must be removed or the userspace tools may fail' ; \
	     echo '         or have unpredictable results!' ; \
	     echo '         Run the following command: rm /usr/lib/$(LIBSHBASENAME)*' ; \
	     echo '******************************************************************************' ; \
	   fi ; \
	   grep -q '^$(DESTDIR)$(LIBDIR)$$' /etc/ld.so.conf || \
	   grep -q '^$(DESTDIR)$(LIBDIR)[[:space:]:,=]' /etc/ld.so.conf || \
	   grep -q '[[:space:]:,]$(DESTDIR)$(LIBDIR)$$' /etc/ld.so.conf || \
	   grep -q '[[:space:]:,]$(DESTDIR)$(LIBDIR)[[:space:]:,=]' /etc/ld.so.conf || \
		( echo '******************************************************************************' ; \
		  echo 'Warning: Library directory $(DESTDIR)$(LIBDIR) is not in /etc/ld.so.conf!' ; \
		  echo '         Add it and run /sbin/ldconfig for the userspace tools to work.' ; \
		  echo '******************************************************************************' ) ; \
	fi
	$(INSTALL) -m 644 $(LIBHEADERFILES) $(DESTDIR)$(LIBINCLUDEDIR)
	$(INSTALL) -m 644 $(LIBMAN3FILES) $(DESTDIR)$(LIBMAN3DIR)
	$(INSTALL) -m 644 $(LIBMAN5FILES) $(DESTDIR)$(LIBMAN5DIR)


user_install :: install-lib

user_uninstall::
	$(RM) $(REMOVELIBST) $(REMOVELIBSH) $(REMOVELNSO) $(REMOVELNBS) 
	$(RM) $(REMOVELIBHF) $(REMOVEMAN3) $(REMOVEMAN5)

clean-lib:
	$(RM) $(LIB_DIR)/*.ld $(LIB_DIR)/*.ad
	$(RM) $(LIB_DIR)/*.lo $(LIB_DIR)/*.ao
	$(RM) $(LIBTARGETS) $(LIBEXTRACLEAN)
# old versions
	$(RM) $(LIB_DIR)/$(LIBSHBASENAME).*
clean :: clean-lib
