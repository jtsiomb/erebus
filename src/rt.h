#ifndef RTW_H_
#define RTW_H_

struct rt_block {
	int frm, sample;
	int x, y, w, h;
	struct rt_block *next;
};

int fbwidth, fbheight;
float *fbpixels;
int cur_frame, cur_sample;

int rt_init(int width, int height);
void rt_cleanup(void);

void rt_clear(void);
void rt_render(int nsamples);
struct rt_block *rt_begin_update(void);
void rt_end_update(void);

void redraw(void);

#endif	/* RTW_H_ */
