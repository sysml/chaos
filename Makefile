# Basic build configuration
verbose		?= n
config		?= .config

ifeq (,$(filter $(MAKECMDGOALS),configure clean distclean))
-include $(config)
endif


# Applications
CHAOS_BIN	:= chaos
CHAOS_OBJ	:=
CHAOS_OBJ	+= $(patsubst %.c, %.o, $(shell find bin/chaos.src/ -name "*.c"))

# Tests

# Libraries
LIBH2_A		:= libh2.a
LIBH2_SO	:= libh2.so
LIBH2_SO_M		:= $(LIBH2_SO).$(LIBH2_V_MAJOR)
LIBH2_SO_Mm		:= $(LIBH2_SO).$(LIBH2_V_MAJOR).$(LIBH2_V_MINOR)
LIBH2_SO_MmB	:= $(LIBH2_SO).$(LIBH2_V_MAJOR).$(LIBH2_V_MINOR).$(LIBH2_V_BUGFIX)

LIBH2_OBJ	:=
LIBH2_OBJ	+= $(patsubst %.c, %.o, $(shell find lib/h2/ -name "*.c"))


# Default build flags
CFLAGS		+= -Iinc
CFLAGS		+= -std=gnu11
CFLAGS		+= -Wall -MD -MP -g

LDFLAGS		+= -Llib

XEN_CFLAGS	:=
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/libxc/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/include

XEN_LDFLAGS	:=
XEN_LDFLAGS	+= -L$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/lib
XEN_LDFLAGS += -lxenctrl -lxenstore -lxenguest -lxentoollog

LIBH2_DEP_CFLAGS	:=
LIBH2_DEP_CFLAGS	+= $(XEN_CFLAGS)

LIBH2_DEP_LDFLAGS	:=
LIBH2_DEP_LDFLAGS	+= -lh2
LIBH2_DEP_LDFLAGS	+= -Wl,-rpath-link,$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/lib


# Targets
all: libh2 chaos

tests:

install: libh2 chaos
	$(call cmd, "INSTALL", "include/h2"   , cp -r , inc/h2          $(PREFIX)/include/)
	$(call cmd, "INSTALL", $(LIBH2_SO_MmB), cp -f , lib/$(LIBH2_SO) $(PREFIX)/lib/$(LIBH2_SO_MmB))
	$(call cmd, "INSTALL", $(LIBH2_SO_M)  , ln -sf, $(LIBH2_SO_MmB) $(PREFIX)/lib/$(LIBH2_SO_M))
	$(call cmd, "INSTALL", $(LIBH2_SO)    , ln -sf, $(LIBH2_SO_MmB) $(PREFIX)/lib/$(LIBH2_SO))
	$(call cmd, "INSTALL", $(LIBH2_A)     , cp -f , lib/$(LIBH2_A)  $(PREFIX)/lib/$(LIBH2_A))
	$(call cmd, "LDCONFIG", "", ldconfig)
	$(call cmd, "INSTALL", $(CHAOS_BIN)   , cp -f , bin/$(CHAOS_BIN) $(PREFIX)/bin/$(CHAOS_BIN))

uninstall:
	$(call cmd, "UNINSTALL", "include/h2"   , rm -rf, $(PREFIX)/include/h2)
	$(call cmd, "UNINSTALL", $(LIBH2_SO_MmB), rm -f , $(PREFIX)/lib/$(LIBH2_SO_MmB))
	$(call cmd, "UNINSTALL", $(LIBH2_SO_M)  , rm -f , $(PREFIX)/lib/$(LIBH2_SO_M))
	$(call cmd, "UNINSTALL", $(LIBH2_SO)    , rm -f , $(PREFIX)/lib/$(LIBH2_SO))
	$(call cmd, "UNINSTALL", $(LIBH2_A)     , rm -f , $(PREFIX)/lib/$(LIBH2_A))
	$(call cmd, "LDCONFIG", "", ldconfig)
	$(call cmd, "UNINSTALL", $(CHAOS_BIN)   , rm -f , $(PREFIX)/bin/$(CHAOS_BIN))

configure: $(config)

clean:
	$(call cmd, "CLEAN", "*.o", rm -rf, $(shell find -name "*.o"))
	$(call cmd, "CLEAN", "*.d", rm -rf, $(shell find -name "*.d"))

distclean: clean
	$(call cmd, "CLEAN", $(LIBH2_A)  , rm -f, lib/$(LIBH2_A))
	$(call cmd, "CLEAN", $(LIBH2_SO) , rm -f, lib/$(LIBH2_SO))
	$(call cmd, "CLEAN", $(CHAOS_BIN), rm -f, bin/$(CHAOS_BIN))
	$(call cmd, "CLEAN", $(config)   , rm -f, $(config))

.PHONY: all tests install uninstall configure clean distclean


libh2: lib/$(LIBH2_SO) lib/$(LIBH2_A)

chaos: bin/$(CHAOS_BIN)

.PHONY: libh2 chaos


# Build rules
$(config): config.in
	$(call cmd, "CONFIG", $@, cp -n, $^ $@)

# Basically bin/$(CHAOS_BIN) should depend on lib/$(LIBH2_SO) so that the library gets built before
# bin/$(CHAOS_BIN). However, if the lib gets built as a dependency of bin$(CHAOS_BIN) LDFLAGS will
# contain -lh2 which will obviously make the build fail. I really have no time or patience to deal
# with make so for the time being just ignore the problem and document it in README.md.
bin/$(CHAOS_BIN): LDFLAGS += -ljansson
bin/$(CHAOS_BIN): LDFLAGS += $(LIBH2_DEP_LDFLAGS)
bin/$(CHAOS_BIN): $(CHAOS_OBJ)
	$(call clink, $^, $@)

$(CHAOS_OBJ): CFLAGS += $(LIBH2_DEP_CFLAGS)

lib/$(LIBH2_SO): LDFLAGS += -shared
lib/$(LIBH2_SO): LDFLAGS += -Wl,-soname,$(LIBH2_SO_M)
lib/$(LIBH2_SO): LDFLAGS += $(XEN_LDFLAGS)
lib/$(LIBH2_SO): $(LIBH2_OBJ)
	$(call clink, $^, $@)

lib/$(LIBH2_A): $(LIBH2_OBJ)
	$(call cmd, "AR", $@, ar rcs, $@ $^)

$(LIBH2_OBJ): CFLAGS += -fPIC
$(LIBH2_OBJ): CFLAGS += $(XEN_CFLAGS)

%.o: %.c $(config)
	$(call ccompile, $<, $@)


# Define verbose command
ifneq ($(verbose),y)
cmd			 = @printf " %-10s %s\n" $(1) $(2) && $(3) $(4)
else
cmd			 = $(3) $(4)
endif

# Compile and link commands
ccompile	 = $(call cmd, "CC", $2, $(CC), $(CFLAGS) -c $(1) -o $(2))
clink		 = $(call cmd, "LD", $2, $(CC), $(CFLAGS) $(1) $(LDFLAGS) -o $(2))


# Include auto-generated prerequisites
-include $(CHAOS_OBJ:%.o=%.d)
-include $(LIBH2_OBJ:%.o=%.d)
