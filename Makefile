CFLAGS += -Wall -Werror -std=gnu99

evdump: evdump.c

evdump.c: input.generated.h

# Create a header file from interesting input.h definitions
input.generated.h: input.generated 
	@echo "// Generated file, do not check in!" > $@
	@$(foreach t,EV SYN KEY BTN REL ABS SW MSC LED REP SND BUS FF_STATUS, { \
		echo -e 'const char * ed_$(t)(int n)\n{\n  switch(n)\n  {'; \
		awk '$$2~/^$(t)_/ && $$3~/^[0-9]/{v=strtonum($$3); if (v<65536 && !u[v]++) {print "    case " v ": return \"" $$2 "\";"}}'; \
		echo -e '  }\n  return NULL;\n}'; \
	} < $< >> $@;)

# Preprocess linux/input.h macro definitions to a temp file, from which we will
# extract various event definitions. Don't access input.h directly because
# newer linux's construct it from nested #includes. Also this gives the right
# stuff if sysroot is in use.
input.generated:
	echo "// Generated file, do not check in!" > $@
	$(CC) $(CFLAGS) -E -dM -include "linux/input.h" - < /dev/null >> $@

.PHONY: clean
clean:; rm -f evdump input.generated*

ifneq ($(DESTINATION),)
.PHONY: install
install:; mkdir -p $(DESTINATION) && install -m 0755 evdump $(DESTINATION)
endif
