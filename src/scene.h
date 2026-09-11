#ifndef SCENE_H_
#define SCENE_H_

#include "rt.h"
#include "bvh.h"

struct node {
	char *name;
	cgm_vec3 pos, scale, pivot;
	cgm_quat rot;

	float matrix[16];

	struct node *next;
	struct node *child;
};

struct camera {
	struct node node;
	float fov;

	struct camera *next;
};

struct light {
	cgm_vec3 pos;
	float rad;
	cgm_vec3 color;

	struct light *next;
};

struct scene {
	cgm_vec3 ambient;
	cgm_vec3 bgcolor;

	struct bvhnode *st_root;
	struct bvhnode *dyn_root;

	struct mesh *meshlist;
	struct camera *camlist;
	struct light *lightlist;
};

void init_scene_node(struct node *n);
void calc_node_matrix(struct node *n);

int load_scene(struct scene *scn, const char *fname);
void destroy_scene(struct scene *scn);

struct node *find_scn_node(struct scene *scn, const char *name);

int ray_scene(cgm_ray *ray, struct scene *scn, float tmax, struct rayhit *hit);

void draw_scene(struct scene *scn);

#endif	/* SCENE_H_ */
