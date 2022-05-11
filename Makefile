target  ?= 2rom
objects := 2rom.o buildrom.o mapfile.o hexdump.o

libs:=

EXTRAS += -fsanitize=undefined -fsanitize=null -fcf-protection=full -fstack-protector-all -fstack-check -Wimplicit-fallthrough -flto

ifdef libs
LDLIBS  += $(shell pkg-config --libs   ${libs})
CFLAGS  += $(shell pkg-config --cflags ${libs})
endif

LDFLAGS += ${EXTRAS}
CFLAGS  += -std=gnu2x -Og -ggdb ${EXTRAS}

.PHONY: all
all:	$(target)

.PHONY: clean
clean:
	rm -f $(target) $(objects)

$(target): $(objects)
