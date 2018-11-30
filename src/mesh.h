#ifndef MESH_H_
#define MESH_H_

#include <cgmath/cgmath.h>
#include "aabox.h"

struct surf_hit;

struct face {
	cgm_vec3 v[3], n[3], tc[3];
	cgm_vec3 normal;
};

struct octitem {
	int idx;
	struct octitem *next;
};

struct octnode {
	struct aabox bbox;
	struct octitem *items;
	int num_items;
	struct octnode *child[8];
};

struct mesh {
	struct face *faces;
	int num_faces;

	struct octnode *octree;

	cgm_vec3 im_norm, im_tc;	/* immedate mode construction state */
};

void init_mesh(struct mesh *m);
void clear_mesh(struct mesh *m);

int load_mesh(struct mesh *m, const char *fname);
int dump_mesh(struct mesh *m, const char *fname);

void calc_face_normal(struct face *f);
void calc_mesh_bounds(const struct mesh *m, struct aabox *aabb);

int find_mesh_isect(const struct mesh *m, const float *inv_xform,
		const cgm_ray *ray, struct surf_hit *hit);

int begin_mesh(struct mesh *m);
void end_mesh(struct mesh *m);
void mesh_vertex(struct mesh *m, float x, float y, float z);
void mesh_normal(struct mesh *m, float x, float y, float z);
void mesh_texcoord(struct mesh *m, float u, float v);

/* max_depth takes precedence over max_node_items */
int build_mesh_octree(struct mesh *m, int max_node_items, int max_depth);

#endif	/* MESH_H_ */
