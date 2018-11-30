#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <cgmath/cgmath.h>
#include "mesh.h"
#include "dynarr.h"

struct facevertex {
	int vidx, tidx, nidx;
};

static char *clean_line(char *s);
static char *parse_face_vert(char *ptr, struct facevertex *fv, int numv, int numt, int numn);

int load_mesh(struct mesh *mesh, const char *fname)
{
	int i, j, line_num = 0, result = -1;
	int found_quad = 0;
	FILE *fp = 0;
	char buf[256];
	cgm_vec3 *varr = 0;
	cgm_vec3 *narr = 0;
	cgm_vec3 *tarr = 0;
	struct facevertex *fvarr = 0, *fvptr;
	int num_fv;
	struct face *fptr;

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "load_mesh: failed to open file: %s\n", fname);
		goto err;
	}

	if(!(varr = dynarr_alloc(0, sizeof *varr)) ||
			!(narr = dynarr_alloc(0, sizeof *narr)) ||
			!(tarr = dynarr_alloc(0, sizeof *tarr)) ||
			!(fvarr = dynarr_alloc(0, sizeof *fvarr))) {
		fprintf(stderr, "load_mesh: failed to allocate resizable vertex array\n");
		goto err;
	}

	while(fgets(buf, sizeof buf, fp)) {
		char *line = clean_line(buf);
		++line_num;

		if(!line || !*line) continue;

		switch(line[0]) {
		case 'v':
			if(isspace(line[1])) {
				/* vertex */
				cgm_vec3 v;
				int num;

				num = sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z);
				if(num != 3) {
					fprintf(stderr, "%s:%d: invalid vertex definition: \"%s\"\n", fname, line_num, line);
					goto err;
				}
				if(!(varr = dynarr_push(varr, &v))) {
					fprintf(stderr, "load_mesh: failed to resize vertex buffer\n");
					goto err;
				}

			} else if(line[1] == 't' && isspace(line[2])) {
				/* texcoord */
				cgm_vec3 tc;
				if(sscanf(line + 3, "%f %f", &tc.x, &tc.y) != 2) {
					fprintf(stderr, "%s:%d: invalid texcoord definition: \"%s\"\n", fname, line_num, line);
					goto err;
				}
				if(!(tarr = dynarr_push(tarr, &tc))) {
					fprintf(stderr, "load_mesh: failed to resize texcoord buffer\n");
					goto err;
				}

			} else if(line[1] == 'n' && isspace(line[2])) {
				/* normal */
				cgm_vec3 norm;
				if(sscanf(line + 3, "%f %f %f", &norm.x, &norm.y, &norm.z) != 3) {
					fprintf(stderr, "%s:%d: invalid normal definition: \"%s\"\n", fname, line_num, line);
					goto err;
				}
				if(!(narr = dynarr_push(narr, &norm))) {
					fprintf(stderr, "load_mesh: failed to resize normal buffer\n");
					goto err;
				}
			}
			break;

		case 'f':
			if(isspace(line[1])) {
				/* face */
				char *ptr = line + 2;
				struct facevertex fv;
				int vsz = dynarr_size(varr);
				int tsz = dynarr_size(tarr);
				int nsz = dynarr_size(narr);

				for(i=0; i<4; i++) {
					if(!(ptr = parse_face_vert(ptr, &fv, vsz, tsz, nsz))) {
						if(i < 3 || found_quad) {
							fprintf(stderr, "%s:%d: invalid face definition: \"%s\"\n", fname, line_num, line);
							goto err;
						} else {
							break;
						}
					}

					if(!(fvarr = dynarr_push(fvarr, &fv))) {
						fprintf(stderr, "load_mesh: failed to resize face vertex array\n");
						goto err;
					}
				}
				if(i > 3) found_quad = 1;
			}
			break;

		default:
			break;
		}
	}

	num_fv = dynarr_size(fvarr);
	mesh->num_faces = num_fv / 3;
	if(!(mesh->faces = malloc(mesh->num_faces * sizeof *mesh->faces))) {
		fprintf(stderr, "load_mesh: failed to create faces array\n");
		goto err;
	}

	fptr = mesh->faces;
	fvptr = fvarr;
	for(i=0; i<mesh->num_faces; i++) {
		for(j=0; j<3; j++) {
			fptr->v[j] = varr[fvptr->vidx];
			fptr->n[j] = narr[fvptr->nidx];
			fptr->tc[j] = tarr[fvptr->tidx];
			fvptr++;
		}

		calc_face_normal(fptr);
		fptr++;
	}

	result = 0;	/* success */

	printf("loaded %s mesh: %s: %d vertices, %d faces\n", found_quad ? "quad" : "triangle",
			fname, dynarr_size(varr), mesh->num_faces);

err:
	if(fp) fclose(fp);
	dynarr_free(varr);
	dynarr_free(narr);
	dynarr_free(tarr);
	dynarr_free(fvarr);
	return result;
}

static char *clean_line(char *s)
{
	char *end;

	while(*s && isspace(*s)) ++s;
	if(!*s) return 0;

	end = s;
	while(*end && *end != '#') ++end;
	*end = 0;

	while(end > s && isspace(*end)) --end;
	*end = 0;

	return s;
}

static char *parse_idx(char *ptr, int *idx, int arrsz)
{
	char *endp;
	int val = strtol(ptr, &endp, 10);
	if(endp == ptr) return 0;

	if(val < 0) {	/* convert negative indices */
		*idx = arrsz + val;
	} else {
		*idx = val - 1;	/* indices in obj are 1-based */
	}
	return endp;
}

/* possible face-vertex definitions:
 * 1. vertex
 * 2. vertex/texcoord
 * 3. vertex//normal
 * 4. vertex/texcoord/normal
 */
static char *parse_face_vert(char *ptr, struct facevertex *fv, int numv, int numt, int numn)
{
	if(!(ptr = parse_idx(ptr, &fv->vidx, numv)))
		return 0;
	if(*ptr != '/') return (!*ptr || isspace(*ptr)) ? ptr : 0;

	if(*++ptr == '/') {	/* no texcoord */
		fv->tidx = -1;
		++ptr;
	} else {
		if(!(ptr = parse_idx(ptr, &fv->tidx, numt)))
			return 0;
		if(*ptr != '/') return (!*ptr || isspace(*ptr)) ? ptr : 0;
		++ptr;
	}

	if(!(ptr = parse_idx(ptr, &fv->nidx, numn)))
		return 0;
	return (!*ptr || isspace(*ptr)) ? ptr : 0;
}
