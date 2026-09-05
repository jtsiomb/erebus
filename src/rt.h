#ifndef RT_H_
#define RT_H_

#include "cgmath/cgmath.h"
#include "image.h"
#include "tpool.h"
#include "tinymt32.h"

struct rayhit;

struct mtlattr {
	cgm_vec3 value;
	struct image *tex;
};

enum {
	MATTR_COLOR,
	MATTR_EMIT,
	MATTR_TRANSMIT,
	MATTR_ROUGHNESS,

	NUM_MATTR
};

struct material {
	char *name;
	struct mtlattr attr[NUM_MATTR];
	float ior;
	int metal;
	struct image *mask;
};

struct framebuffer {
	int width, height;
	float aspect;
	cgm_vec4 *pixels;
#ifdef USE_OIDN
	cgm_vec3 *albedo;
	cgm_vec3 *normals;
#endif
};

struct tile {
	int x, y, width, height;
	int sample;
	cgm_vec4 *fbptr;
#ifdef USE_OIDN
	cgm_vec3 *nptr, *albptr;
#endif

	tinymt32_t rndstate;
};

struct renderer {
	const char *name;
	void (*render_tile)(struct tile*);
	void (*bgcolor)(cgm_vec3 *color, cgm_ray *ray);
	void (*shade)(cgm_vec3 *color, struct rayhit *hit, float energy, int niter);
};

extern struct framebuffer fb;
extern struct thread_pool *tpool;
extern float view_xform[16];
extern float zdist;

extern struct renderer rend;

/* available renderers */
extern struct renderer rt_renderer, pt_renderer;


int fbsize(int width, int height);
void set_fov(float fov);

void render(int samplenum);

void ray_trace(cgm_vec3 *color, cgm_ray *ray, float energy, int max_iter);

void bgcolor(cgm_vec3 *color, cgm_ray *ray);

float fresnel(float costheta, float ior);

float mtlattr_num(struct material *mtl, int attr, cgm_vec2 *uv);
void mtlattr_vec(cgm_vec3 *res, struct material *mtl, int attr, cgm_vec2 *uv);
void tex_lookup(cgm_vec3 *res, struct image *img, float u, float v);

#endif	/* RT_H_ */
