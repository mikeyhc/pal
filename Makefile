LDFLAGS=-lncursesw -lm -lsqlite3 -g

all: pal
%: %.c
	${CC} -o $@ $^ ${LDFLAGS}
clean:
	rm -f pal

.PHONY: all clean
