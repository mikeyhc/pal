LDFLAGS=-lncursesw -lm -lsqlite3

all: pal
clean:
	rm -f pal

.PHONY: all clean
