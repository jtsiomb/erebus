#ifndef IMAGE_H_
#define IMAGE_H_

struct image {
	char *name;
	int width, height;
	unsigned int xmask, ymask, xshift;
	float *pixels;
	float *alpha;
};

int load_image(struct image *img, const char *fname);
void destroy_image(struct image *img);

int add_image(struct image *img);
int remove_image(const char *name);
struct image *get_image(const char *name);

void dbg_dump_images(void);

#endif	/* IMAGE_H_ */
