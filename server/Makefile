# Referred https://www.gnu.org/software/make/manual/make.pdf

CC = $(CROSS_COMPILE)gcc
CFLAGS	:= -Wall -Werror

SRC		= $(wildcard *.c)
OBJS	= $(SRC:.c=.o)
EXE		= aesdsocket

all: $(EXE)

${EXE}: $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -o $@ -c $< ${CFLAGS}

.PHONY: clean

clean:
	rm -rf *.o ${EXE}
