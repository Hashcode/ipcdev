ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=TilerUsr TilerUsr Test
endef

NAME = tilerusr_test
INSTALLDIR = sbin

EXTRA_INCVPATH+=$(PROJECT_ROOT)/../../utils \
				$(PROJECT_ROOT)/../../utils/public \
				$(PROJECT_ROOT)/../../tilerusr/public \
				$(PROJECT_ROOT)/../../memmgr/public \
				$(PROJECT_ROOT)/../../../resmgr/tiler/public

CCOPTS+=-g
CCOPTS+= -O0

EXTRA_LIBVPATH += $(PROJECT_ROOT)/../../memmgr/arm/so.le.v7 \
					$(PROJECT_ROOT)/../../utils/arm/so.le.v7 \
					$(PROJECT_ROOT)/../../tilerusr/arm/so.le.v7 \

LDOPTS+= -lmemmgr_utils -ltilerusr -lmemmgr

include $(MKFILES_ROOT)/qtargets.mk
