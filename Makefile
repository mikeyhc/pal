LDFLAGS=-lncursesw -lm -lsqlite3 -lzmq -g
CFLAGS=-Wall -Wextra -pedantic

all: pal pald
%: %.c
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}
clean:
	rm -f pal

.PHONY: all clean
