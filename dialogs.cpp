#include <iostream>

#include <gtk/gtk.h>
#include "imagesource.h"
#include "pathsupport.h"
#include "patheditor.h"
#include "generaldialogs.h"
#include "profileselector.h"
#include "intentselector.h"
#include "simplecombo.h"

#include "util.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)
#define N_(x) gettext_noop(x)

#include "dialogs.h"

using namespace std;

#define RESPONSE_SAVE 1


struct prefsdialogdata
{
	ProfileManager *pm;
	GtkWidget *monprof;
	GtkWidget *monactive;
	GtkWidget *cmpatheditor;
	GtkWidget *argyllpatheditor;
	GtkWidget *gspatheditor;
};


static void checkbox_changed(GtkWidget *wid,gpointer *ud)
{
	prefsdialogdata *ob=(prefsdialogdata *)ud;
	int ma=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ob->monactive));
	gtk_widget_set_sensitive(ob->monprof,ma);
}


static void paths_changed(GtkWidget *widget,gpointer user_data)
{
	struct prefsdialogdata *dd=(struct prefsdialogdata *)user_data;
	patheditor_get_paths(PATHEDITOR(widget),dd->pm);
	profileselector_refresh(PROFILESELECTOR(dd->monprof));
}


void PreferencesDialog(GtkWindow *parent,CMYKTool_Core &core)
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *profilepage;
	GtkWidget *extutilspage;
	GtkWidget *vbox;
	GtkWidget *label;

	ProfileManager &pm=core.GetProfileManager();

	char *savedpaths=pm.GetPaths();

	struct prefsdialogdata dd;
	dd.pm=&pm;

 	dialog=gtk_dialog_new_with_buttons(_("Preferences..."),
		parent,GtkDialogFlags(0),
		GTK_STOCK_SAVE,RESPONSE_SAVE,
		GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK,GTK_RESPONSE_OK,
		NULL);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 450,350);

	notebook=gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),notebook,TRUE,TRUE,0);
	gtk_widget_show(GTK_WIDGET(notebook));

	// Colour management page:

	label=gtk_label_new(_("Colour Management"));
	gtk_widget_show(label);

	profilepage=gtk_vbox_new(FALSE,0);
	gtk_widget_show(GTK_WIDGET(profilepage));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),GTK_WIDGET(profilepage),GTK_WIDGET(label));


	GtkWidget *table=gtk_table_new(2,4,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table),8);
	gtk_table_set_col_spacing(GTK_TABLE(table),0,10);
	gtk_table_set_row_spacings(GTK_TABLE(table),8);
	gtk_box_pack_start(GTK_BOX(profilepage),table,TRUE,TRUE,5);
	gtk_widget_show(table);

	int row=0;

	// Paths...

	label=gtk_label_new(_("ICC colour profile paths"));
	gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
	gtk_table_attach(GTK_TABLE(table),label,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
	gtk_widget_show(label);

	++row;


	GtkAttachOptions gao = (GtkAttachOptions)(GTK_EXPAND|GTK_FILL);
	dd.cmpatheditor = patheditor_new(&pm);
	g_signal_connect(dd.cmpatheditor,"changed",G_CALLBACK(paths_changed),&dd);
	gtk_table_attach(GTK_TABLE(table),dd.cmpatheditor,0,2,row,row+1,gao,gao,0,0);
	gtk_widget_show(dd.cmpatheditor);

	++row;



	// Profiles...

	dd.monactive=gtk_check_button_new_with_label(_("Monitor Profile:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd.monactive),pm.FindInt("MonitorProfileActive"));
	g_signal_connect(G_OBJECT(dd.monactive),"toggled",G_CALLBACK(checkbox_changed),&dd);
	gtk_table_attach(GTK_TABLE(table),dd.monactive,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
	gtk_widget_show(dd.monactive);

	dd.monprof=profileselector_new(&pm,IS_TYPE_RGB,false);
	gtk_table_attach(GTK_TABLE(table),dd.monprof,1,2,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
	gtk_widget_show(dd.monprof);
	profileselector_set_filename(PROFILESELECTOR(dd.monprof),pm.FindString("MonitorProfile"));

	++row;


	// External utilities

	label=gtk_label_new(_("External utilities"));
	gtk_widget_show(label);

	extutilspage=gtk_vbox_new(FALSE,0);
	gtk_widget_show(GTK_WIDGET(extutilspage));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),GTK_WIDGET(extutilspage),GTK_WIDGET(label));


	table=gtk_table_new(2,4,FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table),8);
	gtk_table_set_col_spacing(GTK_TABLE(table),0,10);
	gtk_table_set_row_spacings(GTK_TABLE(table),8);
	gtk_box_pack_start(GTK_BOX(extutilspage),table,TRUE,TRUE,5);
	gtk_widget_show(table);

	row=0;

	// Paths...

	label=gtk_label_new(_("Search paths for Argyll utilities (TIFFGamut, ColLink, etc.)"));
	gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
	gtk_table_attach(GTK_TABLE(table),label,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
	gtk_widget_show(label);

	++row;

	// FIXME - replace test_sp with a real searchpath
	SearchPathHandler test_sp;
	test_sp.AddPath(core.FindString("ArgyllPath"));
	dd.argyllpatheditor = patheditor_new(&test_sp);
//	g_signal_connect(dd.cmpatheditor,"changed",G_CALLBACK(paths_changed),&dd);
	gtk_table_attach(GTK_TABLE(table),dd.argyllpatheditor,0,2,row,row+1,gao,gao,0,0);
	gtk_widget_show(dd.argyllpatheditor);

	++row;


	label=gtk_label_new(_("Search paths for GhostScript executable"));
	gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
	gtk_table_attach(GTK_TABLE(table),label,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
	gtk_widget_show(label);

	++row;

	// FIXME - replace test_sp2 with a real searchpath
	SearchPathHandler test_sp2;
	dd.gspatheditor = patheditor_new(&test_sp2);
//	g_signal_connect(dd.cmpatheditor,"changed",G_CALLBACK(paths_changed),&dd);
	gtk_table_attach(GTK_TABLE(table),dd.gspatheditor,0,2,row,row+1,gao,gao,0,0);
	gtk_widget_show(dd.gspatheditor);

	++row;


	gtk_widget_show(dialog);
	
	bool done=false;
	while(!done)
	{
		gint result=gtk_dialog_run(GTK_DIALOG(dialog));
		switch(result)
		{
			case GTK_RESPONSE_CANCEL:
				pm.ClearPaths();
				pm.AddPath(savedpaths);
				done=true;
				break;
			case GTK_RESPONSE_OK:
			case RESPONSE_SAVE:
				done=true;
				pm.SetString("MonitorProfile",profileselector_get_filename(PROFILESELECTOR(dd.monprof)));
				pm.SetInt("MonitorProfileActive",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd.monactive)));

				char *tmppaths=pm.GetPaths();
				pm.SetString("ProfilePath",tmppaths);
				free(tmppaths);

				patheditor_get_paths(PATHEDITOR(dd.argyllpatheditor),&test_sp);
				char *argyllpath=test_sp.GetPaths();
				Debug[TRACE] << "ArgyllPath: " << argyllpath << endl;
				core.SetString("ArgyllPath",argyllpath);
				free(argyllpath);

				patheditor_get_paths(PATHEDITOR(dd.gspatheditor),&test_sp2);
				char *gspath=test_sp2.GetPaths();
				Debug[TRACE] << "GhostscriptPath: " << gspath << endl;
				core.SetString("GhostscriptPath",gspath);
				free(gspath);

				if(result==RESPONSE_SAVE)
				{
					core.SaveConfig();
				}
				break;
		}
	}
	gtk_widget_destroy(dialog);

	if(savedpaths)
		free(savedpaths);
}

