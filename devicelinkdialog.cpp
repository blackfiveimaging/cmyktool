#include <gtk/gtk.h>

#include "simplelistview.h"
#include "debug.h"

#include "devicelink.h"
#include "devicelinkdialog.h"

#include "profileselector.h"
#include "intentselector.h"
#include "simplecombo.h"
#include "generaldialogs.h"

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

		GtkWidget *tmp;
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
		g_signal_connect(G_OBJECT(outprofile),"changed",G_CALLBACK(outprofile_changed),this);
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
		gtk_table_attach_defaults(GTK_TABLE(table),description,2,4,row,row+1);


		tmp=gtk_button_new_with_label(_("Build Devicelink"));
		g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(build_devicelink),this);
		gtk_table_attach_defaults(GTK_TABLE(table),tmp,4,5,row,row+1);

		++row;


		tmp=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(vbox),tmp,TRUE,TRUE,0);
		gtk_widget_show(tmp);


		gtk_widget_show_all(table);

		g_signal_connect(devicelinklist,"changed",G_CALLBACK(devicelink_changed),this);

		set_defaults();

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
	static void build_devicelink(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		try
		{
			DeviceLink dl;
			dlg->dialog_to_devicelink(dl);
			dl.CreateDeviceLink(dlg->opts.profilemanager);
			dlg->buildlist();
		}
		catch (const char *err)
		{
			ErrorMessage_Dialog(err,dlg->window);
		}
	}

	void dialog_to_devicelink(DeviceLink &dl)
	{
		dl.blackgen=blackgenselector_get(BLACKGENSELECTOR(blackgen));
		dl.SetString("Description",gtk_entry_get_text(GTK_ENTRY(description)));
		dl.SetString("SourceProfile",profileselector_get_filename(PROFILESELECTOR(inprofile)));
		dl.SetString("SourceViewingConditions",viewingcondselector_get(VIEWINGCONDSELECTOR(inputvc)));
		dl.SetString("DestProfile",profileselector_get_filename(PROFILESELECTOR(outprofile)));
		dl.SetString("DestViewingConditions",viewingcondselector_get(VIEWINGCONDSELECTOR(outputvc)));
		dl.SetInt("RenderingIntent",intentselector_getintent(INTENTSELECTOR(intent)));
		dl.SetInt("InkLimit",gtk_spin_button_get_value(GTK_SPIN_BUTTON(inklimit)));
		dl.SetInt("Quality",simplecombo_get_index(SIMPLECOMBO(quality)));
	}

	void devicelink_to_dialog(DeviceLink &dl)
	{
		blackgenselector_set(BLACKGENSELECTOR(blackgen),dl.blackgen);
		gtk_entry_set_text(GTK_ENTRY(description),dl.FindString("Description"));
		profileselector_set_filename(PROFILESELECTOR(inprofile),dl.FindString("SourceProfile"));
		viewingcondselector_set(VIEWINGCONDSELECTOR(inputvc),dl.FindString("SourceViewingConditions"));
		profileselector_set_filename(PROFILESELECTOR(outprofile),dl.FindString("DestProfile"));
		viewingcondselector_set(VIEWINGCONDSELECTOR(outputvc),dl.FindString("DestViewingConditions"));
		intentselector_setintent(INTENTSELECTOR(intent),LCMSWrapper_Intent(dl.FindInt("RenderingIntent")));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(inklimit),dl.FindInt("InkLimit"));
		simplecombo_set_index(SIMPLECOMBO(quality),dl.FindInt("Quality"));
		outprofile_changed(outprofile,this);
	}

	void set_defaults()
	{
		Argyll_BlackGenerationCurve curve;
		blackgenselector_set(BLACKGENSELECTOR(blackgen),curve);
		gtk_entry_set_text(GTK_ENTRY(description),_("New devicelink"));
		profileselector_set_filename(PROFILESELECTOR(inprofile),opts.GetInRGBProfile());
		viewingcondselector_set(VIEWINGCONDSELECTOR(inputvc),"mt");
		profileselector_set_filename(PROFILESELECTOR(outprofile),opts.GetOutProfile());
		viewingcondselector_set(VIEWINGCONDSELECTOR(outputvc),"pp");
		intentselector_setintent(INTENTSELECTOR(intent),opts.GetIntent());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(inklimit),270);
		simplecombo_set_index(SIMPLECOMBO(quality),DEVICELINK_QUALITY_MEDIUM);
		outprofile_changed(outprofile,this);
	}

	void buildlist()
	{
		DeviceLinkList list;
		SimpleListViewOptions lvo;
		lvo.Add(NULL,_("New Devicelink"));
		for(unsigned int idx=0;idx<list.size();++idx)
		{
			DeviceLinkList_Entry &e=list[idx];
			lvo.Add(e.filename.c_str(),e.displayname.c_str());
		}
		simplelistview_set_opts(SIMPLELISTVIEW(devicelinklist),&lvo);
	}

	static void devicelink_changed(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		SimpleListViewOption *opt=simplelistview_get(SIMPLELISTVIEW(dlg->devicelinklist));
		char *fn;
		if(opt && (fn=opt->key))
		{
			DeviceLink dl(fn);
			dlg->devicelink_to_dialog(dl);
		}
		else
		{
			dlg->set_defaults();
		}
	}

	static void outprofile_changed(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		try
		{
			// Neither black generation and ink limiting are applicable when the target is RGB,
			// so we grey out those widgets if the output space is RGB.
			CMSProfile *p=dlg->opts.profilemanager.GetProfile(profileselector_get_filename(PROFILESELECTOR(dlg->outprofile)));
			if(p)
			{
				gtk_widget_set_sensitive(dlg->blackgen,p->GetColourSpace()==IS_TYPE_CMYK);
				gtk_widget_set_sensitive(dlg->inklimit,p->GetColourSpace()==IS_TYPE_CMYK);
			}
		}
		catch (const char *err)
		{
			Debug[ERROR] << "Error: " << err << std::endl;
		}
	}

	static void delete_devicelink(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		SimpleListViewOption *opt=simplelistview_get(SIMPLELISTVIEW(dlg->devicelinklist));
		char *fn;
		if(opt && (fn=opt->key) && Query_Dialog(_("Are you sure you want to delete this devicelink profile?"),dlg->window))
		{
			DeviceLink dl(fn);
			dl.Delete();
			dlg->buildlist();
		}
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

