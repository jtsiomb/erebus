src = $(wildcard src/*.c) libs/glew/glew.c
obj = $(src:.c=.o) ../src/shmfb.o
dep = $(src:.c=.d)
bin = erebus-gui

dbg = -g
warn = -pedantic -Wall
inc = -I../src -Ilibs/glew

CFLAGS = -std=gnu89 $(warn) $(opt) $(dbg) $(inc) -MMD
LDFLAGS = -lGL -lX11 -lm

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

libs/glew/glew.o: libs/glew/glew.c
	$(CC) -o $@ -c $< $(CFLAGS) -DGLEW_INCLUDE='"glew.h"'

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
