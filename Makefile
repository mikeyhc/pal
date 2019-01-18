LDFLAGS=-lncursesw -lm -lsqlite3 -lzmq -g
CFLAGS=-Wall -Wextra -pedantic

all: pal pald palc
%: src/%.c
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}
clean:
	rm -f pal pald

.PHONY: all clean
