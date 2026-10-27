src = $(wildcard src/*.c)
obj = $(src:.c=.o) ../src/shmfb.o
dep = $(src:.c=.d)
bin = erebus-gui

alibs = imtk.a glew.a

opt = -O3 -fno-strict-aliasing
dbg = -g
warn = -pedantic -Wall
inc = -I../src -Ilibs/glew -Ilibs/imtk

CFLAGS = -std=gnu89 $(warn) $(opt) $(dbg) $(inc) -MMD
LDFLAGS = $(alibs) -lGL -lX11 -lm

$(bin): $(obj) $(alibs)
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


imtk.a:
	$(MAKE) -C libs/imtk

glew.a:
	$(MAKE) -C libs/glew

.PHONY: cleanlibs
cleanlibs:
	$(MAKE) -C libs/imtk clean
	$(MAKE) -C libs/glew clean
