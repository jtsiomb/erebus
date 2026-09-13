#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cgmath/cgmath.h"
#include "mesh.h"
#include "scene.h"
#include "util.h"
#include "meshfile.h"


static struct mesh *conv_mesh(struct mf_mesh *mfm, const char *path_prefix);
int conv_mtl(struct material *mtl, struct mf_material *mmtl, const char *path_prefix);
static struct image *load_texture(const char *fname, const char *path_prefix);
static void calc_face_normal(struct triangle *tri);


int load_scenefile(struct scenefile *scn, const char *fname)
{
	unsigned int i, count, tricount = 0;
	struct mf_meshfile *mf;
	struct mesh *mesh;
	char *path_prefix, *sptr, *endp;

	memset(scn, 0, sizeof *scn);

	if(!(mf = mf_alloc()) || mf_load(mf, fname, 0) == -1) {
		goto err;
	}

	endp = sptr = path_prefix = alloca(strlen(fname) + 1);
	while(*fname) {
		char c = *fname++;
		if(c == '/' || c == '\\') {
			*sptr++ = '/';
			endp = sptr;
		} else {
			*sptr++ = c;
		}
	}
	*endp = 0;

	count = mf_num_meshes(mf);
	for(i=0; i<count; i++) {
		if(!(mesh = conv_mesh(mf_get_mesh(mf, i), path_prefix))) {
			goto err;
		}
		mesh->next = scn->meshlist;
		scn->meshlist = mesh;
		scn->num_meshes++;

		tricount += mesh->num_faces;
	}

	printf("load_scenefile: loaded %d meshes, %d triangles\n", scn->num_meshes, tricount);

	mf_free(mf);
	return 0;

err:
	mesh = scn->meshlist;
	while(mesh) {
		struct mesh *tmp = mesh;
		mesh = mesh->next;
		destroy_mesh(tmp);
		free(tmp);
	}
	return -1;
}

void destroy_scenefile(struct scenefile *scn)
{
	struct mesh *m;
	struct light *lt;

	while(scn->meshlist) {
		m = scn->meshlist;
		scn->meshlist = scn->meshlist->next;
		destroy_mesh(m);
		free(m);
	}

	while(scn->lightlist) {
		lt = scn->lightlist;
		scn->lightlist = scn->lightlist->next;
		free(lt);
	}
}

void destroy_mesh(struct mesh *m)
{
	free(m->mtl.name);
	free(m->faces);
	m->faces = 0;
}

#define CONV_VEC2(mfmv) (*(cgm_vec2*)&(mfmv))
#define CONV_VEC3(mfmv)	(*(cgm_vec3*)&(mfmv))

static struct mesh *conv_mesh(struct mf_mesh *mfm, const char *path_prefix)
{
	unsigned int i, j, vidx;
	struct mesh *m;
	struct triangle *tri;

	if(!(m = calloc(1, sizeof *m))) {
		fprintf(stderr, "failed to allocate mesh structure\n");
		return 0;
	}

	if(!(m->faces = malloc(mfm->num_faces * sizeof *m->faces))) {
		fprintf(stderr, "failed to allocate triangle array\n");
		free(m);
		return 0;
	}
	m->num_faces = mfm->num_faces;

	if(mfm->name) {
		m->name = strdup(mfm->name);
	}

	tri = m->faces;
	for(i=0; i<mfm->num_faces; i++) {
		for(j=0; j<3; j++) {
			vidx = mfm->faces[i].vidx[j];
			tri->v[j].pos = CONV_VEC3(mfm->vertex[vidx]);
			tri->v[j].norm = CONV_VEC3(mfm->normal[vidx]);
			tri->v[j].tex = CONV_VEC2(mfm->texcoord[vidx]);
		}
		calc_face_normal(tri);
		tri->mtl = &m->mtl;
		tri++;
	}

	/* convert material */
	if(conv_mtl(&m->mtl, mfm->mtl, path_prefix) == -1) {
		goto err;
	}
	return m;

err:
	destroy_mesh(m);
	free(m);
	return 0;
}

int conv_mtl(struct material *mtl, struct mf_material *mmtl, const char *path_prefix)
{
	static const enum mf_mtlattr_type mfattr[] = {
		MF_COLOR,
		MF_SPECULAR,
		MF_EMISSIVE,
		MF_TRANSMIT,
		MF_ROUGHNESS,
		MF_METALLIC,
		MF_SHININESS,
		MF_REFLECT
	};

	int i;

	if(mmtl->name && !(mtl->name = strdup(mmtl->name))) {
		return -1;
	}
	for(i=0; i<NUM_MATTR; i++) {
		mtl->attr[i].value = CONV_VEC3(mmtl->attr[mfattr[i]].val);
		mtl->attr[i].tex = load_texture(mmtl->attr[mfattr[i]].map.name, path_prefix);
	}
	mtl->ior = mmtl->attr[MF_IOR].val.x;
	mtl->metal = mtl->attr[MATTR_METALLIC].value.x > 1e-4;
	return 0;
}

static struct image *load_texture(const char *fname, const char *path_prefix)
{
	char *path;

	if(!fname) return 0;

	if(path_prefix && *path_prefix) {
		path = alloca(strlen(fname) + strlen(path_prefix) + 1);
		sprintf(path, "%s/%s", path_prefix, fname);
	} else {
		path = (char*)fname;
	}
	return get_image(path);
}

static void calc_face_normal(struct triangle *tri)
{
	cgm_vec3 va, vb;

	va = tri->v[1].pos;
	cgm_vsub(&va, &tri->v[0].pos);
	vb = tri->v[2].pos;
	cgm_vsub(&vb, &tri->v[0].pos);

	cgm_vcross(&tri->norm, &va, &vb);
	cgm_vnormalize(&tri->norm);
}

#if 0
static char *cleanline(char *s)
{
	char *ptr;

	if((ptr = strchr(s, '#'))) *ptr = 0;

	while(*s && isspace(*s)) s++;
	ptr = s + strlen(s) - 1;
	while(ptr >= s && isspace(*ptr)) *ptr-- = 0;

	return *s ? s : 0;
}

static void conv_mtl(struct material *mm, struct objmtl *om, const char *path_prefix)
{
	char *fname = 0, *suffix = 0;
	int len, prefix_len, maxlen = 0;

	memset(mm, 0, sizeof *mm);
	mm->name = strdup(om->name);
	mm->attr[MATTR_COLOR].value = om->kd;
	mm->attr[MATTR_SPECULAR].value = om->ks;
	mm->attr[MATTR_SHININESS].value.x = om->shin;
	mm->attr[MATTR_EMIT].value = om->ke;
	if(om->valid & MTL_ROUGHNESS) {
		mm->attr[MATTR_ROUGHNESS].value.x = om->roughness;
	} else {
		mm->attr[MATTR_ROUGHNESS].value.x = 1.0f - (om->ks.x + om->ks.y + om->ks.z) / 3.0f;
	}
	mm->attr[MATTR_METALLIC].value.x = om->metallic;
	mm->attr[MATTR_REFLECT].value.x = om->refl;
	mm->attr[MATTR_TRANSMIT].value.x = 1.0f - om->alpha;
	mm->metal = om->metallic > 0.001;

	if(om->map_kd && (len = strlen(om->map_kd)) > maxlen) maxlen = len;
	if(om->map_ke && (len = strlen(om->map_ke)) > maxlen) maxlen = len;
	if(om->map_alpha && (len = strlen(om->map_alpha)) > maxlen) maxlen = len;

	if(maxlen) {
		prefix_len = strlen(path_prefix);
		fname = alloca(maxlen + prefix_len + 2);
		suffix = fname + prefix_len;
		strcpy(fname, path_prefix);
	}

	if(om->map_kd) {
		strcpy(suffix, om->map_kd);
		mm->attr[MATTR_COLOR].tex = get_image(fname);
	}
	if(om->map_ke) {
		strcpy(suffix, om->map_ke);
		mm->attr[MATTR_COLOR].tex = get_image(fname);
	}
	if(om->map_alpha) {
		strcpy(suffix, om->map_alpha);
		mm->mask = get_image(fname);
	}
}
#endif
