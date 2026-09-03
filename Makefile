src = src/bvh.c src/geom.c src/mesh.c src/rt.c src/tpool.c src/denoise.c \
	src/image.c src/opt.c src/scene.c src/erebus.c src/main.c src/rbtree.c \
	src/shmfb.c

obj = $(src:.c=.o)
bin = erebus

alibs = libs/treestor/libtreestore.a libs/imago/libimago.a

opt = -O2
dbg = -g3

inc = -Ilibs -Ilibs/treestor -Ilibs/imago/src

CFLAGS = $(opt) $(dbg) $(inc) $(def)
LDFLAGS = $(alibs) -lm -lpthread

$(bin): $(obj) libs
	$(CC) -o $@ $(obj) $(LDFLAGS)

.c.o:
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

# --- rules for the bundled libraries ---
.PHONY: libs
libs: treestore imago

.PHONY: clean-libs
clean-libs: clean-treestore clean-imago

.PHONY: treestore
treestore:
	cd libs/treestor && $(MAKE)

.PHONY: clean-treestore
clean-treestore:
	cd libs/treestor && $(MAKE) clean

.PHONY: imago
imago:
	cd libs/imago && $(MAKE)

.PHONY: clean-imago
clean-imago:
	cd libs/imago && $(MAKE) clean
