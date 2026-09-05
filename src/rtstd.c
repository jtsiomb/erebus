#include "erebus.h"

void rt_render_tile(struct tile *tile);
void rt_bgcolor(cgm_vec3 *color, cgm_ray *ray);
void rt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter);

struct renderer rt_renderer = {
	"ray tracer",
	rt_render_tile,
	rt_bgcolor,
	rt_shade
};

/* TODO */

void rt_render_tile(struct tile *tile)
{
}

void rt_bgcolor(cgm_vec3 *color, cgm_ray *ray)
{
}

void rt_shade(cgm_vec3 *color, struct rayhit *hit, float energy, int max_iter)
{
}
