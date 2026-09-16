#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <GL/gl.h>
#include "xerebus.h"
#include "ui.h"
#include "xmutil.h"
#include <X11/Xmu/Editres.h>

static void glinit(Widget w, void *cls);
static void gldraw(Widget w, void *cls);
static void glreshape(Widget w, int x, int y, void *cls);
static void glkeyb(Widget w, int key, int press, void *cls);
static void glmouse(Widget w, int bn, int st, int x, int y, void *cls);
static void glmotion(Widget w, int x, int y, void *cls);


XtAppContext app;
Widget app_shell;

struct render_opts ropt = {
	REND_PATH_TRACING,
	TONEMAP_ACESFILMIC,
	1280, 720,
	10,
	1,
	2.2
};

const char *glrend, *glvendor, *glver;

int main(int argc, char **argv)
{
	if(!(app_shell = XtVaOpenApplication(&app, "xerebus", 0, 0, &argc, argv,
					0, sessionShellWidgetClass, NULL))) {
		fprintf(stderr, "failed to initialize ui\n");
		return 1;
	}
	XtVaSetValues(app_shell, XmNtitle, "erebus GUI", NULL);
	XtVaSetValues(app_shell, XmNallowShellResize, True, NULL);

	XtAddEventHandler(app_shell, (EventMask)0, True, _XEditResCheckMessages, NULL);

	if(init_gui() == -1) {
		return 1;
	}

	xm_gl_init_func(glview, glinit, 0);
	xm_gl_draw_func(glview, gldraw, 0);
	xm_gl_reshape_func(glview, glreshape, 0);
	xm_gl_keyboard_func(glview, glkeyb, 0);
	xm_gl_mouse_func(glview, glmouse, 0);
	xm_gl_motion_func(glview, glmotion, 0);

	XtRealizeWidget(app_shell);
	XtAppMainLoop(app);
	return 0;
}

static void glinit(Widget w, void *cls)
{
	glvendor = (const char*)glGetString(GL_VENDOR);
	glrend = (const char*)glGetString(GL_RENDERER);
	glver = (const char*)glGetString(GL_VERSION);

	glClearColor(0.25, 0.25, 0.25, 1);
	glEnable(GL_CULL_FACE);
}

static void gldraw(Widget w, void *cls)
{
	glClear(GL_COLOR_BUFFER_BIT);

	xm_gl_swap_buffers(w);
}

static void glreshape(Widget w, int x, int y, void *cls)
{
	glViewport(0, 0, x, y);
}

static void glkeyb(Widget w, int key, int press, void *cls)
{
	if(!press) return;

	switch(key) {
	case 27:
		exit(0);

	default:
		break;
	}
}

static unsigned int bnstate;
static int prev_mx, prev_my;

static void glmouse(Widget w, int bn, int st, int x, int y, void *cls)
{
	if(st) {
		bnstate |= 1 << bn;
	} else {
		bnstate &= ~(1 << bn);
	}
	prev_mx = x;
	prev_my = y;
}

static void glmotion(Widget w, int x, int y, void *cls)
{
}
