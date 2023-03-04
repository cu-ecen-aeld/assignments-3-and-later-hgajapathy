# Referred https://www.gnu.org/software/make/manual/make.pdf

CC = $(CROSS_COMPILE)gcc
# CFLAGS	:= -g -Wall -Werror
CFLAGS	:= -Wall -Werror
LFLAGS	:= -pthread -lrt

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
