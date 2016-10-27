SUBDIRS = asio boost dump elf-reader harness proc-reader quitvm run preload hook vm-comm
CLEANDIRS = $(SUBDIRS:%=clean-%)
DISTCLEANDIRS = $(SUBDIRS:%=distclean-%)
INSTALLDIRS = $(SUBDIRS:%=install-%)
UNINSTALLDIRS = $(SUBDIRS:%=uninstall-%)

.PHONY: subdirs clean distclean install-include uninstall-include $(SUBDIRS) $(CLEANDIRS) $(DISTCLEANDIRS) $(INSTALLDIRS) $(UNINSTALLDIRS)

TOP_LEVEL_DIR=$(CURDIR)

TLIBS = -L$(TOP_LEVEL_DIR)/bin

export TLIBS 

all: subdirs 

$(SUBDIRS):
	$(MAKE) -C $@
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
$(DISTCLEANDIRS):
	$(MAKE) -C $(@:distclean-%=%) distclean
$(INSTALLDIRS):
	$(MAKE) -C $(@:install-%=%) install
$(UNINSTALLDIRS):
	$(MAKE) -C $(@:uninstall-%=%) uninstall


subdirs: $(SUBDIRS)
clean: $(CLEANDIRS)
distclean: $(DISTCLEANDIRS)
install: $(INSTALLDIRS) install-include
uninstall: $(UNINSTALLDIRS) uninstall-include
asio: boost
boost:
dump: vm-comm
elf-reader:
harness: boost elf-reader proc-reader vm-comm
quitvm: vm-comm 
run: boost asio harness hook vm-comm
preload: harness

install-include:
	$(MAKE) -C include install
uninstall-include:
	$(MAKE) -C include uninstall
