#include <assert.h>
#include "cgmath/cgmath.h"
#include "erebus.h"
#include "rt.h"
#include "scene.h"
#include "brdf.h"


void pt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int iter);


struct renderer pt_renderer = {
	"path tracer",
	bgcolor,
	pt_shade
};


static INLINE float max(float x, float y)
{
	return x > y ? x : y;
}

static INLINE float maxelem(cgm_vec3 v)
{
	return max(v.x, max(v.y, v.z));
}

static INLINE float colavg(cgm_vec3 v)
{
	return (v.x + v.y + v.z) / 3.0f;
}

void pt_shade(cgm_vec3 *out_color, struct rayhit *hit, float energy, int iter)
{
	cgm_vec3 n, hvec;
	float mrough, mtrans, mmetal;
	float specval, pval, pval_h, rval, fres, fres0, clum, slum, pdiff, pspec;
	float prefl, ptrans, contrib;
	cgm_vec3 mcol, mspec, col;
	cgm_ray ray;
	struct material *mtl = hit->mtl;

	if(cgm_vdot(&hit->vdir, &hit->v.norm) < 0.0f) {
		cgm_vneg(&hit->v.norm);
	}
	cgm_vnormalize(&hit->v.norm);
	n = hit->v.norm;

	mtlattr_vec(&mcol, mtl, MATTR_COLOR, &hit->v.tex);
	mtlattr_vec(&mspec, mtl, MATTR_SPECULAR, &hit->v.tex);
	mrough = mtlattr_num(mtl, MATTR_ROUGHNESS, &hit->v.tex);
	mtrans = mtlattr_num(mtl, MATTR_TRANSMIT, &hit->v.tex);
	mmetal = mtlattr_num(mtl, MATTR_METALLIC, &hit->v.tex);

	/* emissive initializes color result */
	mtlattr_vec(out_color, mtl, MATTR_EMIT, &hit->v.tex);

#ifdef USE_OIDN
	if(!auxdata.valid) {
		auxdata.albedo = mcol;
		cgm_vadd(&auxdata.albedo, out_color);	/* add emitted light */
		auxdata.normal = n;
		auxdata.valid = 1;
	}
#endif

	clum = colavg(mcol);
	slum = colavg(mspec);

	/* --- sample BRDF --- */
	fres0 = cgm_lerp(slum * 0.08, clum, mmetal);
	pdiff = clum / (clum + fres0);

	rval = frand();
	/*specval = maxelem(mspec);*/

	ray.origin = hit->v.pos;
	if(rval <= pdiff) {
		/* diffuse interaction */
		lambert_brdf.sample(&ray.dir, hit);
		pval = lambert_brdf.pdf(ray.dir, n, hit);
		lambert_brdf.eval(&col, n, ray.dir, hit->vdir, hit);

		contrib = colavg(col) / (pdiff * pval);
	} else {
		/* specular interaction */
/*
Within the specular branch, sample the microfacet normal first (VNDF), then
evaluate Fresnel at that specific half-vector to choose reflect vs. transmit.
*/
		ggx_brdf.sample(&hvec, hit);
		fres = fresnel(cgm_vvdot(hit->vdir, hvec), mtl->ior);

		rval = frand();
		if(mtrans <= 1e-6 || rval <= fres) {
			/* reflection */
			ray.dir = cgm_vvreflect(hit->vdir, hvec);
			pval_h = ggx_brdf.pdf(ray.dir, hvec, hit);
			pval = pval_h / (4.0f * cgm_vvdot(hit->vdir, hvec));

			prefl = (1.0f - pdiff) * (mtrans > 1e-6 ? fres : 1.0f);

			ggx_brdf.eval(&col, hvec, ray.dir, hit->vdir, hit);
			contrib = colavg(col) * cgm_vvdot(ray.dir, n) / (prefl * pval);
		} else {
			/* transmission */
			ray.dir = cgm_vvrefract(hit->vdir, hvec, mtl->ior);
			pval = ggx_brdf.pdf(ray.dir, hvec, hit);

			ptrans = (1.0f - pdiff) * (1.0f - fres);

			ggx_brdf.eval(&col, hvec, ray.dir, hit->vdir, hit);
			contrib = colavg(col) * cgm_vvdot(ray.dir, n) / (ptrans * pval);
		}
	}

/*
contribution = f(wi) * cos(wi) / (P_selected * pdf_selected(wi))

where P_selected is the discrete probability you used to pick that lobe (pdiff,
pspec*F, or pspec*(1-F)), and pdf_selected is that lobe’s own directional pdf.
*/

	ray_trace(&col, &ray, energy * contrib, iter + 1);
	cgm_vadd(out_color, &col);
}
