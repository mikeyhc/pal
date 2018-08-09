LDFLAGS=-lncursesw -lm -lsqlite3 -lzmq -g

all: pal
%: %.c
	${CC} -o $@ $^ ${LDFLAGS}
clean:
	rm -f pal

.PHONY: all clean
