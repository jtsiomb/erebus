#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include "glew.h"
#include "miniglut.h"
#include "shmfb.h"
#include "sdr.h"

#define STATUS_FONT		GLUT_BITMAP_HELVETICA_18

struct rect {
	int x, y, w, h;
};

#define STATUSBAR_HEIGHT	32

void procinput(int fd);
int spawn_renderer(int argc, char **argv);
int init(void);
void cleanup(void);
void display(void);
void reshape(int x, int y);
void keypress(unsigned char key, int x, int y);
void mouse(int bn, int st, int x, int y);
void motion(int x, int y);
void glprintf(int x, int y, const char *fmt, ...);

void sighandler(int s);

int parse_args(int argc, char **argv);

extern Display *miniglut_dpy;

static char shmpath[64];
static int found_size_arg;
static int width = 1280;
static int height = 720;
static int rend_pid, rend_pipe;
static int opt_wait;

static unsigned int sdr;

#define STATUS_LEN		80
static char st_text[2][STATUS_LEN + 1];
static int st_cur, st_pg;
static int xpos_prog, xpos_samp;

static int full_redraw;

static struct rect active[MAX_SHM_TILES];
static int num_active;


int main(int argc, char **argv)
{
	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	/* map shared memory to get framebuffer size */
	sprintf(shmpath, "/erebus-gui.%d", getpid());
	if(shmfb_create(shmpath, width, height) == -1) {
		kill(rend_pid, SIGINT);
		wait(0);
		return -1;
	}

	if(spawn_renderer(argc, argv) == -1) {
		return 1;
	}

	glutInit(&argc, argv);
	glutInitWindowSize(shmfb->width, shmfb->height + STATUSBAR_HEIGHT);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutCreateWindow("erebus GUI");

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keypress);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutPassiveMotionFunc(motion);
	glutExtInputFunc(rend_pipe, procinput);

	if(init() == -1) {
		return 1;
	}
	atexit(cleanup);

	glutMainLoop();
	return 0;
}

void procinput(int fd)
{
	int sz, c;
	char buf[64];
	char *src, *dst;

	if((sz = read(rend_pipe, buf, sizeof buf)) == -1) {
		return;
	}

	if(sz == 0) {
		if(waitpid(rend_pid, 0, WNOHANG) == rend_pid) {
			printf("renderer process exited\n");
			glutExtInputFunc(rend_pipe, 0);
			close(rend_pipe);
			rend_pipe = -1;
		}
		glutPostRedisplay();
		return;
	}

	src = buf;
	dst = st_text[st_pg ^ 1];
	while(sz > 0) {
		if(*src == 0 || *src == '\b') {
			if(*src == '\b') full_redraw = 1;
			glutPostRedisplay();
			src++;
			sz--;
		}

		c = *src++;
		sz--;

		if(c == '\n') {
			dst = st_text[st_pg];
			st_pg ^= 1;
			st_text[st_pg][st_cur] = 0;
			st_cur = 0;
			printf("> %s\n", st_text[st_pg]);
			glutPostRedisplay();
		} else if(isprint(c) && st_cur < STATUS_LEN) {
			dst[st_cur++] = c;
		}
	}
}

int spawn_renderer(int argc, char **argv)
{
	static char szarg_buf[32];
	int i, msg;
	char **rend_argv;
	int pfd[2];			/* renderer pipe */
	int errpipe[2];		/* second pipe used to detect exec failure */

	pipe(pfd);

	pipe(errpipe);
	fcntl(errpipe[0], F_SETFD, FD_CLOEXEC);
	fcntl(errpipe[1], F_SETFD, FD_CLOEXEC);

	if((rend_pid = fork()) == -1) {
		perror("failed to fork the renderer");
		return -1;
	}

	if(!rend_pid) {
		/* in child process, replace stdout/stderr with write end of the pipe */
		close(1);
		close(2);
		dup(pfd[1]);
		dup(pfd[1]);
		close(pfd[0]);
		close(pfd[1]);

		/* construct command line */
		if(!(rend_argv = malloc((argc + 6) * sizeof *argv))) {
			perror("failed to allocate argument vector");
			return -1;
		}
		rend_argv[0] = "erebus";
		for(i=1; i<argc; i++) {
			rend_argv[i] = argv[i];
		}
		if(!found_size_arg) {
			sprintf(szarg_buf, "%dx%d", width, height);
			rend_argv[i++] = "-s";
			rend_argv[i++] = szarg_buf;
			rend_argv[i++] = "-np";			/* disable stdout progress bar */
		}
		rend_argv[i++] = "-shm";
		rend_argv[i++] = shmpath;
		if(opt_wait) {
			rend_argv[i++] = "-wait";	/* wait for SIGCONT near the start of main */
		}
		rend_argv[i] = 0;

		/* add parent dir to the PATH if the binary is there */
		if(access("../erebus", X_OK) == 0) {
			putenv("PATH=..");
		}

		/* try to execute */
		execvp("erebus", rend_argv);
		perror("failed to execute erebus");
		msg = -1;
		write(errpipe[1], &msg, sizeof msg);
		_exit(1);
	}

	close(errpipe[1]);
	if(read(errpipe[0], &msg, sizeof msg) > 0) {
		/* child wrote an error code, exec failed */
		wait(0);
		return -1;
	}
	close(errpipe[0]);

	/* close write end of the pipe */
	close(pfd[1]);
	/* make read-end non-blocking since we're waiting on select */
	fcntl(pfd[0], F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
	rend_pipe = pfd[0];

	printf("spawned renderer process: %d\n", rend_pid);
	return 0;
}

int init(void)
{
	glewInit();

	glEnable(GL_CULL_FACE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, shmfb->pixels);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

	if(!(sdr = create_program_load("sdr/vertex.glsl", "sdr/pixel.glsl"))) {
		return -1;
	}
	set_uniform_float(sdr, "inv_gamma", 1.0f / 2.2f);

	xpos_samp = glutBitmapLength(STATUS_FONT, "(888/888)");
	xpos_prog = glutBitmapLength(STATUS_FONT, "100%00") + xpos_samp;
	return 0;
}

void cleanup(void)
{
	printf("shutting down\n");
	if(waitpid(rend_pid, 0, WNOHANG) <= 0) {
		printf("signalling the renderer to stop\n");
		kill(rend_pid, SIGINT);
		wait(0);
	}

	free_program(sdr);
}

void updatefb(void)
{
	int tileidx, num_done, num_act, bmidx;
	uint32_t bit;
	struct tile *tile;
	uint32_t done_tiles[TILES_BM_LEN], act_tiles[TILES_BM_LEN];

	num_active = 0;

	if(full_redraw) {
full:	full_redraw = 0;
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, shmfb->width, shmfb->height,
				GL_RGBA, GL_FLOAT, shmfb->pixels);
	} else {
		shmfb_lock();
		num_done = shmfb->num_done;
		num_act = shmfb->num_active;

		/* copy the list of completed tiles */
		memcpy(done_tiles, shmfb->done_tiles, sizeof done_tiles);
		memcpy(act_tiles, shmfb->act_tiles, sizeof act_tiles);

		shmfb->num_done = 0;
		memset(shmfb->done_tiles, 0, sizeof shmfb->done_tiles);
		shmfb_unlock();

		bmidx = 0;
		bit = 1;
		tileidx = 0;
		while(num_act > 0) {
			if(act_tiles[bmidx] & bit) {
				tile = shmfb->tiles + tileidx;
				active[num_active].x = tile->x;
				active[num_active].y = tile->y;
				active[num_active].w = tile->width;
				active[num_active++].h = tile->height;
				num_act--;
			}
			bit <<= 1;
			if(!bit) {
				bit = 1;
				bmidx++;
			}
			tileidx++;
		}

		if(num_done >= MAX_SHM_TILES) {
			/* TODO: maybe even if a large fraction of the tiles need updating? */
			goto full;
		}

		bmidx = 0;
		bit = 1;
		tileidx = 0;
		while(num_done > 0) {
			if(done_tiles[bmidx] & bit) {
				tile = shmfb->tiles + tileidx;
				glTexSubImage2D(GL_TEXTURE_2D, 0, tile->x, tile->y, tile->width,
						tile->height, GL_RGBA, GL_FLOAT, shmfb->pixels + tile->fboffs);
				num_done--;
			}
			bit <<= 1;
			if(!bit) {
				bit = 1;
				bmidx++;
			}
			tileidx++;
		}
	}
}

void display(void)
{
	int i, progr;
	struct rect *rect;

	progr = shmfb_progress();

	updatefb();

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();
	glTranslatef(0, STATUSBAR_HEIGHT, 0);

	bind_program(sdr);

	glBegin(GL_QUADS);
	glColor3f(1, 1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(0, 0);
	glTexCoord2f(1, 1);
	glVertex2f(width, 0);
	glTexCoord2f(1, 0);
	glVertex2f(width, height);
	glTexCoord2f(0, 0);
	glVertex2f(0, height);
	glEnd();

	bind_program(0);

	glBegin(GL_LINES);
	rect = active;
	for(i=0; i<num_active; i++) {
		glColor3f(0, 0.5, 0);
		glVertex2f(rect->x, rect->y);
		glVertex2f(rect->x + rect->w, rect->y);
		glVertex2f(rect->x + rect->w, rect->y);
		glVertex2f(rect->x + rect->w, rect->y + rect->h);
		glVertex2f(rect->x + rect->w, rect->y + rect->h);
		glVertex2f(rect->x, rect->y + rect->h);
		glVertex2f(rect->x, rect->y + rect->h);
		glVertex2f(rect->x, rect->y);
		rect++;
	}
	glEnd();

	glPopMatrix();

	glColor3f(0.1, 0.2, 0.5);
	glRecti(0, 0, progr * width / 1024, STATUSBAR_HEIGHT);

	glColor3f(1, 1, 1);
	glprintf(10, 10, st_text[st_pg]);

	glprintf(width - xpos_prog, 10, "%3d%%", (progr * 100) >> 10);
	glprintf(width - xpos_samp, 10, "(%3d/%-3d)", shmfb->cur_sample + 1, shmfb->total_samples);

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

void reshape(int x, int y)
{
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, x, 0, y, -1, 1);
}

void keypress(unsigned char key, int x, int y)
{
	switch(key) {
	case 27:
		exit(0);

	case ' ':
		kill(rend_pid, SIGCONT);
		break;

	default:
		break;
	}
}

void mouse(int bn, int st, int x, int y)
{
}

void motion(int x, int y)
{
}

void sighandler(int s)
{
	if(s == SIGCHLD) {
		close(rend_pipe);
	}
}

/* silently ignores all arguments we don't need, the rest will be passed on to
 * the renderer
 */
int parse_args(int argc, char **argv)
{
	int i;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-size") == 0) {
				if(!argv[++i] || sscanf(argv[i], "%dx%d", &width, &height) != 2) {
					fprintf(stderr, "%s must be followed by <width>x<height>\n", argv[i - 1]);
					return -1;
				}
				found_size_arg = 1;

			} else if(strcmp(argv[i], "-wait") == 0) {
				opt_wait = 1;
			}
		}
	}

	return 0;
}

void glprintf(int x, int y, const char *fmt, ...)
{
	va_list ap;
	char buf[256];

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glRasterPos2i(x, y);

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	glutBitmapString(STATUS_FONT, buf);

	glPopMatrix();
}
