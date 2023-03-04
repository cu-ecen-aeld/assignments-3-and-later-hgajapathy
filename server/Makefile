# Referred https://www.gnu.org/software/make/manual/make.pdf

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Werror -Wall
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

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
