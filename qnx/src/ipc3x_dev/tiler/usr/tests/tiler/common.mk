ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=MemMgr Tiler Test
endef

NAME = tiler_test
INSTALLDIR = sbin

EXTRA_INCVPATH+=$(PROJECT_ROOT)/../../../resmgr/tiler/public \
				$(PROJECT_ROOT)/../../memmgr/public \
				$(PROJECT_ROOT)/../../utils \
				$(PROJECT_ROOT)/../../utils/public

EXTRA_LIBVPATH += $(PROJECT_ROOT)/../../memmgr/arm/so.le.v7

LDOPTS+= -lmemmgr

include $(MKFILES_ROOT)/qtargets.mk
