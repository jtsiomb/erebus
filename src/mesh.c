#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cgmath/cgmath.h"
#include "mesh.h"
#include "scene.h"
#include "util.h"
#include "meshfile.h"
#include "imago2.h"


static struct mesh *conv_mesh(struct mf_mesh *mfm, const char *path_prefix);
int conv_mtl(struct material *mtl, struct mf_material *mmtl, const char *path_prefix);
static struct image *load_texture(const char *fname, const char *path_prefix);
static struct image *load_texture_mem(struct mf_image_data *imgdata);
static void calc_face_normal(struct triangle *tri);


int load_scenefile(struct scenefile *scn, const char *fname)
{
	unsigned int i, count, tricount = 0;
	struct mf_meshfile *mf;
	struct mesh *mesh;
	char *path_prefix, *sptr, *endp;

	memset(scn, 0, sizeof *scn);

	if(!(mf = mf_alloc()) || mf_load(mf, fname, MF_APPLY_XFORM) == -1) {
		goto err;
	}

	endp = sptr = path_prefix = alloca(strlen(fname) + 1);
	while(*fname) {
		char c = *fname++;
		if(c == '/' || c == '\\') {
			endp = sptr;
			*sptr++ = '/';
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
	mf_free(mf);
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

	if(mfm->num_faces <= 0) {
		fprintf(stderr, "conv_mesh: mesh has no faces\n");
		return 0;
	}

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
			if(mfm->normal) {
				tri->v[j].norm = CONV_VEC3(mfm->normal[vidx]);
			}
			if(mfm->texcoord) {
				tri->v[j].tex = CONV_VEC2(mfm->texcoord[vidx]);
				tri->v[j].tex.y = 1.0f - tri->v[j].tex.y;
			}
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
	/* XXX this must correspond to the MATTR_ enum in rt.h */
	static const enum mf_mtlattr_type mfattr[] = {
		MF_COLOR,
		MF_SPECULAR,
		MF_EMISSIVE,
		MF_REFLECT,
		MF_TRANSMIT,
		MF_ROUGHNESS,
		MF_METALLIC,
		MF_SHININESS
	};

	int i, j, npixels;
	struct image *img;
	float *src, *dest;
	struct mf_texmap *mftex;

	if(mmtl->name && !(mtl->name = strdup(mmtl->name))) {
		return -1;
	}
	for(i=0; i<NUM_MATTR; i++) {
		mtl->attr[i].value = CONV_VEC3(mmtl->attr[mfattr[i]].val);

		mftex = &mmtl->attr[mfattr[i]].map;
		if(mftex->imgdata) {
			img = load_texture_mem(mftex->imgdata);
		} else if(mftex->name) {
			img = load_texture(mftex->name, path_prefix);
		} else {
			continue;
		}

		if(img) {
			mtl->attr[i].tex = img;

			if(i == MATTR_COLOR && img->alpha) {
				if(!(mtl->mask = calloc(1, sizeof *mtl->mask))) {
					continue;
				}
				*mtl->mask = *img;
				mtl->mask->alpha = 0;
				npixels = img->width * img->height;

				if(!(mtl->mask->pixels = malloc(npixels * 3 * sizeof(float)))) {
					free(mtl->mask);
					mtl->mask = 0;
					continue;
				}

				printf("extracting alpha mask from: %s\n", img->name);
				src = img->alpha;
				dest = mtl->mask->pixels;
				for(j=0; j<npixels; j++) {
					dest[0] = dest[1] = dest[2] = *src++;
					dest += 3;
				}
			}
		}
	}
	mtl->ior = mmtl->attr[MF_IOR].val.x;
	mtl->metal = mtl->attr[MATTR_METALLIC].value.x > 1e-4;

	mftex = &mmtl->attr[MF_ALPHA].map;
	if(mftex->imgdata) {
		img = load_texture_mem(mftex->imgdata);
	} else {
		img = load_texture(mftex->name, path_prefix);
	}
	if(img) {
		printf("using alpha texture %s as mask\n", img->name);
		mtl->mask = img;
	}

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
	return get_image(path, 0, 0);
}

static struct image *load_texture_mem(struct mf_image_data *imgdata)
{
	if(!imgdata || !imgdata->data || imgdata->size <= 0) {
		return 0;
	}

	return get_image(imgdata->name, imgdata->data, imgdata->size);
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
