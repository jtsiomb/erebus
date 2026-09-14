#ifndef RT_H_
#define RT_H_

#include <stdio.h>
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
	MATTR_COLOR,		/* PBR: albedo, phong: diffuse */
	MATTR_SPECULAR,		/* phong: specular color */
	MATTR_EMIT,
	MATTR_TRANSMIT,
	MATTR_ROUGHNESS,	/* PBR: roughness */
	MATTR_METALLIC,		/* PBR: metallic */
	MATTR_SHININESS,	/* phong: specular exponent */

	MATTR_REFLECT,		/* phong: reflectivity */

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

struct path_aux_data {
	cgm_vec3 albedo;
	cgm_vec3 normal;
	int valid;
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
	void (*bgcolor)(cgm_vec3 *color, cgm_ray *ray);
	void (*shade)(cgm_vec3 *color, struct rayhit *hit, float energy, int niter);
};

extern struct framebuffer fb;
extern struct thread_pool *tpool;
extern float view_xform[16];
extern float zdist;

extern THREAD_LOCAL struct tile *curtile;
extern THREAD_LOCAL struct path_aux_data auxdata;

extern struct renderer rend;

/* available renderers */
extern struct renderer rt_renderer, pt_renderer;


int fbsize(int width, int height);
void set_fov(float fov);

void render(int samplenum);

void primary_ray(cgm_ray *ray, int x, int y, int sample);
void ray_trace(cgm_vec3 *color, cgm_ray *ray, float energy, int max_iter);

void bgcolor(cgm_vec3 *color, cgm_ray *ray);

float fresnel(float costheta, float ior);

float mtlattr_num(struct material *mtl, int attr, cgm_vec2 *uv);
void mtlattr_vec(cgm_vec3 *res, struct material *mtl, int attr, cgm_vec2 *uv);
void tex_lookup(cgm_vec3 *res, struct image *img, float u, float v);
void mtlprint(FILE *fp, struct material *mtl);

static INLINE float frand(void)
{
	return tinymt32_generate_float(&curtile->rndstate);
}

static INLINE void sphrand(cgm_vec3 *pt, float rad)
{
	float u, v, theta, phi;

	u = frand();
	v = frand();

	theta = 2.0f * M_PI * u;
	phi = acos(2.0f * v - 1.0f);

	pt->x = cos(theta) * sin(phi) * rad;
	pt->y = sin(theta) * sin(phi) * rad;
	pt->z = cos(phi) * rad;
}


#endif	/* RT_H_ */
