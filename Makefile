LDFLAGS=-lncursesw -lm -lsqlite3

all: pal
%: %.c
	${CC} -o $@ $^ ${LDFLAGS}
clean:
	rm -f pal

.PHONY: all clean
