obj = glew.o
alib = ../../glew.a

CFLAGS = -O2 -DGLEW_INCLUDE='"glew.h"' -I/usr/local/include

$(alib): $(obj)
	$(AR) rcs $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(alib)
