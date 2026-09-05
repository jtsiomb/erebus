#include <assert.h>
#include "erebus.h"
#include "rt.h"
#include "scene.h"
#include "shmfb.h"
#include "opt.h"


void pt_render_tile(struct tile *tile);
void pt_bgcolor(cgm_vec3 *color, cgm_ray *ray);
void pt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter);

static INLINE float frand(void);
static INLINE void sphrand(cgm_vec3 *pt, float rad);
static void primary_ray(cgm_ray *ray, int x, int y, int sample);


struct renderer pt_renderer = {
	"path tracer",
	pt_render_tile,
	pt_bgcolor,
	pt_shade
};

#ifdef USE_OIDN
static THREAD_LOCAL struct {
	cgm_vec3 albedo;
	cgm_vec3 normal;
	int valid;
} auxdata;
#endif


static struct tile *curtile;


void pt_render_tile(struct tile *tile)
{
	int i, j;
	cgm_ray ray;
	cgm_vec3 col;
	cgm_vec4 *fbptr = tile->fbptr;
#ifdef USE_OIDN
	cgm_vec3 *nptr = tile->nptr;
	cgm_vec3 *alb = tile->albptr;
#endif

	curtile = tile;

	for(i=0; i<tile->height; i++) {
		for(j=0; j<tile->width; j++) {
			if(quit) return;
#ifdef USE_OIDN
			auxdata.valid = 0;
#endif
			primary_ray(&ray, tile->x + j, tile->y + i, tile->sample);
			if(tile->sample) {
				ray_trace(&col, &ray, 1.0f, opt.max_iter);
				fbptr[j].x += col.x;
				fbptr[j].y += col.y;
				fbptr[j].z += col.z;
				fbptr[j].w++;
#ifdef USE_OIDN
				if(opt.denoise) {
					nptr[j].x += auxdata.normal.x;
					nptr[j].y += auxdata.normal.y;
					nptr[j].z += auxdata.normal.z;
					alb[j].x += auxdata.albedo.x;
					alb[j].y += auxdata.albedo.y;
					alb[j].z += auxdata.albedo.z;
				}
#endif
			} else {
				ray_trace((cgm_vec3*)(fbptr + j), &ray, 1.0f, opt.max_iter);
				fbptr[j].w = 1;
#ifdef USE_OIDN
				if(opt.denoise) {
					nptr[j] = auxdata.normal;
					alb[j] = auxdata.albedo;
				}
#endif
			}
		}
		fbptr += fb.width;
#ifdef USE_OIDN
		nptr += fb.width;
		alb += fb.width;
#endif
	}

	if(shmfb) {
		shmfb_donetile();
	} else if(opt.flags & OPT_PROGRESS) {
		atomic_int_inc(&progr_done_tiles);
	}
}

void pt_bgcolor(cgm_vec3 *color, cgm_ray *ray)
{
	*color = scn.bgcolor;
#ifdef USE_OIDN
	if(!auxdata.valid) {
		auxdata.normal = ray->dir;
		cgm_vneg(&auxdata.normal);
		auxdata.albedo = *color;
		auxdata.valid = 1;
	}
#endif
}


void pt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter)
{
	int transmit;
	cgm_vec3 v, n, out_n;
	float mrough, mtrans;
	float pdiff, pspec, rval;
	float fres;
	cgm_vec3 mcol, rcol;
	cgm_ray ray;
	struct material *mtl = hit->mtl;

	if(cgm_vdot(&hit->ray.dir, &hit->v.norm) > 0.0f) {
		cgm_vcons(&n, -hit->v.norm.x, -hit->v.norm.y, -hit->v.norm.z);
	} else {
		n = hit->v.norm;
	}

	mtlattr_vec(&mcol, hit->mtl, MATTR_COLOR, &hit->v.tex);
	mrough = mtlattr_num(hit->mtl, MATTR_ROUGHNESS, &hit->v.tex);
	mtrans = mtlattr_num(hit->mtl, MATTR_TRANSMIT, &hit->v.tex);

	mtlattr_vec(color, hit->mtl, MATTR_EMIT, &hit->v.tex);

#ifdef USE_OIDN
	if(!auxdata.valid) {
		auxdata.albedo = mcol;
		auxdata.normal = n;
		auxdata.valid = 1;
	}
#endif

	rval = frand();

	pdiff = energy * mrough;
	pspec = energy * (1.0f - mrough);
	assert(pdiff + pspec <= 1.0f);

	if(rval <= pdiff) {
		cgm_vnormalize(&n);

		/* pick diffuse direction with a cosine-weighted probability */
		sphrand(&ray.dir, 0.98f);
		cgm_vadd(&ray.dir, &n);
		cgm_vnormalize(&ray.dir);

		if(cgm_vdot(&ray.dir, &n) < 0.0f) {
			ray.dir.x = -ray.dir.x;
			ray.dir.y = -ray.dir.y;
			ray.dir.z = -ray.dir.z;
		}

		ray.origin = hit->v.pos;
		ray_trace(&rcol, &ray, pdiff, max_iter - 1);

		color->x += rcol.x * mcol.x;
		color->y += rcol.y * mcol.y;
		color->z += rcol.z * mcol.z;

	} else if(rval <= pdiff + pspec) {
		cgm_vnormalize(&n);
		ray.dir = hit->ray.dir;

		if(!mtl->metal && (transmit = mtrans > 0.0f)) {
			/* calculate fresnel factor */
			fres = fresnel(-cgm_vdot(&hit->ray.dir, &n), mtl->ior);
			if(frand() < fres) {
				goto reflect;
			}

			/* calculate refraction direction */
			if(cgm_vrefract(&ray.dir, &n, mtl->ior) == -1) {
				transmit = 0;
			}
		} else {
reflect:	transmit = 0;
			/* calculate reflection direction */
			cgm_vreflect(&ray.dir, &n);
		}

		/* pick specular direction */
		if(mrough > 0.0f) {
			sphrand(&v, mrough);
			cgm_vadd(&ray.dir, &v);
		}
		cgm_vnormalize(&ray.dir);

		if(transmit) {
			cgm_vcons(&out_n, -n.x, -n.y, -n.z);
		} else {
			out_n = n;
		}
		if(cgm_vdot(&ray.dir, &out_n) > 0.0f) {
			/* only sample rays not crashing back into the surface */
			ray.origin = hit->v.pos;
			ray_trace(&rcol, &ray, pspec, max_iter - 1);

			if(mtl->metal) {
				color->x += rcol.x * mcol.x;
				color->y += rcol.y * mcol.y;
				color->z += rcol.z * mcol.z;
			} else {
				cgm_vadd(color, &rcol);
			}
		}
	}
}


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


/* TODO: move to rt.c */
static void primary_ray(cgm_ray *ray, int x, int y, int sample)
{
	float fx = x + frand() - 0.5f;
	float fy = y + frand() - 0.5f;

	ray->origin.x = ray->origin.y = ray->origin.z = 0.0f;
	ray->dir.x = (2.0f * fx / (float)fb.width - 1.0f) * fb.aspect;
	ray->dir.y = 1.0f - 2.0f * fy / (float)fb.height;
	ray->dir.z = -zdist;
	cgm_vnormalize(&ray->dir);

	cgm_rmul_mr(ray, view_xform);
}
