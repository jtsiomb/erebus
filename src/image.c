#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imago2.h"
#include "image.h"
#include "rbtree.h"


static struct rbtree *imgdb;


static int ispow2(unsigned int x);
static int calc_shift(int x);


int load_image(struct image *img, const char *fname)
{
	int has_alpha = 0;
	unsigned int i, npixels;
	float *dest, *ptr;

	printf("loading image: %s\n", fname);
	if(!(img->pixels = img_load_pixels(fname, &img->width, &img->height, IMG_FMT_RGBAF))) {
		fprintf(stderr, "load_image: failed to load %s\n", fname);
		return -1;
	}

	/* split out the alpha channel */
	npixels = img->width * img->height;
	ptr = img->pixels;
	for(i=0; i<npixels; i++) {
		if(ptr[3] < 1.0f) {
			has_alpha = 1;
			break;
		}
		ptr += 4;
	}

	if(has_alpha) {
		if(!(img->alpha = malloc(npixels * sizeof(float)))) {
			fprintf(stderr, "load_image: failed to split out alpha channel from %s\n", fname);
			goto conv_rgb;
		}

		dest = img->alpha;
		ptr = img->pixels + 3;
		for(i=0; i<npixels; i++) {
			*dest++ = *ptr;
			ptr += 4;
		}
	} else {
		img->alpha = 0;
	}

conv_rgb:
	dest = ptr = img->pixels;
	for(i=1; i<npixels; i++) {
		dest += 3;
		ptr += 4;
		memmove(dest, ptr, 3 * sizeof(float));
	}

	if(ispow2(img->width)) {
		img->xmask = img->width - 1;
		img->xshift = calc_shift(img->width);
	} else {
		img->xmask = img->xshift = 0;
	}

	if(ispow2(img->height)) {
		img->ymask = img->height - 1;
	} else {
		img->ymask = 0;
	}

	img->name = strdup(fname);
	return 0;
}

void destroy_image(struct image *img)
{
	img_free_pixels(img->pixels);
	img->pixels = 0;
	free(img->alpha);
	img->alpha = 0;
}

static void delfunc(struct rbnode *node, void *cls)
{
	destroy_image(node->data);
}

int add_image(struct image *img)
{
	if(!imgdb) {
		if(!(imgdb = rb_create(RB_KEY_STRING))) {
			fprintf(stderr, "add_image: failed to create image database\n");
			return -1;
		}
		rb_set_delete_func(imgdb, delfunc, 0);
	}
	return rb_insert(imgdb, img->name, img);
}

int remove_image(const char *name)
{
	if(!imgdb) return -1;

	return rb_delete(imgdb, (void*)name);
}

struct image *get_image(const char *name)
{
	struct rbnode *node;
	struct image *img;

	if(imgdb && (node = rb_find(imgdb, (char*)name))) {
		return rb_node_data(node);
	}

	if(!(img = malloc(sizeof *img))) {
		fprintf(stderr, "get_image: failed to allocate image\n");
		return 0;
	}
	if(load_image(img, name) == -1) {
		free(img);
		return 0;
	}
	add_image(img);
	return img;
}

void dbg_dump_images(void)
{
	int res, n = 0;
	struct rbnode *rbn;
	struct image *img;
	char pathbuf[256], basename[256], *fname, *ptr;

	printf("dbg_dump_images\n");

	rb_begin(imgdb);
	while((rbn = rb_next(imgdb))) {
		img = rbn->data;
		fname = rbn->key;
		if((ptr = strrchr(fname, '/'))) {
			strcpy(basename, ptr + 1);
		} else {
			strcpy(basename, fname);
		}
		if((ptr = strrchr(basename, '.'))) {
			*ptr = 0;
		}
		sprintf(pathbuf, "img%02d-%s.ppm", n, basename);
		res = img_save_pixels(pathbuf, img->pixels, img->width, img->height, IMG_FMT_RGBF);
		printf(" - %s ... %s\n", pathbuf, res == -1 ? "failed" : "done");

		if(img->alpha) {
			sprintf(pathbuf, "img%02d-alpha.ppm", n);
			res = img_save_pixels(pathbuf, img->alpha, img->width, img->height, IMG_FMT_GREYF);
			printf(" - %s ... %s\n", pathbuf, res == -1 ? "failed" : "done");
		}
		n++;
	}
}

static int ispow2(unsigned int x)
{
	return (x & (x - 1)) == 0;
}

static int calc_shift(int x)
{
	int count = 0;
	while(x) {
		x >>= 1;
		count++;
	}
	return count - 1;
}
