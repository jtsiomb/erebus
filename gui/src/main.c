#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include "glew.h"
#include "miniglut.h"
#include "shmfb.h"
#include "sdr.h"

#define STATUSBAR_HEIGHT	32

int proc_rend_inp(void);
int spawn_renderer(int argc, char **argv);
int init(void);
void cleanup(void);
void display(void);
void reshape(int x, int y);
void keypress(unsigned char key, int x, int y);
void mouse(int bn, int st, int x, int y);
void motion(int x, int y);
void glprintf(int x, int y, const char *fmt, ...);

int parse_args(int argc, char **argv);

extern Display *miniglut_dpy;
static int redisp_pending;

static char shmpath[64];
static int found_size_arg;
static int width = 1280;
static int height = 720;
static int rend_pid, rend_pipe;

static unsigned int sdr;

#define STATUS_LEN		80
static char st_text[2][STATUS_LEN + 1];
static int st_cur, st_pg;

static struct tile dirty[4096];
static int ndirty;


int main(int argc, char **argv)
{
	int xfd, maxfd;

	if(parse_args(argc, argv) == -1) {
		return 1;
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

	if(init() == -1) {
		return 1;
	}
	atexit(cleanup);

	xfd = ConnectionNumber(miniglut_dpy);
	maxfd = xfd > rend_pipe ? xfd : rend_pipe;

	for(;;) {
		fd_set rdset;

		FD_ZERO(&rdset);
		if(rend_pipe >= 0) {
			FD_SET(rend_pipe, &rdset);
		}
		FD_SET(xfd, &rdset);

		if(select(maxfd + 1, &rdset, 0, 0, 0) == -1) {
			if(errno == EINTR) continue;
			perror("select failed");
			break;
		}

		if(rend_pipe >= 0 && FD_ISSET(rend_pipe, &rdset)) {
			proc_rend_inp();
		}

		if(FD_ISSET(xfd, &rdset) || redisp_pending) {
			printf("DBG X proc\n");
			glutMainLoopEvent();
		}
	}
	return 0;
}

int proc_rend_inp(void)
{
	static unsigned char *done_data = (unsigned char*)dirty;
	static int done_idx = -1;

	int sz;
	char buf[64];

	if((sz = read(rend_pipe, buf, sizeof buf)) == -1) {
		return 0;
	}

	if(sz == 0) {
		if(waitpid(rend_pid, 0, WNOHANG) == rend_pid) {
			printf("renderer process exited\n");
			close(rend_pipe);
			rend_pipe = -1;
			return -1;
		}
		return 0;
	}

	printf("DBG input pipe (%d)\n", sz);
	while(sz > 0) {
		char *src = buf;
		if(done_idx == -1) {
			char *dst = st_text[st_pg ^ 1];
rdinp:		if(*src == 0) {
				/* starting a tile completion data packet */
				src++;
				sz--;
				goto rddone;
			}

			while(sz-- && st_cur < STATUS_LEN) {
				int c = *src++;

				if(c == '\n') {
					dst = st_text[st_pg];
					st_pg ^= 1;
					st_text[st_pg][st_cur] = 0;
					st_cur = 0;
					printf("INPUT: %s\n", st_text[st_pg]);
					redisp_pending = 1;
					glutPostRedisplay();

				} else if(isprint(c)) {
					dst[st_cur++] = c;
				}
			}
		} else {
rddone:		while(sz && done_idx < sizeof done_data) {
				sz--;
				done_data[done_idx++] = *src++;
			}
			if(done_idx >= sizeof done_data) {
				/* process completion packet */
				printf("pkt %d %d %d %d\n", dirty[ndirty].x, dirty[ndirty].y,
						dirty[ndirty].width, dirty[ndirty].height);
				ndirty++;
				done_idx = -1;
				goto rdinp;
			}
		}
	}

	if(ndirty) {
		redisp_pending = 1;
		glutPostRedisplay();
	}
	return 0;
}

int spawn_renderer(int argc, char **argv)
{
	static char szarg_buf[32];
	int i, msg;
	char **rend_argv;
	int pfd[2];			/* renderer pipe */
	int errpipe[2];		/* second pipe used to detect exec failure */

	sprintf(shmpath, "/erebus-gui.%d", getpid());

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

	/* map shared memory to get framebuffer size */
	if(shmfb_create(shmpath, width, height) == -1) {
		kill(rend_pid, SIGINT);
		wait(0);
		return -1;
	}

	return 0;
}

int init(void)
{
	glewInit();

	glEnable(GL_CULL_FACE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, 0);

	if(!(sdr = create_program_load("sdr/vertex.glsl", "sdr/pixel.glsl"))) {
		return -1;
	}
	set_uniform_float(sdr, "inv_gamma", 1.0f / 2.2f);

	ndirty = 0;
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
	int i;
	struct tile *tile = dirty;

	for(i=0; i<ndirty; i++) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, tile->x, tile->y, tile->width, tile->height,
				GL_RGBA, GL_FLOAT, shmfb->pixels);
		tile++;
	}
	ndirty = 0;
}

void display(void)
{
	int progr;

	redisp_pending = 0;

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

	glPopMatrix();

	glColor3f(0.1, 0.2, 0.5);
	glRecti(0, 0, progr * width / 1024, STATUSBAR_HEIGHT);

	glColor3f(1, 1, 1);
	glprintf(10, 10, st_text[st_pg]);

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

	glutBitmapString(GLUT_BITMAP_HELVETICA_18, buf);

	glPopMatrix();
}
