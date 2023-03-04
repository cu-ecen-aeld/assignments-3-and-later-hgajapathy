# Referred https://www.gnu.org/software/make/manual/make.pdf

# If variables aren't define, define them
ifeq ($(CC),)
	CC = gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -Werror -Wall
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

SRC		= $(wildcard *.c)
OBJS	= $(SRC:.c=.o)
EXE		= aesdsocket

all: $(EXE)

${EXE}: $(OBJS)
	$(CC) -o $@ $^ ${LFLAGS}

%.o: %.c
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean

clean:
	rm -rf *.o ${EXE}
