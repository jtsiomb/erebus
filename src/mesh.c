#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include "mesh.h"
#include "surf.h"
#include "dynarr.h"

static int ray_mesh_noacc(const struct mesh *m, const cgm_ray *ray, struct surf_hit *hit);
static int ray_mesh_octree(const struct mesh *m, struct octnode *on, const cgm_ray *ray, struct surf_hit *hit);
static int octree_height(struct octnode *n);
static int octree_max_faces(struct octnode *n);

void init_mesh(struct mesh *m)
{
	m->faces = 0;
	m->num_faces = 0;
}

void clear_mesh(struct mesh *m)
{
	free(m->faces);
	m->faces = 0;
	m->num_faces = 0;
}

void calc_face_normal(struct face *f)
{
	cgm_vec3 a, b;
	a = b = f->v[0];
	cgm_vsub(&a, f->v + 1);
	cgm_vsub(&b, f->v + 2);

	cgm_vcross(&f->normal, &a, &b);
	cgm_vnormalize(&f->normal);
}

void calc_mesh_bounds(const struct mesh *m, struct aabox *aabb)
{
	int i, j;

	cgm_vcons(&aabb->vmin, FLT_MAX, FLT_MAX, FLT_MAX);
	cgm_vcons(&aabb->vmax, -FLT_MAX, -FLT_MAX, -FLT_MAX);

	for(i=0; i<m->num_faces; i++) {
		for(j=0; j<3; j++) {
			cgm_vec3 *v = m->faces[i].v + j;
			if(v->x < aabb->vmin.x) aabb->vmin.x = v->x;
			if(v->x > aabb->vmax.x) aabb->vmax.x = v->x;
			if(v->y < aabb->vmin.y) aabb->vmin.y = v->y;
			if(v->y > aabb->vmax.y) aabb->vmax.y = v->y;
			if(v->z < aabb->vmin.z) aabb->vmin.z = v->z;
			if(v->z > aabb->vmax.z) aabb->vmax.z = v->z;
		}
	}
}

static float ray_face(const struct face *face, const cgm_ray *ray, cgm_vec3 *bary)
{
	float t, ndotdir, ndotvdir;
	cgm_vec3 vdir, p;

	if(fabs(ndotdir = cgm_vdot(&face->normal, &ray->dir)) < 1e-6) {
		return -1.0f;
	}

	vdir = face->v[0];
	cgm_vsub(&vdir, &ray->origin);
	ndotvdir = cgm_vdot(&face->normal, &vdir);

	if((t = ndotvdir / ndotdir) < 1e-5) {
		return -1.0f;
	}

	cgm_raypos(&p, ray, t);
	cgm_bary(bary, face->v, face->v + 1, face->v + 2, &p);

	if(bary->x < 0.0f || bary->x > 1.0f) return -1.0f;
	if(bary->y < 0.0f || bary->y > 1.0f) return -1.0f;
	if(bary->z < 0.0f || bary->z > 1.0f) return -1.0f;

	return t;
}

static inline void bary_interp(cgm_vec3 *p, const cgm_vec3 *a,
		const cgm_vec3 *b, const cgm_vec3 *c, const cgm_vec3 *bc)
{
	p->x = a->x * bc->x + b->x * bc->y + c->x * bc->z;
	p->y = a->y * bc->x + b->y * bc->y + c->y * bc->z;
	p->z = a->z * bc->x + b->z * bc->y + c->z * bc->z;
}

int find_mesh_isect(const struct mesh *m, const float *inv_xform,
		const cgm_ray *ray, struct surf_hit *hit)
{
	cgm_vec3 bc;
	struct surf_hit tmphit;
	struct face *face;
	cgm_ray lray = *ray;

	cgm_rmul_mr(&lray, inv_xform);

	if(m->octree) {
		/*
		if(!ray_aabox(&m->octree->bbox, &lray, &tmphit)) {
			return 0;
		}
		*/
		if(!ray_mesh_octree(m, m->octree, &lray, &tmphit)) {
			return 0;
		}
	} else {
		if(!ray_mesh_noacc(m, &lray, &tmphit)) {
			return 0;
		}
	}

	if(hit) {
		hit->t = tmphit.t;

		face = (struct face*)tmphit.surf;
		bc = tmphit.pos;

		cgm_raypos(&hit->pos, ray, hit->t);

		bary_interp(&hit->normal, face->n, face->n + 1, face->n + 2, &bc);
		bary_interp(&hit->tex, face->tc, face->tc + 1, face->tc + 2, &bc);
	}
	return 1;
}

static int ray_mesh_noacc(const struct mesh *m, const cgm_ray *ray, struct surf_hit *hit)
{
	int i;
	float t, nearest_t = FLT_MAX;
	struct face *face, *nearest_face = 0;
	cgm_vec3 bc, nearest_bc;

	for(i=0; i<m->num_faces; i++) {
		face = m->faces + i;
		if((t = ray_face(face, ray, &bc)) >= 0.0f && t < nearest_t) {
			nearest_t = t;
			nearest_face = face;
			nearest_bc = bc;
		}
	}

	if(!nearest_face) return 0;
	if(hit) {
		hit->t = nearest_t;
		hit->pos = nearest_bc;
		hit->surf = nearest_face;
	}
	return 1;
}

static int ray_mesh_octree(const struct mesh *m, struct octnode *on, const cgm_ray *ray, struct surf_hit *hit)
{
	int i;
	struct surf_hit nearest_hit, chit;

	if(!ray_aabox(&on->bbox, ray, 0)) {
		return 0;
	}

	if(on->items) {
		/* leaf node: check all faces for intersections, return the nearest */
		float t, nearest_t = FLT_MAX;
		struct face *face, *nearest_face = 0;
		cgm_vec3 bc, nearest_bc;
		struct octitem *it = on->items;

		while(it) {
			face = m->faces + it->idx;
			if((t = ray_face(face, ray, &bc)) >= 0.0f && t < nearest_t) {
				nearest_t = t;
				nearest_face = face;
				nearest_bc = bc;
			}
			it = it->next;
		}

		if(!nearest_face) return 0;
		if(hit) {
			hit->t = nearest_t;
			hit->pos = nearest_bc;
			hit->surf = nearest_face;
		}
		return 1;
	}

	/* internal node: recurse and check children */
	nearest_hit.t = FLT_MAX;
	nearest_hit.surf = 0;
	for(i=0; i<8; i++) {
		if(on->child[i]) {
			if(ray_mesh_octree(m, on->child[i], ray, &chit) && chit.t < nearest_hit.t) {
				nearest_hit = chit;
			}
		}
	}

	if(!nearest_hit.surf) return 0;

	if(hit) {
		*hit = nearest_hit;
	}
	return 1;
}

/* ---- mesh immediate mode construction ---- */
static cgm_vec3 cur_n, cur_tc;
static struct face cur_face;
static int cur_vidx;

int begin_mesh(struct mesh *m)
{
	if(!(m->faces = dynarr_alloc(0, sizeof *m->faces))) {
		return -1;
	}
	cur_vidx = 0;
	return 0;
}

void end_mesh(struct mesh *m)
{
	if(cur_vidx != 0) {
		fprintf(stderr, "end_mesh: ignoring %d leftover vertices\n", cur_vidx);
	}
	m->num_faces = dynarr_size(m->faces);
	m->faces = dynarr_finalize(m->faces);
}

void mesh_vertex(struct mesh *m, float x, float y, float z)
{
	cgm_vcons(cur_face.v + cur_vidx, x, y, z);
	cur_face.n[cur_vidx] = cur_n;
	cur_face.tc[cur_vidx] = cur_tc;

	if(++cur_vidx >= 3) {
		cur_vidx = 0;
		calc_face_normal(&cur_face);
		DYNARR_PUSH(m->faces, &cur_face);
	}
}

void mesh_normal(struct mesh *m, float x, float y, float z)
{
	cgm_vcons(&cur_n, x, y, z);
}

void mesh_texcoord(struct mesh *m, float u, float v)
{
	cgm_vcons(&cur_tc, u, v, 0);
}

static void child_bounds(struct aabox *res, struct aabox *par, int idx)
{
	static const cgm_vec3 tmin[8] = {
		{0, 0.5, 0}, {0.5, 0.5, 0}, {0.5, 0.5, 0.5}, {0, 0.5, 0.5},
		{0, 0, 0}, {0.5, 0, 0}, {0.5, 0, 0.5}, {0, 0, 0.5}
	};

	res->vmin.x = cgm_lerp(par->vmin.x, par->vmax.x, tmin[idx].x);
	res->vmax.x = cgm_lerp(par->vmin.x, par->vmax.x, tmin[idx].x + 0.5f);
	res->vmin.y = cgm_lerp(par->vmin.y, par->vmax.y, tmin[idx].y);
	res->vmax.y = cgm_lerp(par->vmin.y, par->vmax.y, tmin[idx].y + 0.5f);
	res->vmin.z = cgm_lerp(par->vmin.z, par->vmax.z, tmin[idx].z);
	res->vmax.z = cgm_lerp(par->vmin.z, par->vmax.z, tmin[idx].z + 0.5f);
}

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#define ELEM(v, i)	(((float*)(&(v).x))[i])

static int face_in_box(struct face *f, struct aabox *b)
{
	int i;
	float fmin, fmax, bmin, bmax;

	for(i=0; i<3; i++) {
		fmin = MIN(ELEM(f->v[0], i), MIN(ELEM(f->v[1], i), ELEM(f->v[2], i)));
		fmax = MAX(ELEM(f->v[0], i), MAX(ELEM(f->v[1], i), ELEM(f->v[2], i)));
		bmin = ELEM(b->vmin, i);
		bmax = ELEM(b->vmax, i);

		if(fmin > bmax || fmax < bmin) {
			return 0;
		}
	}

	/* TODO signed point-plane distance for each AABB corner */
	return 1;
}

static void free_octree(struct octnode *node)
{
	int i;
	for(i=0; i<8; i++) {
		free_octree(node->child[i]);
	}

	while(node->items) {
		struct octitem *it = node->items;
		node->items = node->items->next;
		free(it);
	}
	free(node);
}

static int build_octree(struct mesh *mesh, struct octnode *node, int max_node_items, int max_depth)
{
	int i;
	struct octnode *cn;
	struct octitem *item, *newit;

	if(node->num_items < max_node_items || max_depth <= 0) {
		return 0;
	}

	memset(node->child, 0, sizeof node->child);

	for(i=0; i<8; i++) {
		if(!(cn = calloc(1, sizeof *cn))) {
			perror("build_octree: failed to allocate node");
			goto fail;
		}
		node->child[i] = cn;
		child_bounds(&cn->bbox, &node->bbox, i);

		item = node->items;
		while(item) {
			if(face_in_box(mesh->faces + item->idx, &cn->bbox)) {
				if(!(newit = malloc(sizeof *newit))) {
					perror("build_octree: failed to allocate item");
					goto fail;
				}
				newit->idx = item->idx;
				newit->next = cn->items;
				cn->items = newit;
				cn->num_items++;
			}
			item = item->next;
		}
	}

	/* remove items from parent node */
	while(node->items) {
		item = node->items;
		node->items = node->items->next;
		free(item);
	}
	node->num_items = 0;

	/* recurse into all children */
	for(i=0; i<8; i++) {
		if(build_octree(mesh, node->child[i], max_node_items, max_depth - 1) == -1) {
			goto fail;
		}
	}
	return 0;

fail:
	for(i=0; i<8; i++) {
		free_octree(node->child[i]);
		node->child[i] = 0;
	}
	return -1;
}

int build_mesh_octree(struct mesh *m, int max_node_items, int max_depth)
{
	int i;

	if(m->num_faces <= 0) return -1;

	printf("building octree for mesh with %d faces\n", m->num_faces);

	if(!(m->octree = calloc(1, sizeof *m->octree))) {
		perror("build_octree: failed to allocate root node");
		return -1;
	}
	calc_mesh_bounds(m, &m->octree->bbox);

	for(i=0; i<m->num_faces; i++) {
		struct octitem *item;

		if(!(item = malloc(sizeof *item))) {
			perror("failed to construct face index m->octree->items");
			while(m->octree->items) {
				item = m->octree->items;
				m->octree->items = m->octree->items->next;
				free(item);
			}
			return -1;
		}
		item->idx = i;
		item->next = m->octree->items;
		m->octree->items = item;
	}
	m->octree->num_items = m->num_faces;

	build_octree(m, m->octree, max_node_items, max_depth);

	printf("  height: %d\n", octree_height(m->octree));
	printf("  max faces/node: %d\n", octree_max_faces(m->octree));

	return 0;
}

static int octree_height(struct octnode *n)
{
	int i, h, maxh = 0;
	if(!n) return 0;

	for(i=0; i<8; i++) {
		if((h = octree_height(n->child[i])) > maxh) {
			maxh = h;
		}
	}
	return maxh + 1;
}

static int octree_max_faces(struct octnode *n)
{
	int i, maxf, num;

	if(!n) return 0;

	maxf = n->num_items;
	for(i=0; i<8; i++) {
		if((num = octree_max_faces(n->child[i])) > maxf) {
			maxf = num;
		}
	}
	return maxf;
}


int dump_mesh(struct mesh *m, const char *fname)
{
	int i, j;
	FILE *fp;


	if(!(fp = fopen(fname, "wb"))) {
		fprintf(stderr, "failed to open file: %s: %s\n", fname, strerror(errno));
		return -1;
	}
	fprintf(fp, "# OBJ mesh dumped from erebus\n");

	for(i=0; i<m->num_faces; i++) {
		for(j=0; j<3; j++) {
			fprintf(fp, "v %f %f %f\n", m->faces[i].v[j].x, m->faces[i].v[j].y, m->faces[i].v[j].z);
		}
	}
	for(i=0; i<m->num_faces; i++) {
		for(j=0; j<3; j++) {
			fprintf(fp, "vn %f %f %f\n", m->faces[i].n[j].x, m->faces[i].n[j].y, m->faces[i].n[j].z);
		}
	}
	for(i=0; i<m->num_faces; i++) {
		for(j=0; j<3; j++) {
			fprintf(fp, "vt %f %f\n", m->faces[i].tc[j].x, m->faces[i].tc[j].y);
		}
	}

	for(i=0; i<m->num_faces; i++) {
		int idx[3];

		idx[0] = i * 3 + 1;
		idx[1] = i * 3 + 2;
		idx[2] = i * 3 + 3;

		fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", idx[0], idx[0], idx[0],
				idx[1], idx[1], idx[1], idx[2], idx[2], idx[2]);
	}
	fclose(fp);
	return 0;
}
