#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "cgmath/cgmath.h"
#include "scene.h"
#include "erebus.h"
#include "treestor.h"
#include "mesh.h"

static int add_mesh_faces(struct bvhnode *bnode, struct mesh *mesh);
static void proc_edits(struct ts_node *snode, struct scenefile *sf);
static int edit_mtl(struct ts_node *node, const char *mtlname, const char *mtlprop, struct scenefile *sf);
int read_scene_node(struct node *node, struct ts_node *tsn);

void init_scene_node(struct node *n)
{
	memset(n, 0, sizeof *n);
	n->rot.w = 1;
	cgm_vcons(&n->scale, 1, 1, 1);
	cgm_midentity(n->matrix);
}

void calc_node_matrix(struct node *n)
{
	int i;
	float rmat[16];

	cgm_mtranslation(n->matrix, n->pivot.x, n->pivot.y, n->pivot.z);
	cgm_mrotation_quat(rmat, &n->rot);

	for(i=0; i<3; i++) {
		n->matrix[i] = rmat[i];
		n->matrix[4 + i] = rmat[4 + i];
		n->matrix[8 + i] = rmat[8 + i];
	}

	n->matrix[0] *= n->scale.x; n->matrix[4] *= n->scale.y; n->matrix[8] *= n->scale.z;
	n->matrix[1] *= n->scale.x; n->matrix[5] *= n->scale.y; n->matrix[9] *= n->scale.z;
	n->matrix[2] *= n->scale.x; n->matrix[6] *= n->scale.y; n->matrix[10] *= n->scale.z;
	n->matrix[12] += n->pos.x;
	n->matrix[13] += n->pos.y;
	n->matrix[14] += n->pos.z;

	cgm_mpretranslate(n->matrix, -n->pivot.x, -n->pivot.y, -n->pivot.z);
}

int load_scene(struct scene *scn, const char *fname)
{
	char *dirname, *ptr;
	char path[256];
	struct ts_node *root, *node;
	struct scenefile sf;
	struct mesh *mesh, *tail;
	unsigned long start_time;
	float *vec, num;

	memset(scn, 0, sizeof *scn);
	if(!(scn->st_root = calloc(1, sizeof *scn->st_root)) ||
			!(scn->dyn_root = calloc(1, sizeof *scn->dyn_root))) {
		free(scn->st_root);
		fprintf(stderr, "load_scene: failed to allocate bvh root nodes\n");
		return -1;
	}

	cgm_vcons(&scn->st_root->aabb.vmin, FLT_MAX, FLT_MAX, FLT_MAX);
	cgm_vcons(&scn->st_root->aabb.vmax, -FLT_MAX, -FLT_MAX, -FLT_MAX);
	scn->dyn_root->aabb = scn->st_root->aabb;

	dirname = alloca(strlen(fname) + 1);
	strcpy(dirname, fname);
	if((ptr = strrchr(dirname, '/'))) {
		ptr[1] = 0;
	} else {
		*dirname = 0;
	}

	if(!(root = ts_load(fname))) {
		fprintf(stderr, "load_scene: failed to load: %s\n", fname);
		return -1;
	}
	if(strcmp(root->name, "erebus") != 0) {
		fprintf(stderr, "load_scene: invalid scene file %s, root is not \"erebus\"\n", fname);
		ts_free_tree(root);
		return -1;
	}

	/* lookup environment properties */
	if((vec = ts_lookup_vec(root, "erebus.env.color", 0))) {
		scn->bgcolor.x = vec[0];
		scn->bgcolor.y = vec[1];
		scn->bgcolor.z = vec[2];
	}

	/* load scene files */
	node = root->child_list;
	while(node) {
		if(strcmp(node->name, "scene") == 0) {
			if(!(fname = ts_get_attr_str(node, "file", 0))) {
				fprintf(stderr, "load_scene: ignoring \"scene\" without a \"file\" attribute\n");
				goto cont;
			}
			snprintf(path, sizeof path, "%s%s", dirname, fname);
			printf("loading scene file: %s\n", path);

			if(load_scenefile(&sf, path) == -1) {
				goto cont;
			}

			/* perform any edits on the loaded scene */
			proc_edits(node, &sf);

			mesh = tail = sf.meshlist;
			while(mesh) {
				add_mesh_faces(scn->st_root, mesh);
				tail = mesh;
				mesh = mesh->next;
			}

			if(tail) {
				tail->next = scn->meshlist;
				scn->meshlist = sf.meshlist;
				sf.meshlist = 0;
			}
			destroy_scenefile(&sf);

		} else if(strcmp(node->name, "camera") == 0) {
			struct camera *cam;

			if(!(cam = malloc(sizeof *cam))) {
				fprintf(stderr, "failed to allocate camera structure\n");
				return -1;
			}

			if(read_scene_node(&cam->node, node) == -1) {
				fprintf(stderr, "failed to read camera node\n");
				return -1;
			}

			if((num = ts_get_attr_num(node, "fov", 0.0f)) > 0.0f && num < 180.0f) {
				cam->fov = cgm_deg_to_rad(num);
			} else {
				cam->fov = CGM_PI / 4.0f;
			}

			cam->next = scn->camlist;
			scn->camlist = cam;
		}
cont:	node = node->next;
	}
	ts_free_tree(root);

	printf("Building static BVH tree\n");
	start_time = get_msec();
	if(build_bvh_sah(scn->st_root) == -1) {
		return -1;
	}
	printf("BVH construction took: %lu msec\n", get_msec() - start_time);
	return 0;
}

void destroy_scene(struct scene *scn)
{
	struct mesh *mesh;

	free_bvh_tree(scn->st_root);
	free_bvh_tree(scn->dyn_root);

	while(scn->meshlist) {
		mesh = scn->meshlist;
		scn->meshlist = scn->meshlist->next;
		destroy_mesh(mesh);
	}
}


int ray_scene(cgm_ray *ray, struct scene *scn, float tmax, struct rayhit *hit)
{
	int found = 0;
	struct rayhit hit0;

	if(!hit) {
		if(ray_bvhnode(ray, scn->st_root, tmax, 0)) return 1;
		if(ray_bvhnode(ray, scn->dyn_root, tmax, 0)) return 1;
		return 0;
	}

	hit0.t = FLT_MAX;
	if(ray_bvhnode(ray, scn->st_root, tmax, hit)) {
		hit0 = *hit;
		found = 1;
	}
	if(ray_bvhnode(ray, scn->dyn_root, tmax, hit) && hit->t < hit0.t) {
		hit0 = *hit;
		found = 1;
	}

	if(found) {
		*hit = hit0;
		return 1;
	}
	return 0;
}

static int add_mesh_faces(struct bvhnode *bnode, struct mesh *mesh)
{
	int i, j, newsz;
	void *tmp;
	struct triangle *tri, **triptr;

	newsz = bnode->num_faces + mesh->num_faces;
	if(!(tmp = realloc(bnode->faces, newsz * sizeof *bnode->faces))) {
		fprintf(stderr, "append_polygons: failed to resize faces array to %d\n", newsz);
		return -1;
	}
	bnode->faces = tmp;
	triptr = bnode->faces + bnode->num_faces;
	bnode->num_faces = newsz;

	tri = mesh->faces;
	for(i=0; i<mesh->num_faces; i++) {
		*triptr++ = tri;
		for(j=0; j<3; j++) {
			cgm_vec3 *p = &tri->v[j].pos;
			if(p->x < bnode->aabb.vmin.x) bnode->aabb.vmin.x = p->x;
			if(p->x > bnode->aabb.vmax.x) bnode->aabb.vmax.x = p->x;
			if(p->y < bnode->aabb.vmin.y) bnode->aabb.vmin.y = p->y;
			if(p->y > bnode->aabb.vmax.y) bnode->aabb.vmax.y = p->y;
			if(p->z < bnode->aabb.vmin.z) bnode->aabb.vmin.z = p->z;
			if(p->z > bnode->aabb.vmax.z) bnode->aabb.vmax.z = p->z;
		}
		tri++;
	}
	return 0;
}

static void proc_edits(struct ts_node *snode, struct scenefile *sf)
{
	const char *mtlname, *mtlprop;
	struct ts_node *node;

	node = snode->child_list;
	while(node) {
		if(strcmp(node->name, "mtledit") == 0) {
			if(!(mtlname = ts_get_attr_str(node, "name", 0))) {
				fprintf(stderr, "proc_edits: name attribute missing\n");
				goto next;
			}
			if(!(mtlprop = ts_get_attr_str(node, "prop", 0))) {
				fprintf(stderr, "proc_edits: prop attribute missing\n");
				goto next;
			}
			edit_mtl(node, mtlname, mtlprop, sf);
		}
next:	node = node->next;
	}
}

enum { OP_SET, OP_SCALE };

struct {
	char *name;
	int op;
} mtlops[] = {
	{"set", OP_SET},
	{"scale", OP_SCALE},
	{0, 0}
};

static void mtlop_vec(int op, float *vptr, float *data, int nelem)
{
	switch(op) {
	case OP_SET:
		while(nelem--) *vptr++ = *data++;
		break;
	case OP_SCALE:
		while(nelem--) *vptr++ *= *data++;
		break;
	}
}

static int edit_mtl(struct ts_node *node, const char *mtlname, const char *mtlprop, struct scenefile *sf)
{
	int i, op;
	char *name;
	struct mesh *mesh;
	struct material *mtl = 0;
	struct ts_attr *opattr = 0;
	float opval[3] = {0};

	for(i=0; mtlops[i].name; i++) {
		if((opattr = ts_get_attr(node, mtlops[i].name))) {
			op = mtlops[i].op;
			break;
		}
	}
	if(!opattr) {
		fprintf(stderr, "mtl edit: missing or invalid operation\n");
		return -1;
	}

	switch(opattr->val.type) {
	case TS_NUMBER:
		opval[0] = opval[1] = opval[2] = opattr->val.fnum;
		break;
	case TS_VECTOR:
		opval[0] = opattr->val.vec[0];
		opval[1] = opattr->val.vec[1];
		opval[2] = opattr->val.vec[2];
	default:
		break;
	}

	/* find the first instance of the named material */
	mesh = sf->meshlist;
	while(mesh) {
		if(strcmp(mesh->mtl.name, mtlname) == 0) {
			mtl = &mesh->mtl;
			break;
		}
		mesh = mesh->next;
	}
	if(!mtl) {
		fprintf(stderr, "mtl edit: no matching material: %s\n", mtlname);
		return -1;
	}

	if(strcmp(mtlprop, "color") == 0) {
		mtlop_vec(op, &mtl->attr[MATTR_COLOR].value.x, opval, 3);
	} else if(strcmp(mtlprop, "emit") == 0) {
		mtlop_vec(op, &mtl->attr[MATTR_EMIT].value.x, opval, 3);
	} else if(strcmp(mtlprop, "roughness") == 0) {
		mtlop_vec(op, &mtl->attr[MATTR_ROUGHNESS].value.x, opval, 1);
	} else if(strcmp(mtlprop, "ior") == 0) {
		mtlop_vec(op, &mtl->ior, opval, 1);
	} else if(strcmp(mtlprop, "transmit") == 0) {
		mtlop_vec(op, &mtl->attr[MATTR_TRANSMIT].value.x, opval, 1);
	} else if(strcmp(mtlprop, "metal") == 0) {
		if(op != OP_SET || opattr->val.type != TS_NUMBER) {
			fprintf(stderr, "mtl edit: invalid operation or attribute type for metal\n");
			return -1;
		}
		mtl->metal = opattr->val.inum != 0;
	} else if(strcmp(mtlprop, "texture") == 0) {
		if(op != OP_SET) {
			fprintf(stderr, "mtl edit: invalid operation for textures\n");
			return -1;
		}
		/* TODO: lookup in the texture db/load, and set pointer */
	}

	/* copy the modified material to every other instance */
	mesh = mesh->next;
	while(mesh) {
		if(strcmp(mesh->mtl.name, mtlname) == 0) {
			name = mesh->mtl.name;
			mesh->mtl = *mtl;
			mesh->mtl.name = name;	/* don't clobber the strdup'ed name */
		}
		mesh = mesh->next;
	}
	return 0;
}

int read_scene_node(struct node *node, struct ts_node *tsn)
{
	int found = 0;
	float *vec;
	float matrix[16];
	cgm_vec3 dir, up;

	init_scene_node(node);

	if((vec = ts_get_attr_vec(tsn, "position", 0))) {
		cgm_vcons(&node->pos, vec[0], vec[1], vec[2]);
		found++;
	}
	if((vec = ts_get_attr_vec(tsn, "rotation", 0))) {
		cgm_qcons(&node->rot, vec[0], vec[1], vec[2], vec[3]);
		found++;
	}
	if((vec = ts_get_attr_vec(tsn, "scale", 0))) {
		cgm_vcons(&node->scale, vec[0], vec[1], vec[2]);
		found++;
	}
	if((vec = ts_get_attr_vec(tsn, "pivot", 0))) {
		cgm_vcons(&node->pivot, vec[0], vec[1], vec[2]);
		found++;
	}

	if((vec = ts_get_attr_vec(tsn, "lookat", 0))) {
		/* lookat target overrides rotation */
		cgm_vcons(&dir, vec[0] - node->pos.x, vec[1] - node->pos.y, vec[2] - node->pos.z);
		cgm_vnormalize(&dir);
		cgm_vcons(&up, 0, 1, 0);
		if(fabs(cgm_vdot(&up, &dir)) < 1e-4) {
			cgm_vcons(&up, 0, 0, 1);
		}
		cgm_mlookat(matrix, &node->pos, (cgm_vec3*)vec, &up);
		cgm_mget_rotation(matrix, &node->rot);
		found++;
	}

	if(found) {
		calc_node_matrix(node);
	}
	return 0;
}
