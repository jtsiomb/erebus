src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = xerebus

incdir = -I/usr/Motif-2.1/include -I/usr/local/include -I/usr/X11R6/include
libdir = -L/usr/Motif-2.1/lib32 -L/usr/local/lib -L/usr/X11R6/lib

warn = -std=gnu89 -pedantic -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast
dbg = -g

CFLAGS = $(warn) $(dbg) $(opt) $(incdir) -fcommon -MMD
LDFLAGS = $(libdir) -lGLw -lGL -lXm -lXt -lX11 -lm

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
