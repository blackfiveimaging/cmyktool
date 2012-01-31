#include <gtk/gtk.h>

#include "config.h"

#include "simplelistview.h"
#include "debug.h"

#include "devicelink.h"
#include "devicelinkdialog.h"

#include "profileselector.h"
#include "intentselector.h"
#include "simplecombo.h"
#include "generaldialogs.h"
#include "errordialogqueue.h"

// FIXME - these are being created separately in each module that uses them
#include "gfx/hourglass.cpp"
#include "gfx/rgb.cpp"
#include "gfx/cmyk.cpp"

#include "imagesource_gdkpixbuf.h"
#include "imagesource_montage.h"
#include "pixbuf_from_imagesource.h"

#include "pixbuf_from_imagedata.h"

#include "viewingcondselector.h"
#include "blackgenselector.h"

class DeviceLinkJob : public Job
{
	public:
	DeviceLinkJob(CMYKTool_Core &core,DeviceLink *dl) : Job(_("Generating Device Link")),core(core), dl(dl)
	{
	}
	virtual ~DeviceLinkJob()
	{
		if(dl)
			delete dl;
	}
	virtual void Run(Worker *worker)
	{
		try
		{
			dl->CreateDeviceLink(core.FindString("ArgyllPath"),core.GetProfileManager());
		}
		catch (const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
		ThreadEvent *ev=core.FindEvent("UpdateUI");
		if(ev)
			ev->Trigger();
	}
	protected:
	CMYKTool_Core &core;
	DeviceLink *dl;
};


class DeviceLinkDialog : public ThreadFunction, public Thread
{
	public:
	DeviceLinkDialog(CMYKTool_Core &core,GtkWidget *parent) : ThreadFunction(), Thread(this), core(core), opts(core.GetOptions()), parent(parent)
	{

		// Create pixbufs from icons in gfx/*.cpp
		hourglass=PixbufFromImageData(hourglass_data,sizeof(hourglass_data));
		rgbpb=PixbufFromImageData(rgb_data,sizeof(rgb_data));
		cmykpb=PixbufFromImageData(cmyk_data,sizeof(cmyk_data));

		window=gtk_dialog_new_with_buttons(_("Device Link Editor"),
			GTK_WINDOW(parent),GtkDialogFlags(0),
			GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
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


		tmp=gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,4);
		gtk_widget_show(tmp);


		exportbutton=gtk_button_new_with_label(_("Export..."));
		gtk_box_pack_start(GTK_BOX(vbox),exportbutton,FALSE,FALSE,0);
		g_signal_connect(exportbutton,"clicked",G_CALLBACK(export_devicelink),this);
		gtk_widget_show(exportbutton);


		tmp=gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,4);
		gtk_widget_show(tmp);


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


		label=gtk_label_new(_("Profile:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		inprofile=profileselector_new(&opts.profilemanager);
		g_signal_connect(G_OBJECT(inprofile),"changed",G_CALLBACK(markdirty),this);
		gtk_table_attach_defaults(GTK_TABLE(table),inprofile,2,5,row,row+1);

		++row;

// Viewing conditions

		label=gtk_label_new(_("Viewing Conditions:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		inputvc=viewingcondselector_new();
		g_signal_connect(G_OBJECT(inputvc),"changed",G_CALLBACK(markdirty),this);
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

		label=gtk_label_new(_("Viewing Conditions:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		outputvc=viewingcondselector_new();
		g_signal_connect(G_OBJECT(outputvc),"changed",G_CALLBACK(markdirty),this);
		gtk_table_attach_defaults(GTK_TABLE(table),outputvc,2,5,row,row+1);

		++row;
		++row;


// Link options

		label=gtk_label_new(_("Link options:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);


		++row;


// Rendering Intent

		label=gtk_label_new(_("Rendering Intent:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		intent=intentselector_new(&opts.profilemanager);
		g_signal_connect(G_OBJECT(intent),"changed",G_CALLBACK(markdirty),this);
		gtk_table_attach_defaults(GTK_TABLE(table),intent,2,5,row,row+1);

		++row;

		blackgen=blackgenselector_new();
		g_signal_connect(G_OBJECT(blackgen),"changed",G_CALLBACK(markdirty),this);
		gtk_table_attach_defaults(GTK_TABLE(table),blackgen,0,5,row,row+1);

		++row;


		// Ink limit

		label=gtk_label_new(_("Ink Limit:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		inklimit=gtk_spin_button_new_with_range(200,400,10);
		g_signal_connect(G_OBJECT(inklimit),"value-changed",G_CALLBACK(markdirty),this);
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
		g_signal_connect(G_OBJECT(quality),"changed",G_CALLBACK(markdirty),this);
		gtk_table_attach_defaults(GTK_TABLE(table),quality,4,5,row,row+1);

		++row;

		label=gtk_label_new(_("Description:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);

		description=gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table),description,2,4,row,row+1);


		tmp=gtk_button_new_with_label(_("Build Device Link"));
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

		Start();
	}
	~DeviceLinkDialog()
	{
		// Cancel monitoring thread
		Stop();
		ThreadEvent *ev=core.FindEvent("UpdateUI");
		if(ev)
			ev->Trigger();

		gtk_widget_destroy(window);

		if(hourglass)
			g_object_unref(G_OBJECT(hourglass));
		if(rgbpb)
			g_object_unref(G_OBJECT(rgbpb));
		if(cmykpb)
			g_object_unref(G_OBJECT(cmykpb));
	}

	// Thread function - hangs around waiting for signal from ThreadEvent, and triggers a list update when received...
	virtual int Entry(Thread &t)
	{
		ThreadEvent *ev=core.FindEvent("UpdateUI");
		while(!TestBreak())
		{
			ev->WaitEvent();
			if(!TestBreak())
				g_timeout_add(1,updatefunc,this);
		}
		return(0);
	}
	// Function to trigger a list rebuild.  Called on the main thread's context.
	static gboolean updatefunc(gpointer ud)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)ud;
		dlg->buildlist();
		return(FALSE);
	}

	std::string Run()	// Returns the filename of the currently-selected devicelink
	{					// unless the dialog was cancelled, in which case an empty string is returned.
		bool done=false;
		std::string selected;
		while(!done)
		{
			gint result=gtk_dialog_run(GTK_DIALOG(window));
			switch(result)
			{
				case GTK_RESPONSE_OK:
					if(dirty)
					{
						if(Query_Dialog(_("You have changed device link parameters but not generated the actual device link profile.  OK to leave the device link editor?"),window))
							done=true;
					}
					else
						done=true;
					if(done)
					{
						SimpleListViewOption *opt=simplelistview_get(SIMPLELISTVIEW(devicelinklist));
						if(opt && opt->key)
							selected=opt->key;
					}
					break;
				case GTK_RESPONSE_CANCEL:
				case GTK_RESPONSE_DELETE_EVENT:
					done=true;
					break;
			}
		}
		return(selected);
	}
	protected:
	static void markdirty(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		dlg->dirty=true;
		Debug[WARN] << "Marking devicelink dialog as dirty" << endl;
	}
	static void build_devicelink(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		try
		{
			// Compare DisplayName against DeviceLink list and prompt user if the same as an existing one.

			const char *desc=gtk_entry_get_text(GTK_ENTRY(dlg->description));
			if(!desc || strlen(desc)==0)
				throw ("Please enter a descriptive name for the device link");

			DeviceLink *dl=new DeviceLink;
			dlg->dialog_to_devicelink(*dl);

			if(!dl->CheckArgyllPath(dlg->core.FindString("ArgyllPath")))
			{
				delete dl;
				throw _("Can't find collink - please check Argyll path");
			}

			DeviceLinkList dll;
			for(unsigned int idx=0;idx<dll.size();++idx)
			{
				std::string dname=dll[idx].displayname;
				if(dname.compare(desc)==0)
				{
					if(Query_Dialog(_("Are you sure you want to overwrite an existing device link?"),dlg->window))
					{
						// Provide the DeviceLink_Entry in lieu of filename.
						dl->Save(dll[idx]);
					}
					else
						return; // bail out since the user declined.
				}
			}

			dl->Save();	// Save metadata before generating the file...
			dlg->core.GetDispatcher().AddJob(new DeviceLinkJob(dlg->core,dl));
			dlg->dirty=false;

			dlg->buildlist();
		}
		catch (const char *err)
		{
			ErrorMessage_Dialog(err,dlg->window);
		}
	}

	void dialog_to_devicelink(DeviceLink &dl)
	{
		dl.SetBlackGen(blackgenselector_get(BLACKGENSELECTOR(blackgen)));
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
		blackgenselector_set(BLACKGENSELECTOR(blackgen),dl.GetBlackGen());
		gtk_entry_set_text(GTK_ENTRY(description),dl.FindString("Description"));
		profileselector_set_filename(PROFILESELECTOR(inprofile),dl.FindString("SourceProfile"));
		viewingcondselector_set(VIEWINGCONDSELECTOR(inputvc),dl.FindString("SourceViewingConditions"));
		profileselector_set_filename(PROFILESELECTOR(outprofile),dl.FindString("DestProfile"));
		viewingcondselector_set(VIEWINGCONDSELECTOR(outputvc),dl.FindString("DestViewingConditions"));
		intentselector_setintent(INTENTSELECTOR(intent),LCMSWrapper_Intent(dl.FindInt("RenderingIntent")));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(inklimit),dl.FindInt("InkLimit"));
		simplecombo_set_index(SIMPLECOMBO(quality),dl.FindInt("Quality"));
		outprofile_changed(outprofile,this);
		gtk_widget_set_sensitive(exportbutton,dl.DeviceLinkBuilt());
		dirty=false;
	}

	void set_defaults()
	{
		Argyll_BlackGenerationCurve curve;
		blackgenselector_set(BLACKGENSELECTOR(blackgen),curve);
		gtk_entry_set_text(GTK_ENTRY(description),_("New devicelink"));
		profileselector_set_filename(PROFILESELECTOR(inprofile),opts.GetInRGBProfile());
		viewingcondselector_set(VIEWINGCONDSELECTOR(inputvc),"");
		profileselector_set_filename(PROFILESELECTOR(outprofile),opts.GetOutProfile());
		viewingcondselector_set(VIEWINGCONDSELECTOR(outputvc),"");
		intentselector_setintent(INTENTSELECTOR(intent),opts.GetIntent());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(inklimit),270);
		simplecombo_set_index(SIMPLECOMBO(quality),DEVICELINK_QUALITY_MEDIUM);
		outprofile_changed(outprofile,this);
		gtk_widget_set_sensitive(exportbutton,false);
		dirty=false;
	}

	void buildlist()
	{
		// First, get the number of jobs queued and in progress...
		JobDispatcher &disp=core.GetDispatcher();
		disp.ObtainMutex();
		int jobcount=disp.JobCount(JOBSTATUS_QUEUED|JOBSTATUS_RUNNING);
		disp.ReleaseMutex();

		DeviceLinkList list;
		SimpleListViewOptions lvo;
		lvo.Add(NULL,_("New Devicelink"));
		for(unsigned int idx=0;idx<list.size();++idx)
		{
			DeviceLinkList_Entry &e=list[idx];
			std::string dn;
			if(e.pending)
			{
				dn="("+e.displayname+")";
				if(jobcount)
					lvo.Add(e.filename.c_str(),dn.c_str(),NULL,hourglass);
				else	// If there are no jobs pending, but the DL is marked pending something went wrong...
				{
					GtkWidget *image = gtk_image_new ();
					GdkPixbuf *pixbuf = gtk_widget_render_icon(image, GTK_STOCK_NO, GTK_ICON_SIZE_MENU, NULL); 
					lvo.Add(e.filename.c_str(),dn.c_str(),NULL,pixbuf);	// FIXME - use stock image somehow...
					gtk_widget_destroy(image);
				}
			}
			else
			{
				// FIXME - pick an RGB / CMYK icon here...
				GdkPixbuf *icon=NULL;
				try
				{
					ProfileManager &pm=core.GetProfileManager();
					DeviceLink dl(e.filename.c_str());

					IS_TYPE intype=IS_TYPE_NULL;
					IS_TYPE outtype=IS_TYPE_NULL;

					CMSProfile *in=pm.GetProfile(dl.FindString("SourceProfile"));
					if(in)
					{
						intype=in->GetColourSpace();
						delete in;
					}

					CMSProfile *out=pm.GetProfile(dl.FindString("DestProfile"));
					if(out)
					{
						outtype=out->GetColourSpace();
						delete out;
					}

					GdkPixbuf *inicon=NULL;
					GdkPixbuf *outicon=NULL;
					switch(intype)
					{
						case IS_TYPE_RGB:
							inicon=rgbpb;
							break;
						case IS_TYPE_CMYK:
							inicon=cmykpb;
							break;
						default:
							break;
					}
					switch(outtype)
					{
						case IS_TYPE_RGB:
							outicon=rgbpb;
							break;
						case IS_TYPE_CMYK:
							outicon=cmykpb;
							break;
						default:
							break;
					}
					ImageSource_Montage *mon=new ImageSource_Montage(IS_TYPE_RGB);
					if(outicon)
					{
						ImageSource *emblem=new ImageSource_GdkPixbuf(outicon);
						mon->Add(emblem,36-emblem->width,30-emblem->height);
					}
					if(inicon)
					{
						ImageSource *emblem=new ImageSource_GdkPixbuf(inicon);
						mon->Add(emblem,0,0);
					}
					if(mon->width)
						icon=pixbuf_from_imagesource(mon);
				}
				catch(const char *err)
				{
					Debug[WARN] << "Couldn't create icon for devicelink " << e.filename << endl;
				}
				dn=e.displayname;
				lvo.Add(e.filename.c_str(),dn.c_str(),NULL,icon);
			}
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
		dlg->dirty=true;
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
		dlg->dirty=false;
	}


	static void export_devicelink(GtkWidget *wid,gpointer userdata)
	{
		DeviceLinkDialog *dlg=(DeviceLinkDialog *)userdata;
		SimpleListViewOption *opt=simplelistview_get(SIMPLELISTVIEW(dlg->devicelinklist));
		char *fn;
		if(opt && (fn=opt->key))
		{
			if(dlg->dirty && Query_Dialog(_("Warning - you have modified the devicelink settings,"
				"but not yet rebuild the devicelink.  The existing devicelink will be exported - is this what you want?"),dlg->window));
			DeviceLink dl(fn);
			char *outfn=File_Save_Dialog(_("Please choose a filename for the DeviceLink"),dl.FindString("Description"),dlg->window);
			if(outfn)
			{
				dl.Export(outfn);
				free(outfn);
			}
		}
	}

	CMYKTool_Core &core;
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
	GtkWidget *exportbutton;

	// Icons for the listview
	GdkPixbuf *hourglass;
	GdkPixbuf *rgbpb;
	GdkPixbuf *cmykpb;
	
	GtkWidget *parent;
	GtkWidget *window;

	bool dirty;
};


std::string DeviceLink_Dialog(CMYKTool_Core &core,GtkWidget *parent)
{
	DeviceLinkDialog dlg(core,parent);
	std::string result=dlg.Run();
	return(result);
}

