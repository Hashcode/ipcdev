# the subdirectories of the project to go into
SUBDIRS =  linux/etc linux/src/utils linux/src/api linux/src/family linux/src/daemon linux/src/tests

# hook to remove libtools library extention files (.la)
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/*.la

# where to install common headers on the system
ipcincludedir = $(includedir)/ti/ipc

# the list of common header files (to be installed later)
ipcinclude_HEADERS = $(top_srcdir)/packages/ti/ipc/MessageQ.h \
                $(top_srcdir)/packages/ti/ipc/MultiProc.h \
                $(top_srcdir)/packages/ti/ipc/NameServer.h

