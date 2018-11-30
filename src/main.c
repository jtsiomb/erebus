#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/freeglut.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include "rt.h"

static void disp(void);
static void reshape(int x, int y);
static void keyb(unsigned char key, int x, int y);
static int save_image(const char *fname);

static const char *sdrsrc =
	"uniform sampler2D tex;\n"
	"uniform float scale, inv_gamma;\n"
	"void main()\n"
	"{\n"
	"\tvec3 texel = texture2D(tex, gl_TexCoord[0].st).rgb * vec3(scale, scale, scale);\n"
	"\tgl_FragColor.rgb = pow(texel, vec3(inv_gamma, inv_gamma, inv_gamma));\n"
	"\tgl_FragColor.a = 1.0;\n"
	"}\n";

static unsigned int sdr;
static int uloc_scale, uloc_inv_gamma;

static int pfd[2];

int main(int argc, char **argv)
{
	int i, xsz = 800, ysz = 600, nsamples = 5;
	int ps, status, info_len, xfd, maxfd;
	char *info;
	Display *dpy;

	glutInit(&argc, argv);

	for(i=1; i<argc; i++) {
		if(strcmp(argv[i], "-s") == 0) {
			if(sscanf(argv[++i], "%dx%d", &xsz, &ysz) != 2 ||
					xsz <= 0 || ysz <= 0) {
				fprintf(stderr, "-s must be followed by WxH\n");
				return -1;
			}

		} else if(strcmp(argv[i], "-r") == 0) {
			if(!(nsamples = atoi(argv[++i]))) {
				fprintf(stderr, "-r must be followed by the number of rays per pixel\n");
				return -1;
			}

		} else {
			fprintf(stderr, "invalid argument: %s\n", argv[i]);
			return -1;
		}
	}


	glutInitWindowSize(xsz, ysz);
	glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
	glutCreateWindow("rt");

	glutDisplayFunc(disp);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyb);

	pipe(pfd);
	fcntl(pfd[0], F_SETFL, fcntl(pfd[0], F_GETFL) | O_NONBLOCK);

	if(rt_init(xsz, ysz) == -1) {
		return 1;
	}
	atexit(rt_cleanup);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, fbwidth, fbheight, 0, GL_RGB, GL_FLOAT, fbpixels);
	glEnable(GL_TEXTURE_2D);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, fbwidth);

	if(!(ps = glCreateShader(GL_FRAGMENT_SHADER))) {
		fprintf(stderr, "failed to create shader\n");
		return 1;
	}
	glShaderSource(ps, 1, &sdrsrc, 0);
	glCompileShader(ps);
	glGetShaderiv(ps, GL_COMPILE_STATUS, &status);
	printf("Shader compilation %s\n", status ? "success" : "failed");
	glGetShaderiv(ps, GL_INFO_LOG_LENGTH, &info_len);
	if(info_len > 0 && (info = malloc(info_len + 1))) {
		glGetShaderInfoLog(ps, info_len + 1, 0, info);
		printf("Compile log:\n%s\n", info);
		free(info);
	}
	if(!status) return 1;

	if(!(sdr = glCreateProgram())) {
		fprintf(stderr, "failed to create shader program\n");
		return 1;
	}
	glAttachShader(sdr, ps);
	glLinkProgram(sdr);
	glGetProgramiv(sdr, GL_LINK_STATUS, &status);
	printf("Shader program linking %s\n", status ? "success" : "failed");
	glGetProgramiv(sdr, GL_INFO_LOG_LENGTH, &info_len);
	if(info_len > 0 && (info = malloc(info_len + 1))) {
		glGetProgramInfoLog(sdr, info_len + 1, 0, info);
		printf("Linking log:\n%s\n", info);
		free(info);
	}
	if(!status) return 1;

	glUseProgram(sdr);
	uloc_scale = glGetUniformLocation(sdr, "scale");
	uloc_inv_gamma = glGetUniformLocation(sdr, "inv_gamma");

	glUniform1f(uloc_scale, 1.0f);
	glUniform1f(uloc_inv_gamma, 1.0f / 2.2f);

	glClear(GL_COLOR_BUFFER_BIT);
	rt_render(nsamples);

	glFlush();
	assert(glGetError() == GL_NO_ERROR);

	dpy = glXGetCurrentDisplay();
	xfd = ConnectionNumber(dpy);
	maxfd = xfd > pfd[0] ? xfd : pfd[0];

	for(;;) {
		fd_set rdset;
		int res, redraw_pending = 0;

		FD_ZERO(&rdset);
		FD_SET(xfd, &rdset);
		FD_SET(pfd[0], &rdset);

		while((res = select(maxfd + 1, &rdset, 0, 0, 0)) == -1 && errno == EINTR);

		if(res > 0) {
			if(FD_ISSET(pfd[0], &rdset)) {
				char tmp[64];
				while(read(pfd[0], tmp, sizeof tmp) > 0);
				glutPostRedisplay();
				redraw_pending = 1;
			}
			if(FD_ISSET(xfd, &rdset) || redraw_pending) {
				redraw_pending = 0;
				glutMainLoopEvent();
			}
		}
	}

	return 0;
}

void redraw(void)
{
	write(pfd[1], pfd, 1);
}

static void update_viewport(int x, int y, int w, int h)
{
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGB, GL_FLOAT, fbpixels + (y * fbwidth + x) * 3);
}

#define QUAD_VERTEX(x, y)	\
	glTexCoord2f((x) / (float)fbwidth, (y) / (float)fbheight); \
	glVertex2f(x, y)

static void disp(void)
{
	struct rt_block *dirty = rt_begin_update();
	while(dirty) {
		update_viewport(dirty->x, dirty->y, dirty->w, dirty->h);

		glUniform1f(uloc_scale, 1.0f / dirty->sample);

		glBegin(GL_QUADS);
		QUAD_VERTEX(dirty->x, dirty->y);
		QUAD_VERTEX(dirty->x + dirty->w, dirty->y);
		QUAD_VERTEX(dirty->x + dirty->w, dirty->y + dirty->h);
		QUAD_VERTEX(dirty->x, dirty->y + dirty->h);
		glEnd();

		dirty = dirty->next;
	}
	rt_end_update();

	glFlush();
	assert(glGetError() == GL_NO_ERROR);
}

static void reshape(int x, int y)
{
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, fbwidth, fbheight, 0, -1, 1);
}

static void keyb(unsigned char key, int x, int y)
{
	switch(key) {
	case 27:
		exit(0);

	case '\n':
	case '\r':
		rt_clear();
		rt_render(5);
		break;

	case ' ':
		rt_render(5);
		break;

	case 's':
		save_image("output.ppm");
		break;
	}
}

#define INV_GAMMA	(1.0 / 2.2)
static int save_image(const char *fname)
{
	FILE *fp;
	int i;
	float *fbptr = fbpixels;
	float pixscale = 1.0f / (float)cur_sample;

	printf("saving framebuffer (%d samples) to %s ... ", cur_sample, fname);
	fflush(stdout);

	if(!(fp = fopen(fname, "wb"))) {
		printf("failed: %s\n", strerror(errno));
		return -1;
	}
	fprintf(fp, "P6\n%d %d\n255\n", fbwidth, fbheight);

	for(i=0; i<fbwidth * fbheight; i++) {
		int r = pow(*fbptr++ * pixscale, INV_GAMMA) * 255.99;
		int g = pow(*fbptr++ * pixscale, INV_GAMMA) * 255.99;
		int b = pow(*fbptr++ * pixscale, INV_GAMMA) * 255.99;

		fputc(r > 255 ? 255 : r, fp);
		fputc(g > 255 ? 255 : g, fp);
		fputc(b > 255 ? 255 : b, fp);
	}
	fclose(fp);

	printf("done\n");
	return 0;
}
