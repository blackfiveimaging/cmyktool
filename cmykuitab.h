#ifndef CMYKUITAB_H
#define CMYKUITAB_H

#include <gtk/gtk.h>
#include <gdk/gdkpixbuf.h>
#include "miscwidgets/uitab.h"
#include "conversionopts.h"

class UITab_RenderThread;



////// Class to hold view parameters - used for linking tabs with chain button
class CMYKUITab;
class CMYKUITab_View
{
	public:
	CMYKUITab_View() : w(0), h(0), xpan(0), ypan(0), zoom(false)
	{
	}
	CMYKUITab_View(CMYKUITab *source,int w,int h,int xpan,int ypan,bool zoom)
		: w(w),h(h),xpan(xpan),ypan(ypan),zoom(zoom)
	{
	}
	CMYKUITab *source;
	int w,h;
	int xpan,ypan;
	bool zoom;
};


class CMYKUITab : public UITab
{
	public:
	CMYKUITab(GtkWidget *parent,GtkWidget *notebook,CMYKConversionOptions &opts,JobDispatcher &dispatcher,const char *filename);
	~CMYKUITab();
	bool GetLinked();
	void SetLinked(bool linked);
	void SetImage(const char *filename);
	void SetView(CMYKUITab_View &view);
	CMYKUITab_View GetView();
	protected:
	static void ColorantsChanged(GtkWidget *wid,gpointer userdata);
	static void Save(GtkWidget *widget,gpointer userdata);
	static gboolean mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata);
	static void LinkToggled(GtkWidget *widget,gpointer userdata);
	static void ViewChanged(GtkWidget *widget,gpointer userdata);
	GtkWidget *parent;
	JobDispatcher &dispatcher;
	GtkWidget *hbox;
	GtkWidget *colsel;
	GtkWidget *pbview;
	GtkWidget *popup;
	GtkWidget *linkbutton;
	bool popupshown;
	CachedImage *image;
	DeviceNColorantList *collist;
	CMYKConversionOptions convopts;
	char *filename;
	Job *renderjob;
	GtkWidget *chain1;
	GtkWidget *chain2;
	CMYKUITab_View view;	// A saved view which is applied when a rendering job finishes
	friend class UITab_CacheJob;
	friend class UITab_RenderJob;
};

#endif

