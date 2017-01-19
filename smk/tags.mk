ctags: TAGS
cscope: cscope.out
.PHONY: ctags cscope


TAGS: cscope.files
	$(call cmd,"TAGS","",etags, cscope.files)

cscope.out: cscope.files
	$(call cmd,"CSCOPE","",cscope, -b -q -k)

cscope.files:
	$(call cmd,"FIND",".*\.[h|hh|c|cc]",find, . -regex ".*\.[h|hh|c|cc]" > $@)


ctags_distclean:
	$(call clean,"TAGS",TAGS)

distclean: ctags_distclean
.PHONY: ctags_distclean

cscope_clean:
	$(call clean,"cscope.*",cscope.files cscope.out cscope.in.out cscope.po.out)

clean: cscope_clean
.PHONY: cscope_clean

cscope_distclean:
	$(call clean,"cscope.out",cscope.out)

distclean: cscope_distclean
.PHONY: cscope_distclean
