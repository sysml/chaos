# smk Build configuration

# Include configuration
## Don't include configuration if we're either configuring or cleaning
ifneq (,$(filter-out configure clean distclean properclean,$(MAKECMDGOALS)))
-include $(smk_conf_file)
endif

## The if clause above doesn't account for when make is called without targets
ifeq (,$(MAKECMDGOALS))
-include $(smk_conf_file)
endif


# Targets
configure: $(smk_conf_file)

.PHONY: configure

configure_properclean:
	$(call clean, $(smk_conf_file), $(smk_conf_file))

properclean: configure_properclean
.PHONY: configure_properclean


# Rules

# Only add build rule if this project has a configure script
ifneq (,$(wildcard $(smk_conf_script)))
$(smk_conf_file):
	$(call cmd, "CONFIGURE", $@, $(smk_conf_script))
else
# If configure is called explicitly and output an error
ifneq (,$(filter configure,$(MAKECMDGOALS)))
$(error "$(smk_conf_script) not found")
endif
endif
