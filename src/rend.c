#include <stdio.h>
#include "rt.h"
#include "rend.h"
#include "scene.h"
#include "tinymt.h"

struct camera {
	cgm_vec3 pos, targ, up;
	float half_fov;
	float xform[16];
};

static struct scene scn;
static int max_ray_depth = 5;
static struct material defmtl;
static struct camera cam;

int init_rend(void)
{
	union surface *surf;
	struct mesh *m;
	struct aabox bbox;

	init_scene(&scn);
	cgm_vcons(&scn.sky_horiz, 5, 4, 4);
	cgm_vcons(&scn.sky_zenith, 5, 4, 4);

	set_camera_pos(1.4, 0.1, 0);
	set_camera_targ(0, 0.5, 0);
	set_camera_up(0, 1, 0);
	set_camera_fov(50);

	/*
	set_camera_pos(0, 1, 5);
	set_camera_targ(0, 0, 0);
	set_camera_up(0, 1, 0);
	set_camera_fov(50);

	surf = create_sphere(0, 1, 0, 1);
	add_surface(&scn, surf);

	surf = create_aabox(0, -1, 0, 10, 2, 10);
	add_surface(&scn, surf);

	surf = create_aabox(2, 1, 0, 1, 2, 1);
	add_surface(&scn, surf);

	surf = create_mesh();
	m = &surf->mesh.m;
	begin_mesh(m);
	mesh_normal(m, 0, 0, 1);
	mesh_vertex(m, -2, 0, 0);
	mesh_vertex(m, -2.5, 1, 0);
	mesh_vertex(m, -1.5, 1, 0);
	end_mesh(m);
	add_surface(&scn, surf);
	*/

	surf = create_mesh();
	m = &surf->mesh.m;
	if(load_mesh(m, "sponza_tri.obj") == -1) {
		return -1;
	}
	calc_mesh_bounds(m, &bbox);
	printf("mesh bounds: (%f %f %f) - (%f %f %f)\n", bbox.vmin.x, bbox.vmin.y,
			bbox.vmin.z, bbox.vmax.x, bbox.vmax.y, bbox.vmax.z);
	add_surface(&scn, surf);

	build_mesh_octree(m, 32, 20);

	cgm_vcons(&defmtl.color, 0.7, 0.7, 0.7);
	defmtl.roughness = 1.0f;

	return 0;
}

void destroy_rend(void)
{
	clear_scene(&scn);
}

void set_camera_pos(float x, float y, float z)
{
	cgm_vcons(&cam.pos, x, y, z);
	cgm_mlookat(cam.xform, &cam.pos, &cam.targ, &cam.up);
}

void set_camera_targ(float x, float y, float z)
{
	cgm_vcons(&cam.targ, x, y, z);
	cgm_mlookat(cam.xform, &cam.pos, &cam.targ, &cam.up);
}

void set_camera_up(float x, float y, float z)
{
	cgm_vcons(&cam.up, x, y, z);
	cgm_mlookat(cam.xform, &cam.pos, &cam.targ, &cam.up);
}

void set_camera_fov(float vfov_deg)
{
	cam.half_fov = cgm_deg_to_rad(vfov_deg) * 0.5f;
}

void primary_ray(cgm_ray *ray, int x, int y, int sample)
{
	struct tinymt32 mt;
	float xoffs, yoffs;
	float aspect = (float)fbwidth / (float)fbheight;

	ray->dir.x = (2.0f * (float)x / (float)fbwidth - 1.0f) * aspect;
	ray->dir.y = 1.0f - 2.0f * (float)y / (float)fbheight;
	ray->dir.z = -1.0f / tan(cam.half_fov);

	ray->origin.x = ray->origin.y = ray->origin.z = 0.0f;

	tinymt32_init(&mt, (sample << 16) | sample);
	xoffs = (2.0f * tinymt32_generate_float(&mt) - 1.0f) / (float)fbheight;
	yoffs = (2.0f * tinymt32_generate_float(&mt) - 1.0f) / (float)fbheight;
	ray->dir.x += xoffs;
	ray->dir.y += yoffs;

	cgm_rmul_mr(ray, cam.xform);
	/*
	if(x == fbwidth / 2 && y == fbheight / 2) {
		asm volatile("int $3");
	}
	*/
}

void trace_ray(cgm_vec3 *color, const cgm_ray *ray, int depth)
{
	struct surf_hit hit;

	if(!ray_scene(&scn, ray, &hit)) {
		backdrop(color, ray);
	} else {
		shade(color, ray, &hit, depth);
	}
}

void backdrop(cgm_vec3 *color, const cgm_ray *ray)
{
	float len, dot = 0.0f;

	len = cgm_vlength(&ray->dir);
	if(len != 0.0f) {
		dot = ray->dir.y / len;
	}

	if(dot >= 0.0f) {
		cgm_vlerp(color, &scn.sky_horiz, &scn.sky_zenith, dot);
	} else {
		cgm_vlerp(color, &scn.sky_horiz, &scn.sky_nadir, -dot);
	}
}

void shade(cgm_vec3 *color, const cgm_ray *ray, const struct surf_hit *hit, int depth)
{
	cgm_ray sray;
	struct material *mtl;
	union surface *surf;

	if(depth >= max_ray_depth) {
		backdrop(color, ray);
		return;
	}

	surf = hit->surf;
	mtl = surf->any.mtl;
	if(!mtl) {
		mtl = &defmtl;
	}

	/* generate random direction with cosine distribution by generating a point
	 * on a unit sphere tangent to the surface, with center hit->pos + hit->normal
	 * and subtracting hit->pos. This boils down to sphrand + normal.
	 */
	cgm_sphrand(&sray.dir, 1.0f);
	cgm_vadd(&sray.dir, &hit->normal);
	cgm_vnormalize(&sray.dir);
	sray.origin = hit->pos;

	trace_ray(color, &sray, depth + 1);
	cgm_vmul(color, &mtl->color);
}
