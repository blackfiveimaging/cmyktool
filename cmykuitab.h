#ifndef CMYKUITAB_H
#define CMYKUITAB_H

#include <gtk/gtk.h>
#include <gdk/gdkpixbuf.h>
#include "miscwidgets/uitab.h"
#include "miscwidgets/spinner.h"
#include "conversionopts.h"
#include "support/debug.h"
#include "support/ptmutex.h"

#include "cmyktool_core.h"

enum CMYKTabDisplayMode {CMYKDISPLAY_INSPECT,CMYKDISPLAY_PROOF_ADAPT_WHITE,CMYKDISPLAY_PROOF};


// Class to provide a spinner widget that can be shown and hidden.
// Autonomously spins by way of a timeout function, which avoids thread issues.

class TabSpinner : public Spinner
{
	public:
	TabSpinner() : Spinner(), active(false), shown(false), frame(0)
	{
		gtk_widget_hide(GetWidget());
	}
	~TabSpinner()
	{
		Stop();
		while(shown)
		{
			// Need to wait until it's safe to delete, so pump GTK event loop...
			gtk_main_iteration_do(TRUE);
		}
	}
	void Start()
	{
		active=true;
		g_timeout_add(100,timeoutfunc,this);
	}
	void Stop()
	{
		active=false;
	}
	protected:
	static gboolean timeoutfunc(gpointer userdata)
	{
		TabSpinner *s=(TabSpinner *)userdata;
		if(!s->shown)
		{
			gtk_widget_show(s->GetWidget());
			s->shown=true;
		}

		s->frame=(s->frame+1)&7;
		s->SetFrame(s->frame);
		if(!s->active)
		{
			gtk_widget_hide(s->GetWidget());
			s->shown=false;
		}
		return(s->active);
	}
	bool active;
	bool shown;
	int frame;
};


////// Class to hold view parameters - used for linking tabs with chain button
class CMYKUITab;
class CMYKUITab_View
{
	public:
	CMYKUITab_View(CMYKUITab *source) : source(source), w(0), h(0), xpan(0), ypan(0), zoom(false), displaymode(CMYKDISPLAY_INSPECT)
	{
	}
	CMYKUITab_View(CMYKUITab *source,int w,int h,int xpan,int ypan,bool zoom, CMYKTabDisplayMode displaymode)
		: source(source), w(w),h(h),xpan(xpan),ypan(ypan),zoom(zoom), displaymode(displaymode)
	{
	}
	CMYKUITab_View &operator=(CMYKUITab_View &other)
	{
		w=other.w;
		h=other.h;
		xpan=other.xpan;
		ypan=other.ypan;
		zoom=other.zoom;
		displaymode=other.displaymode;
		return(*this);
	}
	CMYKUITab *source;
	int w,h;
	int xpan,ypan;
	bool zoom;
	CMYKTabDisplayMode displaymode;
};


class CMYKUITab : public UITab
{
	public:
	CMYKUITab(GtkWidget *parent,GtkWidget *notebook,CMYKTool_Core &core,const char *filename);
	~CMYKUITab();
	bool GetLinked();
	void SetLinked(bool linked);
	void SetImage(const char *filename);
	void SetView(CMYKUITab_View &view);
	CMYKUITab_View GetView();
	void SetDisplayMode(CMYKTabDisplayMode mode);
	protected:
	static void ColorantsChanged(GtkWidget *wid,gpointer userdata);
	static void Save(GtkWidget *widget,gpointer userdata);
	static gboolean mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata);
	static void LinkToggled(GtkWidget *widget,gpointer userdata);
	static void ViewChanged(GtkWidget *widget,gpointer userdata);
	static void DisplayModeChanged(GtkWidget *widget,gpointer userdata);
	static void MouseMove(GtkWidget *widget,gpointer userdata);
	void Redraw();
	CMYKTool_Core &core;
	GtkWidget *parent;
	GtkWidget *hbox;
	GtkWidget *colsel;
	GtkWidget *pbview;
	GtkWidget *displaymode;
	GtkWidget *popup;
	GtkWidget *linkbutton;
	GtkWidget *sumlabel;
	bool popupshown;
	CachedImage *image;
	DeviceNColorantList *collist;
	char *filename;
	Job *renderjob;
	GtkWidget *chain1;
	GtkWidget *chain2;
	CMYKUITab_View view;	// A saved view which is applied when a rendering job finishes
	TabSpinner spinner;
	friend class UITab_CacheJob;
	friend class UITab_RenderJob;
	friend class UITab_SaveDialog;
};

#endif

