obj = src/main.o src/ui.o src/xmutil.o
bin = xerebus

incdir = -I/usr/Motif-2.1/include -I/usr/local/include -I/usr/X11R6/include
libdir = -L/usr/Motif-2.1/lib32 -L/usr/local/lib -L/usr/X11R6/lib

CFLAGS = -g $(incdir)
LDFLAGS = $(libdir) -lGLw -lGL -lXm -lXt -lX11 -lm

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
