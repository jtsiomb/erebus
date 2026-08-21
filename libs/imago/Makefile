libsrc = src/filelbm.c src/imago2.c src/ftmodule.c src/quant.c src/fileppm.c src/filejpeg.c src/imago_gl.c src/filetga.c src/modules.c src/filergbe.c src/byteord.c src/conv.c src/filepng.c
png = libpng/pngtrans.c libpng/pngpread.c libpng/pngread.c libpng/pngmem.c libpng/pngget.c libpng/pngrtran.c libpng/pngwtran.c libpng/pngwutil.c libpng/png.c libpng/pngtest.c libpng/pngrutil.c libpng/pngerror.c libpng/pngrio.c libpng/pngwio.c libpng/pngwrite.c libpng/pngset.c
zlib = zlib/crc32.c zlib/gzread.c zlib/deflate.c zlib/uncompr.c zlib/adler32.c zlib/compress.c zlib/trees.c zlib/gzlib.c zlib/gzclose.c zlib/infback.c zlib/gzwrite.c zlib/inftrees.c zlib/inflate.c zlib/inffast.c zlib/zutil.c
jpeg = jpeglib/jctrans.c jpeglib/jccolor.c jpeglib/jcmainct.c jpeglib/jcparam.c jpeglib/jchuff.c jpeglib/jdcoefct.c jpeglib/jdsample.c jpeglib/jidctflt.c jpeglib/jcapimin.c jpeglib/jquant1.c jpeglib/jcomapi.c jpeglib/jerror.c jpeglib/jcsample.c jpeglib/jdatadst.c jpeglib/jdapistd.c jpeglib/jcapistd.c jpeglib/jdarith.c jpeglib/jdmarker.c jpeglib/jcarith.c jpeglib/jfdctfst.c jpeglib/jdhuff.c jpeglib/jdapimin.c jpeglib/jdmainct.c jpeglib/jmemnobs.c jpeglib/jcprepct.c jpeglib/jcdctmgr.c jpeglib/jquant2.c jpeglib/jdpostct.c jpeglib/jutils.c jpeglib/jdmerge.c jpeglib/jcmarker.c jpeglib/jidctfst.c jpeglib/jccoefct.c jpeglib/jfdctflt.c jpeglib/jidctint.c jpeglib/jdinput.c jpeglib/jdcolor.c jpeglib/jdtrans.c jpeglib/jcmaster.c jpeglib/jdatasrc.c jpeglib/jdmaster.c jpeglib/jfdctint.c jpeglib/jmemmgr.c jpeglib/jcinit.c jpeglib/jddctmgr.c jpeglib/jaricom.c
src = $(libsrc) $(png) $(zlib) $(jpeg)
obj = $(src:.c=.o)
lib = libimago.a

CFLAGS = -O2 -D_LARGEFILE64_SOURCE -Ijpeglib

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
