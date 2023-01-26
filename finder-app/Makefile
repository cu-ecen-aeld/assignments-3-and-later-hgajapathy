# Referred https://www.gnu.org/software/make/manual/make.pdf

ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC = $(CROSS_COMPILE)gcc
else
	CC = gcc
endif

CFLAGS	:= -Wall -Werror

SRC		= $(wildcard *.c)
OBJS	= $(SRC:.c=.o)
EXE		= writer

all: $(EXE)

${EXE}: $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -o $@ -c $< ${CFLAGS}

.PHONY: clean

clean:
	@rm -rf *.o ${EXE}
