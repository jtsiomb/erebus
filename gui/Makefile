src = src/main.c src/sdr.c src/miniglut.c libs/glew/glew.c
obj = $(src:.c=.o) ../src/shmfb.o
bin = erebus-gui

alibs = imtk.a

opt = -O3
dbg = -g3
inc = -I../src -I../libs -Ilibs/glew -Ilibs/imtk -I/usr/local/include

CFLAGS = $(opt) $(dbg) $(inc)
LDFLAGS = -L/usr/local/lib $(alibs) -lGL -lX11 -lm

$(bin): $(obj) $(alibs)
	$(CC) -o $@ $(obj) $(LDFLAGS)

libs/glew/glew.o: libs/glew/glew.c
	$(CC) -o $@ -c $< $(CFLAGS) -DGLEW_INCLUDE='"glew.h"'

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

imtk.a:
	cd libs/imtk; $(MAKE)

.PHONY: cleanlibs
cleanlibs:
	cd libs/imtk; $(MAKE) clean
