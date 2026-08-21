obj = treestor.o text.o dynarr.o
lib = libtreestore.a

CFLAGS = -O2

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
