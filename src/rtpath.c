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
	cgm_vec3 n;
	float mmetal;
	float pval, rval, pdiff;
	float wdiff, wspec;
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

	mcol = mtlv->color;
	mspec = mtlv->spec;
	mmetal = mtlv->metallic;

	/* emissive initializes color result */
	*out_color = mtlv->emit;

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
		if(ggx_brdf.sample(&samp, hit) == -1) return;

		ray.dir = samp.dir;
		pval = samp.pdf;
		contrib = cgm_vvscale(samp.weight, 1.0f / (1.0f - pdiff));
	}

	ray_trace(&col, &ray, energy * maxelem(contrib), iter + 1);
	cgm_vmul(&col, &contrib);
	cgm_vadd(out_color, &col);
}
