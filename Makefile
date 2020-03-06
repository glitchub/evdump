CFLAGS += -Wall -Werror -Wno-maybe-uninitialized -std=gnu99

PROGS = evdump evls

.PHONY: default
default: ${PROGS}

${PROGS}: %: %.c

*.c: input.generated.h

# Create a header file from interesting input.h definitions
input.generated.h: input.generated
	@echo "// Generated file, do not check in!" > $@
	@$(foreach t,EV SYN KEY BTN REL ABS SW MSC LED REP SND BUS FF_STATUS, { \
		printf 'const char * ed_$(t)(int n)\n{\n  switch(n)\n  {\n'; \
                awk '$$2~/^$(t)_/ && $$3~/^[0-9]/{print $$2,$$3}' $< | \
                    while read a b c; do echo $$a $$(($$b)); done | \
                        awk '{if ($$2<65536 && !u[$$2]++) {print "    case " $$2 ": return \"" $$1 "\";"}}' \
			    | sort -k2 -n; \
		printf '  }\n  return NULL;\n}\n'; \
	} >> $@;)

# Preprocess linux/input.h macro definitions to a temp file, from which we will
# extract various event definitions. Don't access input.h directly because
# newer linux's construct it from nested #includes. Also this gives the right
# stuff if sysroot is in use.
input.generated:
	echo "// Generated file, do not check in!" > $@
	$(CC) $(CFLAGS) -E -dM -include "linux/input.h" - < /dev/null >> $@

.PHONY: clean
clean:; rm -f $(PROGS) input.generated*
