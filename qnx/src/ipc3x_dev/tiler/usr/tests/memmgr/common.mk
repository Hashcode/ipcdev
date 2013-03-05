ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=MemMgr Memmgr Test
endef

NAME = memmgr_test
INSTALLDIR = sbin

EXTRA_INCVPATH+=$(PROJECT_ROOT)/../../../resmgr/tiler/public \
				$(PROJECT_ROOT)/../../memmgr/public \
				$(PROJECT_ROOT)/../../utils \
				$(PROJECT_ROOT)/../../utils/public

CCOPTS+=-g
CCOPTS+= -O0

EXTRA_LIBVPATH += $(PROJECT_ROOT)/../../memmgr/arm/so.le.v7 \
					$(PROJECT_ROOT)/../../utils/arm/so.le.v7

LDOPTS+= -lmemmgr_utils -lmemmgr

include $(MKFILES_ROOT)/qtargets.mk
