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
	float ndotv, ndotl, geom;
	float specval, pval, pval_h, rval, fres, pdiff, pspec;
	float wdiff, wspec;
	float prefl, ptrans;
	float ior;
	cgm_vec3 mcol, mspec, col, cdiff, fres0, contrib;
	cgm_ray ray;
	struct brdf_sample samp;
	struct material *mtl = hit->mtl;
	struct hitmaterial *mtlv = &hit->mtlval;

	eval_material(mtlv, hit->mtl, hit->v.tex, MATTR_PBR_BITS);

	if(cgm_vdot(&hit->vdir, &hit->v.norm) < 0.0f) {
		cgm_vneg(&hit->v.norm);
		mtlv->ior = 1.0f / mtl->ior;
	} else {
		mtlv->ior = mtl->ior;
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

	/* --- sample BRDF --- */
	ray.origin = hit->v.pos;

	cdiff = cgm_vvscale(mcol, 1.0f - mmetal);
	fres0 = cgm_vvlerp(cgm_vvscale(mspec, 0.08f), mcol, mmetal);

	wdiff = colavg(cdiff);
	wspec = colavg(fres0);

	if(wdiff + wspec < 1e-6f) return;
	pdiff = wdiff / (wdiff + wspec);	/* probability of diffuse interaction */

	rval = frand();

	if(rval < pdiff) {
		/* diffuse interaction */
		lambert_brdf.sample(&samp, hit);
		ray.dir = samp.dir;
		pval = samp.pdf;
		contrib = cgm_vvscale(samp.weight, 1.0f / pdiff);

	} else {
		/* specular interaction */
		ggx_brdf.sample(&hvec, hit);
		/*fres = fresnel(cgm_vvdot(hit->vdir, hvec), ior);

		rval = frand();
		if(mtrans <= 1e-6 || rval <= fres) {*/
			/* reflection */
			ray.dir = cgm_vvreflect(hit->vdir, hvec);

			if((ndotl = cgm_vvdot(n, ray.dir)) <= 0.0f) {
				return;
			}
			ndotv = cgm_vvdot(n, hit->vdir);
			contrib = fresnel_rgb(cgm_vvdot(hit->vdir, hvec), fres0);
			geom = ggx_g2_over_g1(ndotv, ndotl, mrough) / (1.0f - pdiff);
			cgm_vscale(&contrib, geom);

			/*pval = ggx_brdf.pdf(ray.dir, hvec, hit);
			prefl = (1.0f - pdiff) * (mtrans > 1e-6 ? fres : 1.0f);
			*/
#if 0
		} else {
			/* transmission */
			return;
		}
#endif
	}

	ray_trace(&col, &ray, energy * maxelem(contrib), iter + 1);
	cgm_vmul(&col, &contrib);
	cgm_vadd(out_color, &col);
}
