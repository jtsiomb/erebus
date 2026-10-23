#include "cgmath/cgmath.h"
#include "erebus.h"
#include "brdf.h"
#include "rt.h"

static void lambert_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static int lambert_sample(struct brdf_sample *samp, struct rayhit *hit);
static float lambert_pdf(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit);

static void phong_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static int phong_sample(struct brdf_sample *samp, struct rayhit *hit);
static float phong_pdf(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit);

static void ggx_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit);
static int ggx_sample(struct brdf_sample *samp, struct rayhit *hit);
static float ggx_pdf(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit);


struct brdf lambert_brdf = { lambert_eval, lambert_sample, lambert_pdf };
struct brdf phong_brdf = { phong_eval, phong_sample, phong_pdf };
struct brdf ggx_brdf = { ggx_eval, ggx_sample, ggx_pdf };


static void lambert_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit)
{
	float ndotl;

	if((ndotl = cgm_vdot(&n, &ldir)) < 0.0f) {
		ndotl = 0.0f;
	}

	/*mtlattr_vec(color, hit->mtl, MATTR_COLOR, &hit->v.tex);
	cgm_vscale(color, ndotl);*/
	*color = cgm_vvscale(hit->mtlval.color, ndotl);
}

static int lambert_sample(struct brdf_sample *samp, struct rayhit *hit)
{
	cgm_vec3 dir, mcol;
	float mmetal;

	/* pick direction */
	sphrand(&dir, 0.9999f);
	cgm_vadd(&dir, &hit->v.norm);
	cgm_vnormalize(&dir);

	samp->dir = dir;

	/* compute sampling weight */
	/*mtlattr_vec(&mcol, hit->mtl, MATTR_COLOR, &hit->v.tex);
	mmetal = mtlattr_num(hit->mtl, MATTR_METALLIC, &hit->v.tex);
	*/
	mcol = hit->mtlval.color;
	mmetal = hit->mtlval.metallic;

	samp->weight = cgm_vvscale(mcol, 1.0f - mmetal);

	samp->pdf = lambert_pdf(dir, hit->v.norm, hit);
	samp->transmit = 0;
	return 0;
}

static float lambert_pdf(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit)
{
	return cgm_vvdot(dir, n) / CGM_PI;
}

static void phong_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit)
{
	cgm_vec3 hdir;
	float ndoth, shin;

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

static int phong_sample(struct brdf_sample *samp, struct rayhit *hit)
{
	/* TODO lafortune */
	return -1;
}

static float phong_pdf(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit)
{
	/* TODO lafortune */
	return 0.0f;
}

static void ggx_eval(cgm_vec3 *color, cgm_vec3 n, cgm_vec3 ldir, cgm_vec3 vdir,
		struct rayhit *hit)
{
	/* TODO */
}

static cgm_vec3 sample_ggx_vndf(cgm_vec3 n, cgm_vec3 v, float rough);
static float pdf_ggx_vndf(cgm_vec3 n, cgm_vec3 dir_out, cgm_vec3 dir_in, float rough);

static int ggx_sample(struct brdf_sample *samp, struct rayhit *hit)
{
	cgm_vec3 hvec, fres0_rgb, fres_rgb, mcol, mspec;
	float ndotv, ndotl, geom, fres0, fres, mrough, mmetal, ior;
	float rval;

	/*mtlattr_vec(&mcol, hit->mtl, MATTR_COLOR, &hit->v.tex);
	mtlattr_vec(&mspec, hit->mtl, MATTR_SPECULAR, &hit->v.tex);
	mmetal = mtlattr_num(hit->mtl, MATTR_METALLIC, &hit->v.tex);*/
	mcol = hit->mtlval.color;
	mspec = hit->mtlval.spec;
	mmetal = hit->mtlval.metallic;

	mrough = hit->mtlval.rough;	/* mtlval.rough already clamped to be > 0 */
	ior = hit->mtlval.ior;	/* mtlval.ior has been inverted if ray is exiting */

	/* generate sample direction */

	hvec = sample_ggx_vndf(hit->v.norm, hit->vdir, mrough);
	samp->dir = cgm_vvreflect(cgm_vvneg(hit->vdir), hvec);

	/* calculate sample weight */
	if((ndotl = cgm_vvdot(hit->v.norm, samp->dir)) <= 0.0f) {
		return -1;	/* reflection below the surface */
	}
	ndotv = cgm_vvdot(hit->v.norm, hit->vdir);
	geom = ggx_g2_over_g1(ndotv, ndotl, mrough);

	fres0 = (hit->mtl->ior - 1.0f) / (hit->mtl->ior + 1.0f);
	fres0 *= fres0;

	fres0_rgb = cgm_vvlerp(cgm_vvec(fres0, fres0, fres0), mcol, mmetal);
	fres_rgb = fresnel_rgb(cgm_vvdot(hit->vdir, hvec), fres0_rgb);

	samp->weight = cgm_vvscale(fres_rgb, geom);

	/* pdf */
	samp->pdf = ggx_pdf(samp->dir, hit->v.norm, hit);

	samp->transmit = 0;
	return 0;
}

static float ggx_pdf(cgm_vec3 dir, cgm_vec3 n, struct rayhit *hit)
{
	return pdf_ggx_vndf(n, dir, hit->vdir, hit->mtlval.rough);
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

	/* sample spherical cap */
	vstd_z = cgm_vvdot(vstd, n);
	cstd.z = 1.0f - frand() * (1.0f + vstd_z);
	tmp = 1.0f - cstd.z * cstd.z;
	sin_theta = tmp > 0.0f ? sqrt(tmp) : 0.0f;
	phi = CGM_PI * 2.0f * frand() - CGM_PI;
	cstd.x = sin_theta * cos(phi);
	cstd.y = sin_theta * sin(phi);

	/* reflect sample to align with normal */
	cgm_vcons(&vr, n.x, n.y, n.z + 1.000001f);
	c = cgm_vvscale(vr, cgm_vvdot(vr, cstd) / vr.z);
	cgm_vsub(&c, &cstd);

	/* compute halfway direction */
	hvec = cgm_vvadd(c, vstd);
	hvec_z = cgm_vvscale(n, cgm_vvdot(n, hvec));
	hvec_xy = cgm_vvsub(hvec,  hvec_z);

	hvec = cgm_vvadd(cgm_vvscale(hvec_xy, rough), hvec_z);
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

static float ggx_lambda(float c, float asq)
{
	if(c < 1e-4f) c = 1e-4f;
	return 0.5f * (sqrt(asq + (1.0f - asq) * c * c) / c - 1.0f);
}

float ggx_g2_over_g1(float co, float ci, float rough)
{
	float asq, lo, li;
	asq = rough * rough;
	lo = ggx_lambda(co, asq);
	li = ggx_lambda(ci, asq);
	return (1.0f + lo) / (1.0f + lo + li);
}
