#ifndef REND_H_
#define REND_H_

#include <cgmath/cgmath.h>
#include "surf.h"

int init_rend(void);
void destroy_rend(void);

void set_camera_pos(float x, float y, float z);
void set_camera_targ(float x, float y, float z);
void set_camera_up(float x, float y, float z);
void set_camera_fov(float vfov_deg);

void primary_ray(cgm_ray *ray, int x, int y, int sample);
void trace_ray(cgm_vec3 *color, const cgm_ray *ray, int depth);
void backdrop(cgm_vec3 *color, const cgm_ray *ray);
void shade(cgm_vec3 *color, const cgm_ray *ray, const struct surf_hit *hit, int depth);

#endif	/* REND_H_ */
