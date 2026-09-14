#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <GL/gl.h>
#include "xmutil.h"

static void glinit(Widget w, void *cls);
static void gldraw(Widget w, void *cls);
static void glreshape(Widget w, int x, int y, void *cls);
static void glkeyb(Widget w, int key, int press, void *cls);
static void glmouse(Widget w, int bn, int st, int x, int y, void *cls);
static void glmotion(Widget w, int x, int y, void *cls);
static int init_gui(void);
static void rmode_handler(Widget w, void *cls, void *calldata);
static void tmap_handler(Widget w, void *cls, void *calldata);
static void cbox_handler(Widget w, void *cls, void *calldata);
static void slider_handler(Widget w, void *cls, void *calldata);
static void create_menu(void);
static void file_menu_handler(Widget lst, void *cls, void *calldata);
static void help_menu_handler(Widget lst, void *cls, void *calldata);


XtAppContext app;
Widget app_shell;

static const char *glrend, *glvendor, *glver;

static Widget win, lb_status, viewport;
static Widget slider_hrot, slider_vrot, slider_dist;
static int denoise = 1;
static float gammaval = 2.2;

static int sphere, box;

int main(int argc, char **argv)
{
	if(!(app_shell = XtVaOpenApplication(&app, "xerebus", 0, 0, &argc, argv,
					0, sessionShellWidgetClass, NULL))) {
		fprintf(stderr, "failed to initialize ui\n");
		return 1;
	}
	XtVaSetValues(app_shell, XmNtitle, "erebus GUI", NULL);
	XtVaSetValues(app_shell, XmNallowShellResize, True, NULL);

	win = XmCreateMainWindow(app_shell, "mainwin", 0, 0);
	XtManageChild(win);

	if(init_gui() == -1) {
		return 1;
	}

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

static int init_gui(void)
{
	Widget main;
	Widget frm_view, frm_rend;
	Widget vbox, vbox_ui;

	create_menu();

	main = XtVaCreateWidget("main", xmPanedWindowWidgetClass, win,
			XmNorientation, XmHORIZONTAL, NULL);

	vbox_ui = xm_rowcol(main, XmVERTICAL);

	viewport = xm_gl(main, 640, 480, XM_GL_DOUBLE);
	xm_gl_init_func(viewport, glinit, 0);
	xm_gl_draw_func(viewport, gldraw, 0);
	xm_gl_reshape_func(viewport, glreshape, 0);
	xm_gl_keyboard_func(viewport, glkeyb, 0);
	xm_gl_mouse_func(viewport, glmouse, 0);
	xm_gl_motion_func(viewport, glmotion, 0);

	frm_rend = xm_frame(vbox_ui, "Renderer options");
	vbox = xm_rowcol(frm_rend, XmVERTICAL);
	xm_va_option_menu(vbox, rmode_handler, 0, "Path tracing", "Ray tracing", (void*)0);
	xm_checkbox(vbox, "denoise", denoise, cbox_handler, 0);

	frm_view = xm_frame(vbox_ui, "Post-process");
	vbox = xm_rowcol(frm_view, XmVERTICAL);
	slider_hrot = xm_sliderf(vbox, "gamma", gammaval, 1.0f, 3.0f, slider_handler, 0);
	xm_va_option_menu(vbox, tmap_handler, 0, "None", "Reinhard", "ACES filmic", (void*)0);

	XtManageChild(main);

	lb_status = xm_label(win, "");
	XtVaSetValues(win, XmNmessageWindow, lb_status, NULL);
	return 0;
}

static void rmode_handler(Widget w, void *cls, void *calldata)
{
}

static void tmap_handler(Widget w, void *cls, void *calldata)
{
}

static void cbox_handler(Widget w, void *cls, void *calldata)
{
	int id = (int)cls;
	XmToggleButtonCallbackStruct *cdata = calldata;
}

static void slider_handler(Widget w, void *cls, void *calldata)
{
	int id = (int)cls;
	float val = xm_get_sliderf_value(w);
}

static void create_menu(void)
{
	XmString sfile, shelp, snew, sopen, ssave, ssave_key, squit, sabout, squit_key;
	Widget menubar;

	sfile = XmStringCreateSimple("File");
	shelp = XmStringCreateSimple("Help");
	menubar = XmVaCreateSimpleMenuBar(win, "menubar", XmVaCASCADEBUTTON, sfile, 'F',
			XmVaCASCADEBUTTON, shelp, 'H', NULL);
	XtManageChild(menubar);
	XmStringFree(sfile);
	XmStringFree(shelp);

	snew = XmStringCreateSimple("New");
	sopen = XmStringCreateSimple("Open");
	ssave = XmStringCreateSimple("Save");
	ssave_key = XmStringCreateSimple("Ctrl-S");
	squit = XmStringCreateSimple("Quit");
	squit_key = XmStringCreateSimple("Ctrl-Q");
	XmVaCreateSimplePulldownMenu(menubar, "filemenu", 0, file_menu_handler,
			XmVaPUSHBUTTON, snew, 'N', NULL, NULL,
			XmVaPUSHBUTTON, sopen, 'O', NULL, NULL,
			XmVaPUSHBUTTON, ssave, 'S', "Ctrl<Key>s", ssave_key,
			XmVaPUSHBUTTON, squit, 'Q', "Ctrl<Key>q", squit_key,
			NULL);
	XmStringFree(ssave);
	XmStringFree(ssave_key);
	XmStringFree(squit);
	XmStringFree(squit_key);

	sabout = XmStringCreateSimple("About");
	XmVaCreateSimplePulldownMenu(menubar, "helpmenu", 1, help_menu_handler,
			XmVaPUSHBUTTON, sabout, 'A', NULL, NULL, NULL);
	XmStringFree(sabout);
}

static void file_menu_handler(Widget lst, void *cls, void *calldata)
{
	int item = (int)cls;
	switch(item) {
	case 0:	/* new */
	case 1:	/* open */
	case 2:	/* save */
		break;
	case 3: /* quit */
		exit(0);
	}
}


static void help_menu_handler(Widget lst, void *cls, void *calldata)
{
	static const char *about_fmt = "erebus GUI\n\n"
		"%s\n\n"
		"OpenGL information:\n"
		" Vendor: %s\n"
		" Renderer: %s\n"
		" Version: %s\n"
		"\n";
	char *textbuf;
	int len;

#ifdef XmVERSION_STRING
	const char *motifver = XmVERSION_STRING;
#else
	char motifver[64];
	sprintf(motifver, "Motif %d.%d", XmVERSION, XmREVISION);
#endif

	if(!glrend) {
		glrend = glver = glvendor = "???";
	}
	len = strlen(about_fmt) + strlen(motifver) + strlen(glrend) +
		strlen(glvendor) + strlen(glver);
	if(!(textbuf = malloc(len + 1))) {
		return;
	}
	sprintf(textbuf, about_fmt, motifver, glvendor, glrend, glver);

	messagebox(XmDIALOG_INFORMATION, "About xerebus", textbuf);
	free(textbuf);
}

static void set_status(const char *s)
{
	XmString xs = XmStringCreateSimple((char*)s);
	XtVaSetValues(lb_status, XmNlabelString, xs, NULL);
	XmStringFree(xs);
}
