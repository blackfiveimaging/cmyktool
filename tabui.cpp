#include <iostream>

#include <gtk/gtk.h>

#include "miscwidgets/uitab.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;

class UITab;

class TestUI
{
	public:
	TestUI();
	~TestUI();
	GtkWidget *window;
	GtkWidget *notebook;
	protected:
	friend class UITab;
};


TestUI::TestUI() : notebook(NULL)
{
	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),600,450);
	gtk_window_set_title (GTK_WINDOW (window), _("TestUI"));
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		(GtkSignalFunc) gtk_main_quit, NULL);
	gtk_widget_show(window);


	GtkWidget *hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),hbox);
	gtk_widget_show(hbox);

	notebook=gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook),true);
	gtk_box_pack_start(GTK_BOX(hbox),notebook,TRUE,TRUE,0);
	gtk_widget_show(GTK_WIDGET(notebook));
}

TestUI::~TestUI()
{
}


int main(int argc,char **argv)
{
	gtk_init(&argc,&argv);
	try
	{
		TestUI ui;

		new UITab(ui.notebook,"Tab 1");
		(new UITab(ui.notebook))->SetText("Tab 2");
		new UITab(ui.notebook);
		new UITab(ui.notebook);

		gtk_main();
	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	return(0);
}

