ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

INSTALLDIR=/usr/lib

define PINFO
PINFO DESCRIPTION=Syslink QNX Usr Libs
endef
NAME=syslink_client

# ---------------------------------------------------------------------------- #
# Defines                                                                      #
# ---------------------------------------------------------------------------- #
# Override definitions in base Makefile if required

#To Over ride the usage of syslink memory manager
SYSLINK_USE_SYSMGR := 0
#To Over ride the build optimization flag
SYSLINK_BUILD_OPTIMIZE := 0
#To override the debug build flag
SYSLINK_BUILD_DEBUG := 0
#To override the TRACE flag
SYSLINK_TRACE_ENABLE := 0

ifeq ("$(SYSLINK_PLATFORM)", "")
#default value
SYSLINK_PLATFORM=omap4430
endif # ifeq ("$(SYSLINK_PLATFORM)", "")

ifeq ("$(SYSLINK_PLATFORM)", "omap4430")
CCOPTS += -DSYSLINK_PLATFORM_OMAP4430
endif # ifeq ("$(SYSLINK_PLATFORM)", "omap4430")
ifeq ("$(SYSLINK_PLATFORM)", "omap5430")
CCOPTS += -DSYSLINK_PLATFORM_OMAP5430
endif # ifeq ("$(SYSLINK_PLATFORM)", "omap5430")

#default SYSLINK Product root path and can be overridden from commandline
SYSLINK_ROOT = $(PROJECT_ROOT)/../../../../..
SYSLINK_BUILDOS = Qnx

#For SOURCE and include paths
#-include $(SYSLINK_ROOT)/ti/syslink/buildutils/hlos/usr/Makefile.inc

#Add Resource Manager include path
#EXTRA_INCVPATH+=$(SYSLINK_ROOT)/ti/syslink/utils/hlos/knl/Qnx/resMgr

#Add extra include path
EXTRA_INCVPATH+=$(SYSLINK_ROOT)	\
				$(SYSLINK_ROOT)/ti/syslink/inc	\
				$(SYSLINK_ROOT)/ti/syslink/inc/usr/$(SYSLINK_BUILDOS)	\
				$(SYSLINK_ROOT)/ti/syslink/inc/usr	\
				$(SYSLINK_ROOT)/ti/syslink/inc/$(SYSLINK_BUILDOS)
#SRCS:=$(CSRCS)

#SRCDIRS=$(sort $(foreach i,$(CSRCS),$(shell dirname $i)))
#EXTRA_SRCVPATH+=$(SRCDIRS)
EXTRA_SRCVPATH+=$(SYSLINK_ROOT)/ti/syslink/ipc/hlos/usr \
				$(SYSLINK_ROOT)/ti/syslink/ipc/hlos/usr/$(SYSLINK_BUILDOS)	\
				$(SYSLINK_ROOT)/ti/syslink/utils/hlos	\
				$(SYSLINK_ROOT)/ti/syslink/utils/hlos/usr	\
				$(SYSLINK_ROOT)/ti/syslink/utils/hlos/usr/$(SYSLINK_BUILDOS)	\
				$(SYSLINK_ROOT)/ti/syslink/utils/hlos/usr/osal/$(SYSLINK_BUILDOS)

#Using the default build rules TODO: to selective pick and choose the compiler/linker/archiver & build rules
include $(MKFILES_ROOT)/qtargets.mk
CCOPTS += -DSYSLINK_BUILDOS_QNX -DSYSLINK_BUILD_DEBUG
ifeq ("$(SYSLINK_DEBUG)", "1")
#enable debug build
CCOPTS += -g -O0
endif # ifeq ("$(SYSLINK_DEBUG)", "")
CCFLAGS += $(COMPILE_FLAGS)
CCFLAGS += -fPIC


