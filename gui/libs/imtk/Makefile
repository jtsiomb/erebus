src = button.c checkbox.c draw.c imtk.c layout.c listbox.c progress.c slider.c \
	  state.c textbox.c
obj = $(src:.c=.o)
alib = ../../imtk.a

CFLAGS = -O2 $(dbg) -I../../src -I/usr/local/include

$(alib): $(obj)
	$(AR) rcs $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(alib)
