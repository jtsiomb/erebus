#include "rt.h"
#include "postproc.h"
#include "util.h"

void post_gamma(float gamma)
{
	int i, npixels;
	cgm_vec4 *fbptr;
	float inv_gamma;

	if(gamma == 1.0f || gamma <= 0.0f) return;

	inv_gamma = 1.0f / gamma;

	fbptr = fb.pixels;
	npixels = fb.width * fb.height;

	for(i=0; i<npixels; i++) {
		fbptr->x = pow(fbptr->x, inv_gamma);
		fbptr->y = pow(fbptr->y, inv_gamma);
		fbptr->z = pow(fbptr->z, inv_gamma);
		fbptr++;
	}
}

void post_reinhard(void)
{
	int i, npixels = fb.width * fb.height;
	cgm_vec4 *fbptr = fb.pixels;

	for(i=0; i<npixels; i++) {
		fbptr->x /= 1.0f + fbptr->x;
		fbptr->y /= 1.0f + fbptr->y;
		fbptr->z /= 1.0f + fbptr->z;
		fbptr++;
	}
}

static INLINE float acesfilm(float x)
{
	float res = (x * (x * 2.51 + 0.03)) / (x * (x * 2.43 + 0.59) + 0.14);
	if(res < 0.0f) return 0.0f;
	if(res > 1.0f) return 1.0f;
	return res;
}

void post_acesfilm(void)
{
	int i, npixels = fb.width * fb.height;
	cgm_vec4 *fbptr = fb.pixels;

	for(i=0; i<npixels; i++) {
		fbptr->x = acesfilm(fbptr->x * 0.6f);
		fbptr->y = acesfilm(fbptr->y * 0.6f);
		fbptr->z = acesfilm(fbptr->z * 0.6f);
		fbptr++;
	}
}
