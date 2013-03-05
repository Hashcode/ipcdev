ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=MemMgr Utils Test
endef

NAME = utils_test
INSTALLDIR = sbin

EXTRA_INCVPATH+=$(PROJECT_ROOT)/../../utils \
				$(PROJECT_ROOT)/../../utils/public

EXTRA_LIBVPATH += $(PROJECT_ROOT)/../../memmgr/arm/so.le.v7 \
					$(PROJECT_ROOT)/../../utils/arm/so.le.v7

LDOPTS+= -lmemmgr_utils -lmemmgr

include $(MKFILES_ROOT)/qtargets.mk
