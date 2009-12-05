#include <iostream>

#include <gtk/gtk.h>
#include "imagesource/imagesource.h"
#include "support/pathsupport.h"
#include "miscwidgets/patheditor.h"
#include "miscwidgets/generaldialogs.h"
#include "profilemanager/profileselector.h"
#include "profilemanager/intentselector.h"
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
	GtkWidget *rgbprof;
	GtkWidget *cmykprof;
	GtkWidget *monprof;
	GtkWidget *outputprof;
	GtkWidget *rgbactive;
	GtkWidget *cmykactive;
	GtkWidget *monactive;
	GtkWidget *outputactive;
	GtkWidget *patheditor;
};


static void checkbox_changed(GtkWidget *wid,gpointer *ud)
{
	prefsdialogdata *ob=(prefsdialogdata *)ud;

	int pa=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ob->outputactive));
	int ra=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ob->rgbactive));
	int ca=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ob->cmykactive));
	int ma=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ob->monactive));

	gtk_widget_set_sensitive(ob->outputprof,pa);
	gtk_widget_set_sensitive(ob->rgbprof,ra);
	gtk_widget_set_sensitive(ob->cmykprof,ca);
	gtk_widget_set_sensitive(ob->monprof,ma);
}


static void paths_changed(GtkWidget *widget,gpointer user_data)
{
	struct prefsdialogdata *dd=(struct prefsdialogdata *)user_data;
	patheditor_get_paths(PATHEDITOR(widget),dd->pm);
	profileselector_refresh(PROFILESELECTOR(dd->rgbprof));
	profileselector_refresh(PROFILESELECTOR(dd->cmykprof));
	profileselector_refresh(PROFILESELECTOR(dd->monprof));
}


void PreferencesDialog(GtkWindow *parent,ProfileManager &pm)
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *profilepage;
	GtkWidget *pathspage;
	GtkWidget *vbox;
	GtkWidget *label;
//	GtkWidget *argyllpath_tg;
//	GtkWidget *argyllpath_il;

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
	gtk_box_pack_start(GTK_BOX(profilepage),table,FALSE,FALSE,5);
	gtk_widget_show(table);


	dd.rgbactive=gtk_check_button_new_with_label(_("Default RGB Profile:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd.rgbactive),pm.FindInt("DefaultRGBProfileActive"));
	g_signal_connect(G_OBJECT(dd.rgbactive),"toggled",G_CALLBACK(checkbox_changed),&dd);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.rgbactive,0,1,0,1);
	gtk_widget_show(dd.rgbactive);

	dd.rgbprof=profileselector_new(&pm,IS_TYPE_RGB,false);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.rgbprof,1,2,0,1);
	gtk_widget_show(dd.rgbprof);
	profileselector_set_filename(PROFILESELECTOR(dd.rgbprof),pm.FindString("DefaultRGBProfile"));


	dd.cmykactive=gtk_check_button_new_with_label(_("Default CMYK Profile:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd.cmykactive),pm.FindInt("DefaultCMYKProfileActive"));
	g_signal_connect(G_OBJECT(dd.cmykactive),"toggled",G_CALLBACK(checkbox_changed),&dd);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.cmykactive,0,1,1,2);
	gtk_widget_show(dd.cmykactive);

	dd.cmykprof=profileselector_new(&pm,IS_TYPE_CMYK,false);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.cmykprof,1,2,1,2);
	gtk_widget_show(dd.cmykprof);
	profileselector_set_filename(PROFILESELECTOR(dd.cmykprof),pm.FindString("DefaultCMYKProfile"));


	dd.monactive=gtk_check_button_new_with_label(_("Monitor Profile:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd.monactive),pm.FindInt("MonitorProfileActive"));
	g_signal_connect(G_OBJECT(dd.monactive),"toggled",G_CALLBACK(checkbox_changed),&dd);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.monactive,0,1,2,3);
	gtk_widget_show(dd.monactive);

	dd.monprof=profileselector_new(&pm,IS_TYPE_RGB,false);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.monprof,1,2,2,3);
	gtk_widget_show(dd.monprof);
	profileselector_set_filename(PROFILESELECTOR(dd.monprof),pm.FindString("MonitorProfile"));


	dd.outputactive=gtk_check_button_new_with_label(_("Default Output Profile:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd.outputactive),pm.FindInt("PrinterProfileActive"));
	g_signal_connect(G_OBJECT(dd.outputactive),"toggled",G_CALLBACK(checkbox_changed),&dd);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.outputactive,0,1,3,4);
	gtk_widget_show(dd.outputactive);

	dd.outputprof=profileselector_new(&pm,IS_TYPE_NULL,false);
	gtk_table_attach_defaults(GTK_TABLE(table),dd.outputprof,1,2,3,4);
	gtk_widget_show(dd.outputprof);
	profileselector_set_filename(PROFILESELECTOR(dd.outputprof),pm.FindString("PrinterProfile"));


	// Paths page:

	label=gtk_label_new(_("Paths"));
	gtk_widget_show(label);

	pathspage=gtk_hbox_new(FALSE,8);
	gtk_widget_show(GTK_WIDGET(pathspage));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),GTK_WIDGET(pathspage),GTK_WIDGET(label));

	vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(pathspage),vbox,TRUE,TRUE,8);
	gtk_widget_show(vbox);

	label=gtk_label_new(_("ICC colour profile paths"));
	gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
	gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,8);
	gtk_widget_show(label);

	dd.patheditor = patheditor_new(&pm);
	g_signal_connect(dd.patheditor,"changed",G_CALLBACK(paths_changed),&dd);
	gtk_box_pack_start(GTK_BOX(vbox),dd.patheditor,TRUE,TRUE,0);
	gtk_widget_show(dd.patheditor);


#if 0
	// Path to Argyll commands

	table=gtk_table_new(2,2,FALSE);
	gtk_table_set_col_spacing(GTK_TABLE(table),0,10);
	gtk_box_pack_start(GTK_BOX(vbox),table,FALSE,FALSE,16);
	gtk_widget_show(table);

	GtkAttachOptions gao = (GtkAttachOptions)(GTK_EXPAND|GTK_FILL);	
	
	label=gtk_label_new(_("Path to Argyll's 'tiffgamut' command"));
	gtk_table_attach(GTK_TABLE(table),label,0,1,0,1,GTK_SHRINK,gao,0,0);
	gtk_widget_show(label);

	argyllpath_tg=gtk_file_chooser_button_new(_("Path to Argyll utilities (TIFFGamut and ICCLink)"),GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_table_attach(GTK_TABLE(table),argyllpath_tg,1,2,0,1,gao,gao,0,0);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(argyllpath_tg),gui.settings.FindString("ArgyllPath_TIFFGamut"));
	gtk_widget_show(argyllpath_tg);

	label=gtk_label_new(_("Path to Argyll's 'collink' command"));
	gtk_table_attach(GTK_TABLE(table),label,0,1,1,2,GTK_SHRINK,gao,0,0);
	gtk_widget_show(label);

	argyllpath_il=gtk_file_chooser_button_new(_("Path to Argyll's collink command"),GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_table_attach(GTK_TABLE(table),argyllpath_il,1,2,1,2,gao,gao,0,0);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(argyllpath_il),gui.settings.FindString("ArgyllPath_ICCLink"));
	gtk_widget_show(argyllpath_il);
#endif

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
				pm.SetString("DefaultRGBProfile",profileselector_get_filename(PROFILESELECTOR(dd.rgbprof)));
				pm.SetString("DefaultCMYKProfile",profileselector_get_filename(PROFILESELECTOR(dd.cmykprof)));
				pm.SetString("MonitorProfile",profileselector_get_filename(PROFILESELECTOR(dd.monprof)));
				pm.SetString("PrinterProfile",profileselector_get_filename(PROFILESELECTOR(dd.outputprof)));
				pm.SetInt("DefaultRGBProfileActive",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd.rgbactive)));
				pm.SetInt("DefaultCMYKProfileActive",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd.cmykactive)));
				pm.SetInt("MonitorProfileActive",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd.monactive)));
				pm.SetInt("PrinterProfileActive",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd.outputactive)));
				char *tmppaths=pm.GetPaths();
				pm.SetString("ProfilePath",tmppaths);
				free(tmppaths);
//				pm.settings.SetString("ArgyllPath_TIFFGamut",gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(argyllpath_tg)));
//				pm.settings.SetString("ArgyllPath_ICCLink",gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(argyllpath_il)));
//				if(result==RESPONSE_SAVE)
//				{
//					pm.SaveGeometry();
//					pm.SaveConfigFile();
//				}
				break;
		}
	}
	gtk_widget_destroy(dialog);

	if(savedpaths)
		free(savedpaths);
}

