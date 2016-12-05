ifneq ($(verbose),y)
cmd			 = @printf " %-10s %s\n" $(1) $(2) && $(3) $(4)
else
cmd			 = $(3) $(4)
endif

clean		 = $(call cmd, "CLEAN", $1, rm -f, $(2))
