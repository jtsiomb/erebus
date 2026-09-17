#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "xerebus.h"

static void cb_rendopt(Widget w, void *cls, void *calldata);
static void cb_postopt(Widget w, void *cls, void *calldata);
static void cb_bnrend(Widget w, void *cls, void *calldata);
static void create_menu(void);
static void file_menu_handler(Widget lst, void *cls, void *calldata);
static void help_menu_handler(Widget lst, void *cls, void *calldata);

Widget glview;

static Widget win, lb_status;


int init_gui(void)
{
	Widget main;
	Widget frm_view, frm_rend;
	Widget vbox, vbox_ui, hbox;
	Widget w;

	win = XmCreateMainWindow(app_shell, "mainwin", 0, 0);
	XtManageChild(win);

	create_menu();

#if XmVERSION >= 2
	main = XtVaCreateWidget("main", xmPanedWindowWidgetClass, win,
			XmNorientation, XmHORIZONTAL, NULL);
#else
	main = xm_rowcol(win, XmHORIZONTAL);
#endif

	glview = xm_gl(main, 640, 480, XM_GL_DOUBLE);
	vbox_ui = xm_rowcol(main, XmVERTICAL);

	frm_rend = xm_frame(vbox_ui, "Render options");
	vbox = xm_rows_begin(frm_rend, 2);
	xm_label(vbox, "method");
	xm_va_option_menu(vbox, cb_rendopt, 0, "Path tracing", "Ray tracing", NULL);
	xm_label(vbox, "resolution");
	hbox = xm_rowcol(vbox, XmHORIZONTAL);
	{
		w = xm_textfield(hbox, "1280", cb_rendopt, (void*)1);
		XtVaSetValues(w, XmNcolumns, 4, XmNmaxLength, 4, XmNmarginHeight, 2, NULL);
		XtAddCallback(w, XmNmodifyVerifyCallback, xm_cb_verify_numeric, 0);
		xm_label(hbox, "X");
		w = xm_textfield(hbox, "720", cb_rendopt, (void*)2);
		XtVaSetValues(w, XmNcolumns, 4, XmNmaxLength, 4, XmNmarginHeight, 2, NULL);
		XtAddCallback(w, XmNmodifyVerifyCallback, xm_cb_verify_numeric, 0);
	}
	xm_label(vbox, "samples");
	xm_spinboxi(vbox, ropt.nsamples, 1, 10000, True, cb_rendopt, (void*)3);
	xm_label(vbox, "");
	xm_checkbox(vbox, "denoise", ropt.denoise, cb_rendopt, (void*)4);
	xm_rows_end();


	frm_view = xm_frame(vbox_ui, "Post-process");
	vbox = xm_rows_begin(frm_view, 2);
	xm_label(vbox, "gamma");
	xm_sliderf(vbox, 0, ropt.gammaval, 1.0f, 3.0f, 1, cb_postopt, 0);
	xm_label(vbox, "tone mapping");
	w = xm_va_option_menu(vbox, cb_postopt, (void*)1, "None", "Reinhard", "ACES filmic", NULL);
	xm_select_option(w, 2);
	xm_rows_end();

	xm_button(vbox_ui, "Render", cb_bnrend, 0);
	w = xm_progress(vbox_ui);
	xm_set_progress(w, 80);

	XtManageChild(main);

	lb_status = xm_label(win, "");
	XtVaSetValues(win, XmNmessageWindow, lb_status, NULL);
	return 0;
}

static void cb_rendopt(Widget w, void *cls, void *calldata)
{
	int id = (int)(unsigned long)cls;
	int val;

	switch(id) {
	case 0:		/* rendering method option */
		ropt.rend = xm_selected_option(w);
		printf("render mode: %s\n", ropt.rend == 0 ? "path tracing" : "ray tracing");
		break;

	case 1:		/* width spinbox */
		ropt.xres = atoi(XmTextFieldGetString(w));
		if(0)
	case 2:		/* height spinbox */
		ropt.yres = atoi(XmTextFieldGetString(w));
		printf("resolution: %dx%d\n", ropt.xres, ropt.yres);
		break;

	case 3:		/* samples spinbox */
#ifdef HAVE_SPINBOX
		ropt.nsamples = ((XmSpinBoxCallbackStruct*)calldata)->position;
#else
		ropt.nsamples = atoi(XmTextFieldGetString(w));
#endif
		printf("samples: %d\n", ropt.nsamples);
		break;

	case 4:		/* denoise checkbox */
		ropt.denoise = ((XmToggleButtonCallbackStruct*)calldata)->set;
		printf("denoise: %s\n", ropt.denoise ? "on" : "off");
		break;

	default:
		break;
	}
}

static void cb_postopt(Widget w, void *cls, void *calldata)
{
	static const char *tmapstr[] = {"None", "Reinhard", "ACES filmic"};
	int id = (int)(unsigned long)cls;
	XmScaleCallbackStruct *sdata;

	switch(id) {
	case 0:		/* gamma */
		sdata = calldata;
		ropt.gammaval = (float)sdata->value * 0.1f;
		printf("gamma: %g\n", ropt.gammaval);
		break;

	case 2:		/* tone mapping operator */
		ropt.tmap = xm_selected_option(w);
		printf("tone mapping: %s\n", tmapstr[ropt.tmap]);
		break;

	default:
		break;
	}
}

static void cb_bnrend(Widget w, void *cls, void *calldata)
{
	printf("render!\n");
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
	int item = (int)(unsigned long)cls;
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
