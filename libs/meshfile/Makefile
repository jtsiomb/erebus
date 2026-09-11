obj = src/dynarr.o src/fmt3ds.o src/fmtgltf.o src/fmtjtf.o src/fmtobj.o \
	  src/fmtstl.o src/json.o src/meshfile.o src/rbtree.o src/util.o
lib = libmeshfile.a

CFLAGS = -O2

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
