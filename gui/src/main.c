#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "miniglut.h"
#include "shmfb.h"


int init(void);
void cleanup(void);
void display(void);
void reshape(int x, int y);
void keypress(unsigned char key, int x, int y);
void mouse(int bn, int st, int x, int y);
void motion(int x, int y);

static char shmpath[64];
static int rend_pid;
static int pfd[2];

int main(int argc, char **argv)
{
	int i, msg, status;
	char **rend_argv;

	sprintf(shmpath, "/erebus-gui.%d", getpid());

	/* launch the renderer */
	pipe(pfd);
	fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
	fcntl(pfd[1], F_SETFD, FD_CLOEXEC);

	if((rend_pid = fork()) == -1) {
		perror("failed to fork the renderer");
		return 1;
	}

	if(!rend_pid) {
		/* construct command line */
		if(!(rend_argv = malloc((argc + 3) * sizeof *argv))) {
			perror("failed to allocate argument vector");
			return 1;
		}
		rend_argv[0] = "erebus";
		for(i=1; i<argc; i++) {
			rend_argv[i] = argv[i];
		}
		rend_argv[i++] = "-shm";
		rend_argv[i++] = shmpath;
		rend_argv[i] = 0;

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
		wait(&status);
		return 1;
	}
	close(pfd[0]);

	/* map shared memory to get framebuffer size */
	if(shmfb_init(shmpath, 0, 0) == -1) {
		kill(rend_pid, SIGINT);
		wait(&status);
		return 1;
	}

	glutInit(&argc, argv);
	glutInitWindowSize(1280, 720);
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

	glutMainLoop();
	return 0;
}


int init(void)
{
	return 0;
}

void cleanup(void)
{
}

void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

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
