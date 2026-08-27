#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "glew.h"
#include "miniglut.h"
#include "shmfb.h"
#include "sdr.h"

#define STATUSBAR_HEIGHT	32

int spawn_renderer(int argc, char **argv);
int init(void);
void cleanup(void);
void display(void);
void idle(void);
void reshape(int x, int y);
void keypress(unsigned char key, int x, int y);
void mouse(int bn, int st, int x, int y);
void motion(int x, int y);

int parse_args(int argc, char **argv);


static char shmpath[64];
static int found_size_arg;
static int width = 1280;
static int height = 720;
static int rend_pid;
static int pfd[2];

static unsigned int sdr;


int main(int argc, char **argv)
{
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
	glutIdleFunc(idle);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keypress);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutPassiveMotionFunc(motion);

	if(init() == -1) {
		return 1;
	}
	atexit(cleanup);

	glutMainLoop();
	return 0;
}

int spawn_renderer(int argc, char **argv)
{
	static char szarg_buf[32];
	int i, msg;
	char **rend_argv;

	sprintf(shmpath, "/erebus-gui.%d", getpid());

	/* launch the renderer */
	pipe(pfd);
	fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
	fcntl(pfd[1], F_SETFD, FD_CLOEXEC);

	if((rend_pid = fork()) == -1) {
		perror("failed to fork the renderer");
		return -1;
	}

	if(!rend_pid) {
		/* construct command line */
		if(!(rend_argv = malloc((argc + 5) * sizeof *argv))) {
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
		write(pfd[1], &msg, sizeof msg);
		_exit(1);
	}

	close(pfd[1]);
	if(read(pfd[0], &msg, sizeof msg) > 0) {
		/* child wrote an error code, exec failed */
		wait(0);
		return -1;
	}
	close(pfd[0]);

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
	glEnable(GL_TEXTURE_2D);

	if(!(sdr = create_program_load("sdr/vertex.glsl", "sdr/pixel.glsl"))) {
		return -1;
	}
	set_uniform_float(sdr, "inv_gamma", 1.0f / 2.2f);
	bind_program(sdr);

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
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, shmfb->pixels);
}

void display(void)
{
	int rendering, progr;

	progr = shmfb_progress();
	rendering = shmfb_rendering();

	updatefb();

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();
	glTranslatef(0, STATUSBAR_HEIGHT, 0);

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

	glPopMatrix();

	glColor3f(0.1, 0.3, 1.0);
	glRecti(0, 0, progr / 1024 * width, STATUSBAR_HEIGHT);

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

void idle(void)
{
	glutPostRedisplay();
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
