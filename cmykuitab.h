#ifndef CMYKUITAB_H
#define CMYKUITAB_H

#include <gtk/gtk.h>
#include "miscwidgets/uitab.h"
#include "conversionopts.h"

class UITab_RenderThread;


class CMYKUITab : public UITab
{
	public:
	CMYKUITab(GtkWidget *parent,GtkWidget *notebook,CMYKConversionOptions &opts,JobDispatcher &dispatcher,const char *filename);
	~CMYKUITab();
	void SetImage(const char *filename);
	static void ColorantsChanged(GtkWidget *wid,gpointer userdata);
	static void Save(GtkWidget *widget,gpointer userdata);
	static gboolean mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata);
	protected:
	GtkWidget *parent;
	JobDispatcher &dispatcher;
	GtkWidget *hbox;
	GtkWidget *colsel;
	GtkWidget *pbview;
	GtkWidget *popup;
	bool popupshown;
	CachedImage *image;
	DeviceNColorantList *collist;
	CMYKConversionOptions convopts;
	char *filename;
	Job *renderjob;
	friend class UITab_CacheJob;
	friend class UITab_RenderJob;
};

#endif

