LDFLAGS=-lm $(shell pkgconf --libs ncursesw libzmq sqlite3)
CFLAGS=-Wall -Wextra -pedantic -g

all: pal pald palc
%: src/%.c
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}
clean:
	rm -f pal pald

.PHONY: all clean
