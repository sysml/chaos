# Basic build configuration
verbose		?= n
config		?= .config

ifeq (,$(filter $(MAKECMDGOALS),configure clean distclean))
-include $(config)
endif


# Applications

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


# Targets
all: libh2

tests:

install: libh2
	$(call cmd, "INSTALL", "include/h2"   , cp -r , inc/h2            $(PREFIX)/include/)
	$(call cmd, "INSTALL", $(LIBH2_SO_MmB), cp -f , lib/$(LIBH2_SO)   $(PREFIX)/lib/$(LIBH2_SO_MmB))
	$(call cmd, "INSTALL", $(LIBH2_SO_M)  , ln -sf, $(LIBH2_SO_MmB)   $(PREFIX)/lib/$(LIBH2_SO_M))
	$(call cmd, "INSTALL", $(LIBH2_SO)    , ln -sf, $(LIBH2_SO_MmB)   $(PREFIX)/lib/$(LIBH2_SO))
	$(call cmd, "INSTALL", $(LIBH2_A)     , cp -f , lib/$(LIBH2_A)    $(PREFIX)/lib/$(LIBH2_A))
	$(call cmd, "LDCONFIG", "", ldconfig)

configure: $(config)

clean:
	$(call cmd, "CLEAN", "*.o", rm -rf, $(shell find -name "*.o"))
	$(call cmd, "CLEAN", "*.d", rm -rf, $(shell find -name "*.d"))

distclean: clean
	$(call cmd, "CLEAN", $(LIBH2_A), rm -f, lib/$(LIBH2_A))
	$(call cmd, "CLEAN", $(LIBH2_SO), rm -f, lib/$(LIBH2_SO))
	$(call cmd, "CLEAN", $(config), rm -f, $(config))

.PHONY: all tests install configure clean distclean


libh2: lib/$(LIBH2_SO) lib/$(LIBH2_A)

.PHONY: libh2


# Build rules
$(config): config.in
	$(call cmd, "CONFIG", $@, cp -n, $^ $@)

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
-include $(LIBH2_OBJ:%.o=%.d)
