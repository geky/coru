TARGET = coru.a
ifneq ($(wildcard test.c main.c),)
override TARGET = coru
endif

CC ?= gcc
AR ?= ar
SIZE ?= size

SRC += $(wildcard *.c)
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)
ASM := $(SRC:.c=.s)

TEST := $(patsubst tests/%.sh,%,$(wildcard tests/test_*))

SHELL = /bin/bash -o pipefail

ifdef DEBUG
override CFLAGS += -O0 -g3
else
override CFLAGS += -Os
endif
ifdef WORD
override CFLAGS += -m$(WORD)
endif
override CFLAGS += -I.
override CFLAGS += -std=c99 -Wall -pedantic
override CFLAGS += -Wextra -Wshadow -Wjump-misses-init
# Remove missing-field-initializers because of GCC bug
override CFLAGS += -Wno-missing-field-initializers


all: $(TARGET)

asm: $(ASM)

size: $(OBJ)
	$(SIZE) -t $^

.SUFFIXES:
test: \
		test_simple \
		test_params \
		test_corners \
		test_overflow \
		test_parallel \
		test_nested
	@rm test.c
test_%: tests/test_%.sh
	@/bin/bash $<

-include $(DEP)

coru: $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

%.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) -c -MMD $(CFLAGS) $< -o $@

%.s: %.c
	$(CC) -S $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET)
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(ASM)
