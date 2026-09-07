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
	cgm_vec3 hdir;
	float shin, ndoth;

	mtlattr_vec(color, hit->mtl, MATTR_SPECULAR, &hit->v.tex);
	shin = mtlattr_num(hit->mtl, MATTR_SHININESS, &hit->v.tex);

	hdir = vdir;
	cgm_vadd(&hdir, &ldir);
	cgm_vnormalize(&hdir);

	if((ndoth = cgm_vdot(&n, &hdir)) < 0.0f) {
		cgm_vcons(color, 0, 0, 0);
		return;
	}

	cgm_vscale(color, pow(ndoth, shin));
}

static void phong_sample(cgm_vec3 *dir, struct rayhit *hit)
{
}
