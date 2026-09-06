#include "erebus.h"
#include "brdf.h"
#include "rt.h"

static void lambert_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static void lambert_sample(cgm_vec3 *dir, struct rayhit *hit);

static void phong_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static void phong_sample(cgm_vec3 *dir, struct rayhit *hit);


struct brdf lambert_brdf = { lambert_eval, lambert_sample };
struct brdf phong_brdf = { phong_eval, phong_sample };


static void lambert_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit)
{
	float ndotl;

	if((ndotl = cgm_vdot(&n, &ldir)) < 0.0f) {
		ndotl = 0.0f;
	}

	mtlattr_vec(color, hit->mtl, MATTR_COLOR, &hit->v.tex);
	cgm_vscale(color, ndotl);
}

static void lambert_sample(cgm_vec3 *dir, struct rayhit *hit)
{
}

static void phong_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit)
{
	cgm_vec3 v;
	cgm_vec3 kd, ks, ke;
	float shin;
	struct material *mtl = hit->mtl;

	mtlattr_vec(&kd, hit->mtl, MATTR_COLOR, &hit->v.tex);
	mtlattr_vec(&ks, hit->mtl, MATTR_SPECULAR, &hit->v.tex);

	mtlattr_vec(color, hit->mtl, MATTR_EMIT, &hit->v.tex);
}

static void phong_sample(cgm_vec3 *dir, struct rayhit *hit)
{
}
