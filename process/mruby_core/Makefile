# mruby is using Rake (http://rake.rubyforge.org) as a build tool.
# We provide a minimalistic version called minirake inside of our
# codebase.

RAKE = ruby ./minirake -v

all :
	$(RAKE)
.PHONY : all

test : all
	$(RAKE) test
.PHONY : test

clean :
	$(RAKE) clean
.PHONY : clean
