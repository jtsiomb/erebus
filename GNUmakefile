-include cfg.mk

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = erebus

alibs = libs/treestor/libtreestore.a libs/imago/libimago.a \
		libs/meshfile/libmeshfile.a

opt = -O0 -ffast-math -fno-strict-aliasing
dbg = -g
warn = -pedantic -Wall

inc = -Ilibs -Ilibs/treestor -Ilibs/imago/src -Ilibs/meshfile/src

CFLAGS = -std=gnu89 $(warn) $(opt) $(dbg) $(inc) $(def) -MMD
LDFLAGS = $(alibs) -lm

ifeq ($(oidn), true)
	def += -DUSE_OIDN
	LDFLAGS += -Llibs/oidn -loidn -loidn_device_cpu -loidn_core -ltbb
	LDCC = g++
else
	LDCC = gcc
endif

$(bin): $(obj) libs
	$(LDCC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

# --- rules for the bundled libraries ---
.PHONY: libs
libs: treestore imago meshfile

.PHONY: clean-libs
clean-libs: clean-treestore clean-imago clean-meshfile

.PHONY: treestore
treestore:
	$(MAKE) -C libs/treestor

.PHONY: clean-treestore
clean-treestore:
	$(MAKE) -C libs/treestor clean

.PHONY: imago
imago:
	$(MAKE) -C libs/imago

.PHONY: clean-imago
clean-imago:
	$(MAKE) -C libs/imago clean

.PHONY: meshfile
meshfile:
	$(MAKE) -C libs/meshfile

.PHONY: clean-meshfile
clean-meshfile:
	$(MAKE) -C libs/meshfile clean
