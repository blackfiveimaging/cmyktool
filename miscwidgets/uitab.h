#ifndef UITAB_H
#define UITAB_H

#include <gtk/gtk.h>

class UITab;

class UITab
{
	public:
	UITab(GtkWidget *notebook,const char *tabname);
	~UITab();
	GtkWidget *GetBox();
	protected:
	static void deleteclicked(GtkWidget *wid,gpointer userdata);
	static void setclosebuttonsize(GtkWidget *wid,GtkStyle *style,gpointer userdata);
	static bool style_applied;
	static void apply_style();
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *notebook;
};

#endif

