#include <gtk/gtk.h>

#include "simplelistview.h"
#include "debug.h"

#include "devicelink.h"
#include "devicelinkdialog.h"

#include "profileselector.h"
#include "intentselector.h"
#include "simplecombo.h"

#include "argyllsupport/viewingcondselector.h"
#include "argyllsupport/blackgenselector.h"

class DeviceLinkDialog
{
	public:
	DeviceLinkDialog(CMYKConversionOptions &opts,GtkWidget *parent) : opts(opts), parent(parent)
	{
		window=gtk_dialog_new_with_buttons(_("DeviceLink manager"),
			GTK_WINDOW(parent),GtkDialogFlags(0),
			GTK_STOCK_OK,GTK_RESPONSE_OK,
			NULL);

		gtk_window_set_default_size(GTK_WINDOW(window),800,450);

		GtkWidget *vbox = GTK_DIALOG(window)->vbox;

		GtkWidget *hbox = gtk_hbox_new(FALSE,8);
		gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,8);
		gtk_widget_show(hbox);

		vbox=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(hbox),vbox,FALSE,FALSE,8);
		gtk_widget_show(vbox);

		devicelinklist=simplelistview_new();
		gtk_box_pack_start(GTK_BOX(vbox),devicelinklist,TRUE,TRUE,0);
		gtk_widget_show(devicelinklist);


		GtkWidget *label;
		label=gtk_button_new_with_label(_("Delete"));
		gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
		g_signal_connect(label,"clicked",G_CALLBACK(delete_devicelink),this);
		gtk_widget_show(label);

		buildlist();


		vbox=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,8);
		gtk_widget_show(vbox);

		GtkWidget *table=gtk_table_new(2,5,FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table),4);
		gtk_table_set_col_spacings(GTK_TABLE(table),4);
		gtk_box_pack_start(GTK_BOX(vbox),table,TRUE,TRUE,8);

		int row=0;


// Source Profile

		label=gtk_label_new(_("Input options:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);


		++row;


		label=gtk_label_new(_("Profile"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		inprofile=profileselector_new(&opts.profilemanager);
		gtk_table_attach_defaults(GTK_TABLE(table),inprofile,2,5,row,row+1);

		++row;

// Viewing conditions

		label=gtk_label_new(_("Viewing Conditions"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		inputvc=viewingcondselector_new();
		gtk_table_attach_defaults(GTK_TABLE(table),inputvc,2,5,row,row+1);

		++row;
		++row;


// Destination Profile

		label=gtk_label_new(_("Output options:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);


		++row;


		label=gtk_label_new(_("Profile:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		outprofile=profileselector_new(&opts.profilemanager);
		gtk_table_attach_defaults(GTK_TABLE(table),outprofile,2,5,row,row+1);

		++row;

// Viewing conditions

		label=gtk_label_new(_("Viewing Conditions"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		outputvc=viewingcondselector_new();
		gtk_table_attach_defaults(GTK_TABLE(table),outputvc,2,5,row,row+1);

		++row;
		++row;


// Link options

		label=gtk_label_new(_("Link options:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);


		++row;


// Rendering Intent

		label=gtk_label_new(_("Rendering Intent"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		intent=intentselector_new(&opts.profilemanager);
		gtk_table_attach_defaults(GTK_TABLE(table),intent,2,5,row,row+1);

		++row;

		blackgen=blackgenselector_new();
		gtk_table_attach_defaults(GTK_TABLE(table),blackgen,0,5,row,row+1);

		++row;


		// Ink limit

		label=gtk_label_new(_("Ink Limit:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		inklimit=gtk_spin_button_new_with_range(200,400,10);
		gtk_table_attach_defaults(GTK_TABLE(table),inklimit,2,3,row,row+1);


		// Quality

		label=gtk_label_new(_("Quality:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,3,4,row,row+1);

		SimpleComboOptions comboopts;
		comboopts.Add(NULL,_("Low"),_("Fastest mode at the expense of quality"));
		comboopts.Add(NULL,_("Medium"),_("Balances speed and quality"));
		comboopts.Add(NULL,_("High"),_("Highest quality at the expense of speed"));

		quality=simplecombo_new(comboopts);
		gtk_table_attach_defaults(GTK_TABLE(table),quality,4,5,row,row+1);

		++row;

		label=gtk_label_new(_("Description:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		description=gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table),description,2,5,row,row+1);
		

		++row;


		GtkWidget *tmp=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(vbox),tmp,TRUE,TRUE,0);
		gtk_widget_show(tmp);


		gtk_widget_show_all(table);

		g_signal_connect(devicelinklist,"changed",G_CALLBACK(devicelink_changed),this);

		gtk_widget_show(window);
	}
	~DeviceLinkDialog()
	{
		gtk_widget_destroy(window);
	}
	void Run()
	{
		gint result=gtk_dialog_run(GTK_DIALOG(window));
		switch(result)
		{
			default:
				break;
		}
	}
	protected:
	void buildlist()
	{
		DeviceLinkList list;
		SimpleListViewOptions lvo;
		for(unsigned int idx=0;idx<list.size();++idx)
		{
			DeviceLinkList_Entry &e=list[idx];
			lvo.Add(e.filename.c_str(),e.displayname.c_str());
		}
		simplelistview_set_opts(SIMPLELISTVIEW(devicelinklist),&lvo);
	}
	static void devicelink_changed(GtkWidget *wid,gpointer userdata)
	{
	}
	static void delete_devicelink(GtkWidget *wid,gpointer userdata)
	{
	}
	CMYKConversionOptions &opts;
	GtkWidget *devicelinklist;
	GtkWidget *inprofile;
	GtkWidget *inputvc;
	GtkWidget *outprofile;
	GtkWidget *outputvc;
	GtkWidget *intent;
	GtkWidget *blackgen;
	GtkWidget *inklimit;
	GtkWidget *quality;
	GtkWidget *description;
	
	GtkWidget *parent;
	GtkWidget *window;
};


void DeviceLink_Dialog(CMYKConversionOptions &opts,GtkWidget *parent)
{
	DeviceLinkDialog dlg(opts,parent);
	dlg.Run();
}

