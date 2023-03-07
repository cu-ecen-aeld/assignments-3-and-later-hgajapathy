# Referred https://www.gnu.org/software/make/manual/make.pdf

CC 		?= $(CROSS_COMPILE)gcc
CFLAGS 	?= -g -Werror -Wall
LDFLAGS ?= -pthread

SRC		= $(wildcard *.c)
OBJS	= $(SRC:.c=.o)
EXE		= aesdsocket

all: $(EXE)

${EXE}: $(OBJS)
	$(CC) -o $@ $^ ${LDFLAGS}

%.o: %.c
	$(CC) -c ${CFLAGS} $< -o $@

.PHONY: clean

clean:
	rm -rf *.o ${EXE}
