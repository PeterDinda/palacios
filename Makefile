VERSION = 1
PATCHLEVEL = 1
SUBLEVEL = 0
EXTRAVERSION = Palacios
NAME=Palacios


# *DOCUMENTATION*
# To see a list of typical targets execute "make help"
# More info can be located in ./README
# Comments in this file are targeted only to the developer, do not
# expect to learn how to build the kernel reading this file.

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

# We are using a recursive build, so we need to do a little thinking
# to get the ordering right.
#
# Most importantly: sub-Makefiles should only ever modify files in
# their own directory. If in some directory we have a dependency on
# a file in another dir (which doesn't happen often, but it's often
# unavoidable when linking the built-in.o targets which finally
# turn into palacios), we will call a sub make in that other dir, and
# after that we are sure that everything which is in that other dir
# is now up to date.
#
# The only cases where we need to modify files which have global
# effects are thus separated out and done before the recursive
# descending is started. They are now explicitly listed as the
# prepare rule.

# To put more focus on warnings, be less verbose as default
# Use 'make V=1' to see the full commands

ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
  endif
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

# Call sparse as part of compilation of C files
# Use 'make C=1' to enable sparse checking

ifdef C
  ifeq ("$(origin C)", "command line")
    KBUILD_CHECKSRC = $(C)
  endif
endif
ifndef KBUILD_CHECKSRC
  KBUILD_CHECKSRC = 0
endif

# Use make M=dir to specify directory of external module to build
# Old syntax make ... SUBDIRS=$PWD is still supported
# Setting the environment variable KBUILD_EXTMOD take precedence
ifdef SUBDIRS
  KBUILD_EXTMOD ?= $(SUBDIRS)
endif
ifdef M
  ifeq ("$(origin M)", "command line")
    KBUILD_EXTMOD := $(M)
  endif
endif


# kbuild supports saving output files in a separate directory.
# To locate output files in a separate directory two syntaxes are supported.
# In both cases the working directory must be the root of the kernel src.
# 1) O=
# Use "make O=dir/to/store/output/files/"
# 
# 2) Set KBUILD_OUTPUT
# Set the environment variable KBUILD_OUTPUT to point to the directory
# where the output files shall be placed.
# export KBUILD_OUTPUT=dir/to/store/output/files/
# make
#
# The O= assignment takes precedence over the KBUILD_OUTPUT environment
# variable.


# KBUILD_SRC is set on invocation of make in OBJ directory
# KBUILD_SRC is not intended to be used by the regular user (for now)
ifeq ($(KBUILD_SRC),)

# OK, Make called in directory where kernel src resides
# Do we want to locate output files in a separate directory?
ifdef O
  ifeq ("$(origin O)", "command line")
    KBUILD_OUTPUT := $(O)
  endif
endif

# That's our default target when none is given on the command line
PHONY := _all
_all:

ifneq ($(KBUILD_OUTPUT),)
# Invoke a second make in the output directory, passing relevant variables
# check that the output directory actually exists
saved-output := $(KBUILD_OUTPUT)
KBUILD_OUTPUT := $(shell cd $(KBUILD_OUTPUT) && /bin/pwd)
$(if $(KBUILD_OUTPUT),, \
     $(error output directory "$(saved-output)" does not exist))

PHONY += $(MAKECMDGOALS)

$(filter-out _all,$(MAKECMDGOALS)) _all:
	$(if $(KBUILD_VERBOSE:1=),@)$(MAKE) -C $(KBUILD_OUTPUT) \
	KBUILD_SRC=$(CURDIR) \
	KBUILD_EXTMOD="$(KBUILD_EXTMOD)" -f $(CURDIR)/Makefile $@

# Leave processing to above invocation of make
skip-makefile := 1
endif # ifneq ($(KBUILD_OUTPUT),)
endif # ifeq ($(KBUILD_SRC),)

# We process the rest of the Makefile if this is the final invocation of make
ifeq ($(skip-makefile),)

# If building an external module we do not care about the all: rule
# but instead _all depend on modules
PHONY += all

_all: all


srctree		:= $(if $(KBUILD_SRC),$(KBUILD_SRC),$(CURDIR))
TOPDIR		:= $(srctree)
# FIXME - TOPDIR is obsolete, use srctree/objtree
objtree		:= $(CURDIR)
src		:= $(srctree)
obj		:= $(objtree)

VPATH		:= $(srctree)$(if $(KBUILD_EXTMOD),:$(KBUILD_EXTMOD))

export srctree objtree VPATH TOPDIR


# SUBARCH tells the usermode build what the underlying arch is.  That is set
# first, and if a usermode build is happening, the "ARCH=um" on the command
# line overrides the setting of ARCH below.  If a native build is happening,
# then ARCH is assigned, getting whatever value it gets normally, and 
# SUBARCH is subsequently ignored.

SUBARCH := $(shell uname -m | sed -e s/i.86/i386/  )

# Cross compiling and selecting different set of gcc/bin-utils
# ---------------------------------------------------------------------------
#
# When performing cross compilation for other architectures ARCH shall be set
# to the target architecture. (See arch/* for the possibilities).
# ARCH can be set during invocation of make:
# make ARCH=ia64
# Another way is to have ARCH set in the environment.
# The default ARCH is the host where make is executed.

# CROSS_COMPILE specify the prefix used for all executables used
# during compilation. Only gcc and related bin-utils executables
# are prefixed with $(CROSS_COMPILE).
# CROSS_COMPILE can be set on the command line
# make CROSS_COMPILE=ia64-linux-
# Alternatively CROSS_COMPILE can be set in the environment.
# Default value for CROSS_COMPILE is not to prefix executables
# Note: Some architectures assign CROSS_COMPILE in their arch/*/Makefile

ARCH		?= $(SUBARCH)
CROSS_COMPILE	?=

# Architecture as present in compile.h
UTS_MACHINE := $(ARCH)

# SHELL used by kbuild
CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)

HOSTCC  	= gcc
HOSTCXX  	= g++
HOSTCFLAGS	= -Wall -Wstrict-prototypes  -fomit-frame-pointer \
			-Wno-unused -Wno-format-security -U_FORTIFY_SOURCE
HOSTCXXFLAGS	= -O

# 	Decide whether to build built-in, modular, or both.
#	Normally, just do built-in.

KBUILD_MODULES :=
KBUILD_BUILTIN := 1

#	If we have only "make modules", don't compile built-in objects.
#	When we're building modules with modversions, we need to consider
#	the built-in objects during the descend as well, in order to
#	make sure the checksums are uptodate before we record them.

ifeq ($(MAKECMDGOALS),modules)
  KBUILD_BUILTIN := $(if $(CONFIG_MODVERSIONS),1)
endif

#	If we have "make <whatever> modules", compile modules
#	in addition to whatever we do anyway.
#	Just "make" or "make all" shall build modules as well

ifneq ($(filter all _all modules,$(MAKECMDGOALS)),)
  KBUILD_MODULES := 1
endif

ifeq ($(MAKECMDGOALS),)
  KBUILD_MODULES := 1
endif

export KBUILD_MODULES KBUILD_BUILTIN
export KBUILD_CHECKSRC KBUILD_SRC KBUILD_EXTMOD

# Beautify output
# ---------------------------------------------------------------------------
#
# Normally, we echo the whole command before executing it. By making
# that echo $($(quiet)$(cmd)), we now have the possibility to set
# $(quiet) to choose other forms of output instead, e.g.
#
#         quiet_cmd_cc_o_c = Compiling $(RELDIR)/$@
#         cmd_cc_o_c       = $(CC) $(c_flags) -c -o $@ $<
#
# If $(quiet) is empty, the whole command will be printed.
# If it is set to "quiet_", only the short version will be printed. 
# If it is set to "silent_", nothing wil be printed at all, since
# the variable $(silent_cmd_cc_o_c) doesn't exist.
#
# A simple variant is to prefix commands with $(Q) - that's useful
# for commands that shall be hidden in non-verbose mode.
#
#	$(Q)ln $@ :<
#
# If KBUILD_VERBOSE equals 0 then the above command will be hidden.
# If KBUILD_VERBOSE equals 1 then the above command is displayed.

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

# If the user is running make -s (silent mode), suppress echoing of
# commands

ifneq ($(findstring s,$(MAKEFLAGS)),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE


# Look for make include files relative to root of kernel src
MAKEFLAGS += --include-dir=$(srctree)

# We need some generic definitions
include  $(srctree)/scripts/Kbuild.include

# For maximum performance (+ possibly random breakage, uncomment
# the following)

#MAKEFLAGS += -rR

# Make variables (CC, etc...)

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
AWK		= awk
GENKSYMS	= scripts/genksyms/genksyms
DEPMOD		= /sbin/depmod
KALLSYMS	= scripts/kallsyms
PERL		= perl
CHECK		= sparse


CFLAGS_KERNEL	=
AFLAGS_KERNEL	=


# Use V3_INCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
V3_INCLUDE      := -Ipalacios/include \
                   $(if $(KBUILD_SRC),-I$(srctree)/palacios/include) \
		   -include palacios/include/autoconf.h

CPPFLAGS        := $(V3_INCLUDE) -D__V3VEE__

CFLAGS 		:=  -fno-stack-protector -Wall -Werror  -mno-red-zone -fno-common 



#-ffreestanding


LDFLAGS         := --whole-archive 

ifeq ($(call cc-option-yn, -fgnu89-inline),y)
CFLAGS		+= -fgnu89-inline
endif

AFLAGS		:= 

# Read KERNELRELEASE from .kernelrelease (if it exists)
#KERNELRELEASE = $(shell cat .kernelrelease 2> /dev/null)
#KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

export	VERSION PATCHLEVEL SUBLEVEL KERNELRELEASE KERNELVERSION \
	ARCH CONFIG_SHELL HOSTCC HOSTCFLAGS CROSS_COMPILE AS LD CC \
	CPP AR NM STRIP OBJCOPY OBJDUMP MAKE AWK GENKSYMS PERL UTS_MACHINE \
	HOSTCXX HOSTCXXFLAGS CHECK

export CPPFLAGS NOSTDINC_FLAGS V3_INCLUDE OBJCOPYFLAGS LDFLAGS
export CFLAGS CFLAGS_KERNEL 
export AFLAGS AFLAGS_KERNEL

# When compiling out-of-tree modules, put MODVERDIR in the module
# tree rather than in the kernel tree. The kernel tree might
# even be read-only.
export MODVERDIR := $(if $(KBUILD_EXTMOD),$(firstword $(KBUILD_EXTMOD))/).tmp_versions

# Files to ignore in find ... statements

RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS -o -name .pc -o -name .hg -o -name .git \) -prune -o
export RCS_TAR_IGNORE := --exclude SCCS --exclude BitKeeper --exclude .svn --exclude CVS --exclude .pc --exclude .hg --exclude .git

# ===========================================================================
# Rules shared between *config targets and build targets

# Basic helpers built in scripts/
PHONY += scripts_basic
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic

# To avoid any implicit rule to kick in, define an empty command.
scripts/basic/%: scripts_basic ;

PHONY += outputmakefile
# outputmakefile generates a Makefile in the output directory, if using a
# separate output directory. This allows convenient use of make in the
# output directory.
outputmakefile:
ifneq ($(KBUILD_SRC),)
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkmakefile \
	    $(srctree) $(objtree) $(VERSION) $(PATCHLEVEL)
endif

# To make sure we do not include .config for any of the *config targets
# catch them early, and hand them over to scripts/kconfig/Makefile
# It is allowed to specify more targets when calling make, including
# mixing *config targets and build targets.
# For example 'make oldconfig all'. 
# Detect when mixed targets is specified, and make a second invocation
# of make so .config is not included in this case either (for *config).

no-dot-config-targets := clean mrproper distclean \
			 cscope TAGS tags help %docs check%

config-targets := 0
mixed-targets  := 0
dot-config     := 1

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif

ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter config %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(filter-out config %config,$(MAKECMDGOALS)),)
                        mixed-targets := 1
                endif
        endif
endif

ifeq ($(mixed-targets),1)
# ===========================================================================
# We're called with mixed targets (*config and build targets).
# Handle them one by one.

%:: FORCE
	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= $@

else
ifeq ($(config-targets),1)
# ===========================================================================
# *config targets only - make sure prerequisites are updated, and descend
# in scripts/kconfig to make the *config target

# Read arch specific Makefile to set KBUILD_DEFCONFIG as needed.
# KBUILD_DEFCONFIG may point out an alternative default configuration
# used for 'make defconfig'
include $(srctree)/Makefile.$(ARCH)
export KBUILD_DEFCONFIG

config %config: scripts_basic outputmakefile FORCE
	$(Q)mkdir -p palacios/include/config
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
#	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= .kernelrelease

else
# ===========================================================================
# Build targets only - this includes Palacios arch specific targets, clean
# targets and others. In general all targets except *config targets.


# Additional helpers built in scripts/
# Carefully list dependencies so we do not try to build scripts twice
# in parrallel
PHONY += scripts
scripts: scripts_basic palacios/include/config/MARKER
	$(Q)$(MAKE) $(build)=$(@)

scripts_basic: palacios/include/autoconf.h

# Objects we will link into palacios / subdirs we need to visit
#drivers-y	:= drivers/
#net-y		:= net/
#core-y		:= usr/
core-y          := palacios/src/palacios/
libs-y		:= palacios/lib/$(ARCH)/
devices-y       := palacios/src/devices/
modules-y       := modules/



ifeq ($(dot-config),1)
# In this section, we need .config

# Read in dependencies to all Kconfig* files, make sure to run
# oldconfig if changes are detected.
-include .kconfig.d

include .config

# If .config needs to be updated, it will be done via the dependency
# that autoconf has on .config.
# To avoid any implicit rule to kick in, define an empty command
.config .kconfig.d: ;

# If .config is newer than palacios/include/autoconf.h, someone tinkered
# with it and forgot to run make oldconfig.
# If kconfig.d is missing then we are probarly in a cleaned tree so
# we execute the config step to be sure to catch updated Kconfig files
palacios/include/autoconf.h: .kconfig.d .config
	$(Q)mkdir -p palacios/include/config
	$(Q)$(MAKE) -f $(srctree)/Makefile silentoldconfig
else
# Dummy target needed, because used as prerequisite
palacios/include/autoconf.h: ;
endif


ifdef CONFIG_LINUX
DEFAULT_EXTRA_TARGETS=linux_module
else
DEFAULT_EXTRA_TARGETS=
endif

# The all: target is the default when no target is given on the
# command line.
# This allow a user to issue only 'make' to build a kernel including modules
# Defaults palacios but it is usually overriden in the arch makefile
all: palacios $(DEFAULT_EXTRA_TARGETS)


ifdef CONFIG_LINUX
CFLAGS          += -mcmodel=kernel 
else
CFLAGS          += -fPIC
endif

ifdef CONFIG_FRAME_POINTER
CFLAGS		+= -fno-omit-frame-pointer $(call cc-option,-fno-optimize-sibling-calls,)
else
CFLAGS		+= -fomit-frame-pointer
endif


ifdef CONFIG_UNWIND_INFO
CFLAGS		+= -fasynchronous-unwind-tables
endif

ifdef CONFIG_DEBUG_INFO
CFLAGS		+= -g
else 
CFLAGS          += -O
endif


include $(srctree)/Makefile.$(ARCH)

# arch Makefile may override CC so keep this after arch Makefile is included
NOSTDINC_FLAGS +=


# disable pointer signedness warnings in gcc 4.0
CFLAGS += $(call cc-option,-Wno-pointer-sign,)

# Default kernel image to build when no specific target is given.
# KBUILD_IMAGE may be overruled on the commandline or
# set in the environment
# Also any assignments in /Makefile.$(ARCH) take precedence over
# this default value
export KBUILD_IMAGE ?= palacios

#
# INSTALL_PATH specifies where to place the updated kernel and system map
# images. Default is /boot, but you can set it to other values
export	INSTALL_PATH ?= /build





palacios-dirs	:= $(patsubst %/,%,$(filter %/,  \
		     $(core-y) $(devices-y) $(libs-y)) $(modules-y))



#palacios-alldirs	:= $(sort $(palacios-dirs) $(patsubst %/,%,$(filter %/, \
#		     $(core-n) $(core-) $(devices-n) $(devices-) \
#		     $(libs-n)    $(libs-))))


palacios-cleandirs := $(sort $(palacios-dirs) $(patsubst %/,%,$(filter %/, \
		     	$(core-n) $(core-) $(devices-n) $(devices-) $(modules-n) $(modules-))))



core-y		:= $(patsubst %/, %/built-in.o, $(core-y))
devices-y	:= $(patsubst %/, %/built-in.o, $(devices-y))
libs-y		:= $(patsubst %/, %/built-in.o, $(libs-y))
modules-y       := $(patsubst %/, %/built-in.o, $(modules-y))
#lnxmod-y        := $(patsubst %/, %/built-in.o, $(lnxmod-y))

#core-y		:= $(patsubst %/, %/lib.a, $(core-y))
#devices-y	:= $(patsubst %/, %/lib.a, $(devices-y))

# $(patsubst %/, %/built-in.o, $(libs-y))


# Build palacios
# ---------------------------------------------------------------------------
# Palacios is build from the objects selected by $(palacios-init) and
# $(palacios). Most are built-in.o files from top-level directories
# in the kernel tree, others are specified in Makefile.
# Ordering when linking is important, and $(palacios-init) must be first.
#
# palacios
#   ^
#   |
#   +--< $(palacios)
#   |    +--< devices/built-in.o  + more
#



palacios := $(core-y) $(devices-y) $(libs-y) $(modules-y)


# Rule to link palacios - also used during CONFIG_CONFIGKALLSYMS
# May be overridden by /Makefile.$(ARCH)
quiet_cmd_palacios__ ?= AR $@ 
      cmd_palacios__ ?= $(AR) rcs $@ $^



# Generate new palacios version
quiet_cmd_palacios_version = GEN     .version
      cmd_palacios_version = set -e;                       \
	if [ ! -r .version ]; then			\
	  rm -f .version;				\
	  echo 1 >.version;				\
	else						\
	  mv .version .old_version;			\
	  expr 0$$(cat .old_version) + 1 >.version;	\
	fi;					


# Link of palacios
# If CONFIG_KALLSYMS is set .version is already updated
# Generate System.map and verify that the content is consistent
# Use + in front of the palacios_version rule to silent warning with make -j2
# First command is ':' to allow us to use + in front of the rule
define rule_palacios__
	:
	$(call cmd,palacios__)
	$(Q)echo 'cmd_$@ := $(cmd_palacios__)' > $(@D)/.$(@F).cmd


endef


# palacios image - including updated kernel symbols
libv3vee.a: $(palacios)
	$(call if_changed_rule,palacios__)

palacios: libv3vee.a




linux_module/v3vee.ko: linux_module/*.c libv3vee.a
	cd linux_module/ && make CONFIG_LINUX_KERN=$(CONFIG_LINUX_KERN)
	cp linux_module/v3vee.ko v3vee.ko


linux_module: linux_module/v3vee.ko 



palacios.asm: palacios
	$(OBJDUMP) --disassemble $< > $@

# The actual objects are generated when descending, 
# make sure no implicit rule kicks in
$(sort  $(palacios)) : $(palacios-dirs) ;

# Handle descending into subdirectories listed in $(palacios-dirs)
# Preset locale variables to speed up the build process. Limit locale
# tweaks to this spot to avoid wrong language settings when running
# make menuconfig etc.
# Error messages still appears in the original language

PHONY += $(palacios-dirs)
$(palacios-dirs): prepare scripts
	$(Q)$(MAKE) $(build)=$@





# Things we need to do before we recursively start building the kernel
# or the modules are listed in "prepare".
# A multi level approach is used. prepareN is processed before prepareN-1.
# archprepare is used in arch Makefiles and when processed arch symlink,
# version.h and scripts_basic is processed / created.

# Listed in dependency order
PHONY += prepare archprepare prepare0 prepare1 prepare2 prepare3

# prepare-all is deprecated, use prepare as valid replacement
PHONY += prepare-all

# prepare3 is used to check if we are building in a separate output directory,
# and if so do:
# 1) Check that make has not been executed in the kernel src $(srctree)
prepare3: 
ifneq ($(KBUILD_SRC),)
	@echo '  Using $(srctree) as source for kernel'
	$(Q)if [ -f $(srctree)/.config ]; then \
		echo "  $(srctree) is not clean, please run 'make mrproper'";\
		echo "  in the '$(srctree)' directory.";\
		/bin/false; \
	fi;
endif

# prepare2 creates a makefile if using a separate output directory
prepare2: prepare3 outputmakefile

prepare1: prepare2 palacios/include/config/MARKER

archprepare: prepare1 scripts_basic

prepare0: archprepare FORCE
	$(Q)$(MAKE) $(build)=.

# All the preparing..
prepare prepare-all: prepare0



palacios/include/config/MARKER: scripts/basic/split-include palacios/include/autoconf.h
	@echo '  SPLIT   palacios/include/autoconf.h -> palacios/include/config/*'
	@scripts/basic/split-include palacios/include/autoconf.h palacios/include/config
	@touch $@



# ---------------------------------------------------------------------------

PHONY += depend dep
depend dep:
	@echo '*** Warning: make $@ is unnecessary now.'



###
# Cleaning is done on three levels.
# make clean     Delete most generated files
#                Leave enough to build external modules
# make mrproper  Delete the current configuration, and all generated files
# make distclean Remove editor backup files, patch leftover files and the like

# Directories & files removed with 'make clean'
CLEAN_DIRS  += $(MODVERDIR)
CLEAN_FILES +=	 v3vee.asm \
                 .tmp_version .tmp_palacios*

# Directories & files removed with 'make mrproper'
MRPROPER_DIRS  += palacios/include/config 
MRPROPER_FILES += .config .config.old  .version .old_version \
                  palacios/include/autoconf.h  \
		  tags TAGS cscope*


#	 	\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \


# clean - Delete most, but leave enough to build external modules
#
clean: rm-dirs  := $(CLEAN_DIRS)
clean: rm-files := $(CLEAN_FILES)
clean-dirs      := $(addprefix _clean_,$(srctree) $(palacios-cleandirs))

PHONY += $(clean-dirs) clean archclean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)


clean: archclean $(clean-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@find . $(RCS_FIND_IGNORE) \
		\( -name 'lib' \) -prune -o \
	 	\( -name '*.[oas]' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' -o -name '*.ko' \) \
		-type f -print | xargs rm -f

# mrproper - Delete all generated files, including .config
#
mrproper: rm-dirs  := $(wildcard $(MRPROPER_DIRS))
mrproper: rm-files := $(wildcard $(MRPROPER_FILES))
#mrproper-dirs      := $(addprefix _mrproper_,Documentation/DocBook scripts)
mrproper-dirs      := $(addprefix _mrproper_, scripts)

PHONY += $(mrproper-dirs) mrproper archmrproper
$(mrproper-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _mrproper_%,%,$@)

mrproper: clean archmrproper $(mrproper-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)

# distclean
#
PHONY += distclean

distclean: mrproper
	@find $(srctree) $(RCS_FIND_IGNORE) \
	 	\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
	 	-o -name '.*.rej' -o -size 0 \
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \) \
		-type f -print | xargs rm -f


# Packaging of the kernel to various formats
# ---------------------------------------------------------------------------
# rpm target kept for backward compatibility
package-dir	:= $(srctree)/scripts/package

%pkg: FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@
rpm: FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@


# Brief documentation of the typical targets used
# ---------------------------------------------------------------------------


help:
	@echo  'Cleaning targets:'
	@echo  '  clean		  - remove most generated files but keep the config'
	@echo  '  mrproper	  - remove all generated files + config + various backup files'
	@echo  ''
	@echo  'Configuration targets:'
	@$(MAKE) -f $(srctree)/scripts/kconfig/Makefile help
	@echo  ''
	@echo  'Other generic targets:'
	@echo  '  all		  - Build all targets marked with [*]'
	@echo  '* palacios	  - Build the palacios VMM'
	@echo  '  dir/            - Build all files in dir and below'
	@echo  '  dir/file.[ois]  - Build specified target only'
	@echo  '  dir/file.ko     - Build module including final link'
	@echo  '  rpm		  - Build a kernel as an RPM package'
	@echo  '  tags/TAGS	  - Generate tags file for editors'
	@echo  '  cscope	  - Generate cscope index'
	@echo  ''
	@echo  'Kernel packaging:'
	@$(MAKE) $(build)=$(package-dir) help
	@echo  ''
	@echo  'Documentation targets:'
	@$(MAKE) -f $(srctree)/Documentation/DocBook/Makefile dochelp
	@echo  ''
	@echo  'Architecture specific targets ($(ARCH)):'
	@$(if $(archhelp),$(archhelp),\
		echo '  No architecture specific help defined for $(ARCH)')
	@echo  ''
	@echo  '  make V=0|1 [targets] 0 => quiet build (default), 1 => verbose build'
	@echo  '  make O=dir [targets] Locate all output files in "dir", including .config'
	@echo  '  make C=1   [targets] Check all c source with $$CHECK (sparse)'
	@echo  '  make C=2   [targets] Force check of all c source with $$CHECK (sparse)'
	@echo  ''
	@echo  'Execute "make" or "make all" to build all targets marked with [*] '
	@echo  'For further info see the ./README file'


# Documentation targets
# ---------------------------------------------------------------------------
%docs: scripts_basic FORCE
	$(Q)$(MAKE) $(build)=Documentation/DocBook $@


# Generate tags for editors
# ---------------------------------------------------------------------------

#We want __srctree to totally vanish out when KBUILD_OUTPUT is not set
#(which is the most common case IMHO) to avoid unneeded clutter in the big tags file.
#Adding $(srctree) adds about 20M on i386 to the size of the output file!

ifeq ($(src),$(obj))
__srctree =
else
__srctree = $(srctree)/
endif

ifeq ($(ALLSOURCE_ARCHS),)
ifeq ($(ARCH),um)
ALLINCLUDE_ARCHS := $(ARCH) $(SUBARCH)
else
ALLINCLUDE_ARCHS := $(ARCH)
endif
else
#Allow user to specify only ALLSOURCE_PATHS on the command line, keeping existing behaviour.
ALLINCLUDE_ARCHS := $(ALLSOURCE_ARCHS)
endif

ALLSOURCE_ARCHS := $(ARCH)

define all-sources
	( find $(__srctree)palacios $(RCS_FIND_IGNORE) \
	       \( -name lib \) -prune -o \
	       -name '*.[chS]' -print; )
endef

quiet_cmd_cscope-file = FILELST cscope.files
      cmd_cscope-file = (echo \-k; echo \-q; $(all-sources)) > cscope.files

quiet_cmd_cscope = MAKE    cscope.out
      cmd_cscope = cscope -b

cscope: FORCE
	$(call cmd,cscope-file)
	$(call cmd,cscope)

quiet_cmd_TAGS = MAKE   $@
define cmd_TAGS
	rm -f $@; \
	ETAGSF=`etags --version | grep -i exuberant >/dev/null &&     \
                echo "-I __initdata,__exitdata,__acquires,__releases  \
                      -I EXPORT_SYMBOL,EXPORT_SYMBOL_GPL              \
                      --extra=+f --c-kinds=+px"`;                     \
                $(all-sources) | xargs etags $$ETAGSF -a
endef

TAGS: FORCE
	$(call cmd,TAGS)


quiet_cmd_tags = MAKE   $@
define cmd_tags
	rm -f $@; \
	CTAGSF=`ctags --version | grep -i exuberant >/dev/null &&     \
                echo "-I __initdata,__exitdata,__acquires,__releases  \
                      -I EXPORT_SYMBOL,EXPORT_SYMBOL_GPL              \
                      --extra=+f --c-kinds=+px"`;                     \
                $(all-sources) | xargs ctags $$CTAGSF -a
endef

tags: FORCE
	$(call cmd,tags)


# Scripts to check various things for consistency
# ---------------------------------------------------------------------------

endif #ifeq ($(config-targets),1)
endif #ifeq ($(mixed-targets),1)




# Single targets
# ---------------------------------------------------------------------------
# Single targets are compatible with:
# - build whith mixed source and output
# - build with separate output dir 'make O=...'
# - external modules
#
#  target-dir => where to store outputfile
#  build-dir  => directory in kernel source tree to use

build-dir  = $(patsubst %/,%,$(dir $@))
target-dir = $(dir $@)


%.s: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.i: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.lst: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.s: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)

# Modules
/ %/: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1) \
	$(build)=$(build-dir)
%.ko: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1)   \
	$(build)=$(build-dir) $(@:.ko=.o)
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modpost

# FIXME Should go into a make.lib or something 
# ===========================================================================

quiet_cmd_rmdirs = $(if $(wildcard $(rm-dirs)),CLEAN   $(wildcard $(rm-dirs)))
      cmd_rmdirs = rm -rf $(rm-dirs)

quiet_cmd_rmfiles = $(if $(wildcard $(rm-files)),CLEAN   $(wildcard $(rm-files)))
      cmd_rmfiles = rm -f $(rm-files)


a_flags = -Wp,-MD,$(depfile) $(AFLAGS) $(AFLAGS_KERNEL) \
	  $(NOSTDINC_FLAGS) $(CPPFLAGS) \
	  $(modkern_aflags) $(EXTRA_AFLAGS) $(AFLAGS_$(*F).o)

quiet_cmd_as_o_S = AS      $@
cmd_as_o_S       = $(CC) $(a_flags) -c -o $@ $<

# read all saved command lines

targets := $(wildcard $(sort $(targets)))
cmd_files := $(wildcard .*.cmd $(foreach f,$(targets),$(dir $(f)).$(notdir $(f)).cmd))

ifneq ($(cmd_files),)
  $(cmd_files): ;	# Do not try to update included dependency files
  include $(cmd_files)
endif

# Shorthand for $(Q)$(MAKE) -f scripts/Makefile.clean obj=dir
# Usage:
# $(Q)$(MAKE) $(clean)=dir
clean := -f $(if $(KBUILD_SRC),$(srctree)/)scripts/Makefile.clean obj

endif	# skip-makefile

# Force the O variable for user and init_task (not set by kbuild?)
user init_task: O:=$(if $O,$O,$(objtree))
# Build Palacios user-space libraries and example programs
user: FORCE
	if [ ! -d $O/$@ ]; then mkdir $O/$@; fi
	$(MAKE) \
		-C $(src)/$@ \
		O=$O/$@ \
		src=$(src)/$@ \
		all



PHONY += FORCE
FORCE:


# Declare the contents of the .PHONY variable as phony.  We keep that
# information in a variable se we can use it in if_changed and friends.
.PHONY: $(PHONY)
