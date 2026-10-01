#include "cgmath/cgmath.h"
#include "erebus.h"
#include "brdf.h"
#include "rt.h"

static void lambert_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static float lambert_sample(cgm_vec3 *dir, struct rayhit *hit);

static void phong_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static float phong_sample(cgm_vec3 *dir, struct rayhit *hit);

static void ggx_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static float ggx_sample(cgm_vec3 *dir, struct rayhit *hit);


struct brdf lambert_brdf = { lambert_eval, lambert_sample };
struct brdf phong_brdf = { phong_eval, phong_sample };
struct brdf ggx_brdf = { ggx_eval, ggx_sample };


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

static float lambert_sample(cgm_vec3 *dir, struct rayhit *hit)
{
	sphrand(dir, 0.999f);
	cgm_vadd(dir, &hit->v.norm);
	cgm_vnormalize(dir);

	if(cgm_vdot(dir, &hit->v.norm) < 0.0f) {
		cgm_vneg(dir);
	}

	return cgm_vdot(dir, &hit->v.norm) / CGM_PI;
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

static float phong_sample(cgm_vec3 *dir, struct rayhit *hit)
{
	/* TODO: lafortune */
	return 0.0f;
}

static void ggx_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit)
{
}

static cgm_vec3 sample_ggx_vndf(cgm_vec3 n, cgm_vec3 v, float rough);
static float pdf_ggx_vndf(cgm_vec3 n, cgm_vec3 dir_out, cgm_vec3 dir_in, float rough);

static float ggx_sample(cgm_vec3 *dir, struct rayhit *hit)
{
	float rough;
	cgm_vec3 hvec;

	rough = mtlattr_num(hit->mtl, MATTR_ROUGHNESS, &hit->v.tex);

	hvec = sample_ggx_vndf(hit->v.norm, hit->vdir, rough);
	*dir = cgm_vvreflect(hit->vdir, hvec);

	return pdf_ggx_vndf(hit->v.norm, *dir, hit->vdir, rough);
}

/* GGX sampling routine from:
 * https://auzaiffe.wordpress.com/2024/04/15/vndf-importance-sampling-an-isotropic-distribution/
 *
 * A simpler special case for isotropic roughness, of the improved GGX sampling
 * method from: J.Dupuy, A.Benyoub, "Sampling Visible GGX Normals with Spherical
 * Caps", High-Performance Computing 2023.
 */
static cgm_vec3 sample_ggx_vndf(cgm_vec3 n, cgm_vec3 v, float rough)
{
	cgm_vec3 hvec, vloc_up, vloc_tan, vstd, cstd, vr, c, hvec_z, hvec_xy;
	float vstd_z, sin_theta, phi, tmp;

	/* split view vector into local up (parallel to normal), and tangential
	 * components
	 */
	vloc_up = cgm_vvscale(n, cgm_vvdot(n, v));
	vloc_tan = cgm_vvsub(v, vloc_up);

	/* warp to standard hemisphere */
	vstd = cgm_vvnormalize(cgm_vvadd(cgm_vvscale(vloc_tan, rough), vloc_up));
	cgm_vneg(&vstd);

	/* sample spherical cap */
	vstd_z = cgm_vvdot(vstd, n);
	cstd.z = 1.0f - frand() * (1.0f + vstd_z);
	tmp = 1.0f - cstd.z * cstd.z;
	sin_theta = sqrt(tmp > 1.0f ? 1.0f : tmp);
	phi = CGM_PI * 2.0f * frand() - CGM_PI;
	cstd.x = sin_theta * cos(phi);
	cstd.y = sin_theta * sin(phi);

	/* reflect sample to align with normal */
	cgm_vcons(&vr, n.x, n.y, n.z + 1.000001f);
	c = cgm_vvscale(vr, cgm_vvdot(vr, cstd) / vr.z);
	cgm_vsub(&c, &cstd);

	/* compute halfway direction */
	hvec = cgm_vvadd(c, vstd);
	hvec_z = cgm_vvscale(n, cgm_vvdot(n, vstd));
	hvec_xy = cgm_vvsub(hvec_z,  hvec);

	cgm_vvadd(cgm_vvscale(hvec_xy, rough), hvec_z);
	return cgm_vvnormalize(hvec);
}

static float pdf_ggx_vndf(cgm_vec3 n, cgm_vec3 dir_out, cgm_vec3 dir_in, float rough)
{
	cgm_vec3 hvec;
	float rough_sq, zh, zin, invlen, sigma_std, sigma_i, nrmn;

	rough_sq = rough * rough;
	hvec = cgm_vvnormalize(cgm_vvadd(dir_in, dir_out));
	zh = cgm_vvdot(hvec, n);
	zin = cgm_vvdot(dir_in, n);
	invlen = 1.0f / sqrt(zin * zin * (1.0f - rough_sq) + rough_sq);
	sigma_std = zin * invlen * 0.5f + 0.5f;
	sigma_i = sigma_std / invlen;
	nrmn = zh * zh * (rough_sq - 1.0f) + 1.0f;
	return rough_sq / (CGM_PI * 4.0f * nrmn * nrmn * sigma_i);
}
