# The file was modified for CRETE
#
# Copyright (c) <2016>, <System Validation Laboratory, Portland State University>
# Main authors: Bo Chen (chenbo@pdx.edu)
#

BUILD_DIR=$(CURDIR)

GENERATED_HEADERS = config-host.h

ifneq ($(wildcard config-host.mak),)
# Put the all: rule here so that config-host.mak can contain dependencies.
all: build-all
include config-host.mak
include $(SRC_PATH)/rules.mak
config-host.mak: $(SRC_PATH)/configure
	@echo $@ is out-of-date, running configure
	@sed -n "/.*Configured with/s/[^:]*: //p" $@ | sh
else
config-host.mak:
	@echo "Please call configure before running make!"
	@exit 1
endif

# Don't try to regenerate Makefile or configure
# We don't generate any of them
Makefile: ;
configure: ;

build-all: recurse-all

config-host.h: config-host.h-timestamp
config-host.h-timestamp: config-host.mak

SUBDIR_RULES=$(patsubst %,subdir-%, $(TARGET_DIRS))

subdir-%: $(GENERATED_HEADERS)
	$(call quiet-command,$(MAKE) $(SUBDIR_MAKEFLAGS) -C $* V="$(V)" TARGET_DIR="$*/" all,)


recurse-all: $(SUBDIR_RULES)

clean:
# avoid old build problems by removing potentially incorrect old files
	rm -f config.mak op-i386.h opc-i386.h gen-op-i386.h op-arm.h opc-arm.h gen-op-arm.h
	rm -f qemu-options.def
	rm -f *.o *.d *.a *.lo $(TOOLS) $(CHECKS) qemu-ga TAGS cscope.* *.pod *~ */*~
	rm -Rf .libs
	rm -f slirp/*.o slirp/*.d audio/*.o audio/*.d block/*.o block/*.d net/*.o net/*.d fsdev/*.o fsdev/*.d ui/*.o ui/*.d qapi/*.o qapi/*.d qga/*.o qga/*.d
	rm -f qemu-img-cmds.h
	rm -f trace/*.o trace/*.d
	rm -f trace.c trace.h trace.c-timestamp trace.h-timestamp
	rm -f trace-dtrace.dtrace trace-dtrace.dtrace-timestamp
	rm -f trace-dtrace.h trace-dtrace.h-timestamp
	rm -f $(GENERATED_SOURCES)
	rm -rf $(qapi-dir)
	for d in $(TARGET_DIRS); do \
	if test -d $$d; then $(MAKE) -C $$d $@ || exit 1; fi; \
        done

distclean: clean
	rm -f config-host.mak config-host.h* config-host.ld $(DOCS) qemu-options.texi qemu-img-cmds.texi qemu-monitor.texi
	rm -f config-all-devices.mak
	rm -f roms/seabios/config.mak roms/vgabios/config.mak
	rm -f qemu-doc.info qemu-doc.aux qemu-doc.cp qemu-doc.cps qemu-doc.dvi
	rm -f qemu-doc.fn qemu-doc.fns qemu-doc.info qemu-doc.ky qemu-doc.kys
	rm -f qemu-doc.log qemu-doc.pdf qemu-doc.pg qemu-doc.toc qemu-doc.tp
	rm -f qemu-doc.vr
	rm -f config.log
	rm -f qemu-tech.info qemu-tech.aux qemu-tech.cp qemu-tech.dvi qemu-tech.fn qemu-tech.info qemu-tech.ky qemu-tech.log qemu-tech.pdf qemu-tech.pg qemu-tech.toc qemu-tech.tp qemu-tech.vr
	for d in $(TARGET_DIRS) $(QEMULIBS); do \
	rm -rf $$d || exit 1 ; \
        done
