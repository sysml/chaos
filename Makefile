# Basic build configuration
verbose		?= n
config		?= .config

ifeq (,$(filter $(MAKECMDGOALS),configure clean distclean))
-include $(config)
endif


# Applications

# Tests

# Libraries
LIBH2		:= lib/h2/libh2.so.$(LIBH2_V_MAJOR).$(LIBH2_V_MINOR).$(LIBH2_V_BUGFIX)
LIBH2_OBJ	:=
LIBH2_OBJ	+= $(patsubst %.c, %.o, $(shell find lib/h2 -name "*.c"))


# Default build flags
CFLAGS		+= -Iinc
CFLAGS		+= -std=gnu11
CFLAGS		+= -Wall -MD -MP -g -O3

XEN_CFLAGS	:=
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/libxc/include

XEN_LDFLAGS	:=
XEN_LDFLAGS += -L$(XEN_ROOT)/tools/libs/call
XEN_LDFLAGS += -L$(XEN_ROOT)/tools/libs/evtchn
XEN_LDFLAGS += -L$(XEN_ROOT)/tools/libs/foreignmemory
XEN_LDFLAGS += -L$(XEN_ROOT)/tools/libs/gnttab
XEN_LDFLAGS += -L$(XEN_ROOT)/tools/libs/toollog
XEN_LDFLAGS += -L$(XEN_ROOT)/tools/libxc
XEN_LDFLAGS += -lxenctrl -lxenstore -lxenguest -lxentoollog


# Targets
all: $(LIBH2)

tests:

install:

configure: $(config)

clean:
	$(call cmd, "CLEAN", "*.o", rm -rf, $(shell find -name "*.o"))
	$(call cmd, "CLEAN", "*.d", rm -rf, $(shell find -name "*.d"))

distclean: clean
	$(call cmd, "CLEAN", $(config), rm -f, $(config))

.PHONY: all tests install configure clean distclean


libh2: $(LIBH2)

.PHONY: libh2


# Build rules
$(config): config.in
	$(call cmd, "CONFIG", $@, cp -n, $^ $@)

$(LIBH2): LDFLAGS += -shared
$(LIBH2): LDFLAGS += -Wl,-soname,libh2.so.$(LIBH2_V_MAJOR)
$(LIBH2): LDFLAGS += $(XEN_LDFLAGS)
$(LIBH2): $(LIBH2_OBJ)
	$(call clink, $^, $@)

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
