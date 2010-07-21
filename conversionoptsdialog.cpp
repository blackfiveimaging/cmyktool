#include <iostream>

#include <gtk/gtk.h>

#include "support/debug.h"
#include "support/util.h"

#include "miscwidgets/simplecombo.h"
#include "miscwidgets/simplelistview.h"
#include "miscwidgets/generaldialogs.h"

#include "profilemanager/profilemanager.h"
#include "profilemanager/profileselector.h"
#include "profilemanager/intentselector.h"

#include "conversionoptsdialog.h"
#include "devicelink.h"
#include "devicelinkdialog.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;

#define RESPONSE_SAVE 1


#define PRESET_MAXCHARS 17


class CMYKConversionOptsDialog
{
	public:
	CMYKConversionOptsDialog(CMYKTool_Core &core, GtkWidget *parent) : core(core), opts(core.GetOptions()), backupopts(opts), blockupdates(false)
	{
		window=gtk_dialog_new_with_buttons(_("Colour conversion options"),
			GTK_WINDOW(parent),GtkDialogFlags(0),
			GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK,GTK_RESPONSE_OK,
			NULL);

		GtkWidget *vbox = GTK_DIALOG(window)->vbox;

		GtkWidget *hbox = gtk_hbox_new(FALSE,8);
		gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,8);
		gtk_widget_show(hbox);

		vbox=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,8);
		gtk_widget_show(vbox);

		listview=simplelistview_new(NULL);
		gtk_box_pack_start(GTK_BOX(vbox),listview,TRUE,TRUE,8);
		gtk_widget_show(listview);

		build_presetlist();

		// FIXME - preselect the current preset in the listview...
		g_signal_connect(listview,"changed",G_CALLBACK(preset_changed),this);


		// Create Delete button

		GtkWidget *label;
		label=gtk_button_new_with_label(_("Delete"));
		gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
		g_signal_connect(label,"clicked",G_CALLBACK(delete_preset),this);
		gtk_widget_show(label);

		// Now a vbox to contain the table

		vbox=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,8);
		gtk_widget_show(vbox);

		GtkWidget *table=gtk_table_new(5,4,FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table),6);
		gtk_table_set_col_spacings(GTK_TABLE(table),6);
		gtk_box_pack_start(GTK_BOX(vbox),table,FALSE,FALSE,8);
		gtk_widget_show(table);

		int row=0;

		label=gtk_button_new_with_label(_("<- Save"));
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);
		g_signal_connect(label,"clicked",G_CALLBACK(save_preset),this);
		gtk_widget_show(label);

		label=gtk_label_new(_("Name:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,1,2,row,row+1);
		gtk_widget_show(label);

		description=gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table),description,2,5,row,row+1);
		gtk_widget_show(description);


		++row;


		label=gtk_label_new(_("Input:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);
		gtk_widget_show(label);


		++row;


		// Input profile

		label=gtk_label_new(_("Fallback RGB Profile:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);
		gtk_widget_show(label);

		irps = profileselector_new(&opts.profilemanager,IS_TYPE_RGB);
		g_signal_connect(irps,"changed",G_CALLBACK(inprofile_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),irps,2,5,row,row+1);
		gtk_widget_show(irps);


		++row;


		// Input CMYK profile

		label=gtk_label_new(_("Fallback CMYK Profile:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);
		gtk_widget_show(label);

		icps = profileselector_new(&opts.profilemanager,IS_TYPE_CMYK,true,true);
		g_signal_connect(icps,"changed",G_CALLBACK(inprofile_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),icps,2,5,row,row+1);
		gtk_widget_show(icps);


		++row;


		// Ignore embedded profile button

		ignore = gtk_check_button_new_with_label(_("Ignore embedded profiles"));
		g_signal_connect(ignore,"toggled",G_CALLBACK(ignoreembedded_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),ignore,2,5,row,row+1);
		gtk_widget_show(ignore);


		++row;

		label=gtk_label_new(_("Output:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);
		gtk_widget_show(label);


		++row;

		// Output profile

		label=gtk_label_new(_("Output Profile:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);
		gtk_widget_show(label);

		ps = profileselector_new(&opts.profilemanager,IS_TYPE_NULL,true);
		g_signal_connect(ps,"changed",G_CALLBACK(profile_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),ps,2,5,row,row+1);
		gtk_widget_show(ps);


		++row;

		// Rendering intent

		label=gtk_label_new(_("Rendering intent:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);
		gtk_widget_show(label);

		is = intentselector_new(&opts.profilemanager);
		g_signal_connect(is,"changed",G_CALLBACK(intent_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),is,2,5,row,row+1);
		gtk_widget_show(is);


		++row;


		label=gtk_label_new(_("Processing:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,row,row+1);
		gtk_widget_show(label);


		++row;

		// DeviceLink settings
		usedevicelink=gtk_check_button_new_with_label(_("Use Device Link:"));
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(usedevicelink),opts.GetUseDeviceLink());
		gtk_table_attach_defaults(GTK_TABLE(table),usedevicelink,0,2,row,row+1);
		gtk_widget_show(usedevicelink);
		
		SimpleComboOptions devlinks;
		devlinks.Add(NULL,_("Other..."),_("Choose or create a Device Link profile..."));
		devicelink=simplecombo_new(devlinks);
		gtk_table_attach_defaults(GTK_TABLE(table),devicelink,2,5,row,row+1);
		gtk_widget_show(devicelink);

		++row;


		// Conversion mode

		label=gtk_label_new(_("Conversion mode:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,row,row+1);
		gtk_widget_show(label);

		SimpleComboOptions scopts;
		scopts.Add("",_("Normal"),_("A regular conversion from source to destination profile"));
		scopts.Add("",_("Hold Black"),_("Maps pure black input to CMYK (0,0,0,100)"));
		scopts.Add("",_("Hold Grey"),_("Maps grey input (R=G=B) to K channel only"));
		scopts.Add("",_("Overprint"),_("Maps pure black to CMYK (0,0,0,100) and copies C, M and Y from the most recent non-black pixel."));

		combo=simplecombo_new(scopts);
		gtk_table_attach_defaults(GTK_TABLE(table),combo,2,3,row,row+1);
		gtk_widget_show(combo);

		for(int i=0;i<int(CMYKCONVERSIONMODE_MAX);++i)
		{
			if(convmodes[i]==opts.GetMode())
				simplecombo_set_index(SIMPLECOMBO(combo),i);
		}
		g_signal_connect(combo,"changed",G_CALLBACK(combo_changed),this);


//		++row;

		// Conversion width

		label=gtk_label_new(_("Transition width:"));
		gtk_misc_set_alignment(GTK_MISC(label),1.0,0.5);
		gtk_table_attach_defaults(GTK_TABLE(table),label,3,4,row,row+1);
		gtk_widget_show(label);

		widthbutton=gtk_spin_button_new_with_range(0.0,32.0,1.0);
		g_signal_connect(widthbutton,"value-changed",G_CALLBACK(width_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),widthbutton,4,5,row,row+1);
		gtk_widget_show(widthbutton);


		// Including the "None" setting in the dialog is too confusing, so we ensure that
		// the mode gets set to something else.
		combo_changed(combo,this);

		gtk_widget_show(window);

		update_dialog();

		g_signal_connect(usedevicelink,"toggled",G_CALLBACK(usedevicelink_changed),this);
		g_signal_connect(devicelink,"changed",G_CALLBACK(devicelink_changed),this);
	}
	~CMYKConversionOptsDialog()
	{
		gtk_widget_destroy(window);
	}
	void Run()
	{
		gint result=gtk_dialog_run(GTK_DIALOG(window));
		if(result==GTK_RESPONSE_CANCEL)
			opts=backupopts;
		blockupdates=true;
	}
	void SetSensitive()
	{
		bool active=opts.GetOutputType()==IS_TYPE_CMYK;

		// FIXME - find a better solution to the problem of GetOutputType returning
		// IS_TYPE_RGB if conversion mode is NONE
		if(opts.GetMode()==CMYKCONVERSIONMODE_NONE)
			active=true;

		gtk_widget_set_sensitive(GTK_WIDGET(combo),active);
	}
	private:
	CMYKTool_Core &core;
	CMYKConversionOptions &opts;
	CMYKConversionOptions backupopts;
	GtkWidget *window;
	GtkWidget *listview;
	GtkWidget *description;
	GtkWidget *irps;
	GtkWidget *icps;
	GtkWidget *ignore;
	GtkWidget *ps;
	GtkWidget *is;
	GtkWidget *combo;
	GtkWidget *widthbutton;
	GtkWidget *usedevicelink;
	GtkWidget *devicelink;
	bool blockupdates;

	void update_dialog()
	{
		if(blockupdates)
			return;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ignore),opts.GetIgnoreEmbedded());

		intentselector_setintent(INTENTSELECTOR(is),opts.GetIntent());

		profileselector_set_filename(PROFILESELECTOR(irps),opts.GetInRGBProfile());
		profileselector_set_filename(PROFILESELECTOR(icps),opts.GetInCMYKProfile());
		profileselector_set_filename(PROFILESELECTOR(ps),opts.GetOutProfile());

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usedevicelink),opts.GetUseDeviceLink());
		simplecombo_set(SIMPLECOMBO(devicelink),opts.GetDeviceLink());
		SetSensitive();

		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widthbutton),opts.GetWidth());
	}

	void build_presetlist()
	{
		// Build a set of SimpleListViewOptions for the presets.
		// Returns the index of the special "Previous" item, if found.
		blockupdates=true;
		SimpleListViewOptions listopts;
		listopts.Add(NULL,_("New preset"),_("New preset"));
		PresetList list;
		for(unsigned int idx=0;idx<list.size();++idx)
		{
			const char *fn=list[idx].filename.c_str();
			const char *dn=list[idx].displayname.c_str();

			if(strlen(dn)<=PRESET_MAXCHARS)
				listopts.Add(fn,dn,dn);
			else
			{
				char buf[PRESET_MAXCHARS*4+4];
				utf8ncpy(buf,dn,PRESET_MAXCHARS);
				int n=strlen(buf);
				buf[n]=buf[n+1]=buf[n+2]='.';
				buf[n+3]=0;
				listopts.Add(fn,buf,buf);
			}
		}
		simplelistview_set_opts(SIMPLELISTVIEW(listview),&listopts);
		blockupdates=false;
	}


	static void save_preset(GtkWidget *widget,gpointer user_data)
	{
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		try
		{
			PresetList list;

			CMYKConversionPreset p;
			p.Store(dlg->opts);

			const char *txt=gtk_entry_get_text(GTK_ENTRY(dlg->description));
			if(txt && strlen(txt) && strcmp(txt,"unnamed preset")!=0)
			{
				p.SetString("DisplayName",txt);
				Debug[TRACE] << "Stored preset details using name " << txt << endl;
			}
			else
			{
				GtkWidget *savedlg=gtk_dialog_new_with_buttons(_("Choose a name for this preset..."),
					GTK_WINDOW(dlg->window),GtkDialogFlags(0),
					GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK,GTK_RESPONSE_OK,
					NULL);

				GtkWidget *vbox = GTK_DIALOG(savedlg)->vbox;

				GtkWidget *hbox = gtk_hbox_new(FALSE,8);
				gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,8);
				gtk_widget_show(hbox);

				GtkWidget *entry = gtk_entry_new();
				gtk_box_pack_start(GTK_BOX(hbox),entry,TRUE,TRUE,8);
				gtk_widget_show(entry);

				gint result=gtk_dialog_run(GTK_DIALOG(savedlg));
				switch(result)
				{
					case GTK_RESPONSE_OK:
						{
							CMYKConversionPreset p;
							p.Store(dlg->opts);
							const char *desc=gtk_entry_get_text(GTK_ENTRY(entry));
							p.SetString("DisplayName",desc);
							gtk_entry_set_text(GTK_ENTRY(dlg->description),desc);
							p.Save(PRESET_NEW_ESCAPE);
							gtk_widget_destroy(savedlg);
						}
						break;
					default:
						gtk_widget_destroy(savedlg);
						return;
						break;
				}
			}

			txt=p.FindString("DisplayName");
			try
			{
				PresetList_Entry &entry=list[txt];
				Debug[TRACE] << "Successfully got entry for " << txt << endl;
				// If we got here the preset already exists, so save using the existing preset's filename...
				if(Query_Dialog(_("Overwrite existing preset?"),dlg->window))
					p.Save(entry.filename.c_str());
			}
			catch(const char *err)
			{
				Debug[TRACE] << "Unable to fetch entry for " << txt << endl;
				// If we got here, the preset doesn't exist yet - so...
 				p.Save(PRESET_NEW_ESCAPE);
			}
		}
		catch(const char *err)
		{
			ErrorMessage_Dialog(err,dlg->window);
		}
		dlg->build_presetlist();
	}


	static void delete_preset(GtkWidget *widget,gpointer user_data)
	{
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		dlg->blockupdates=true;
		SimpleListViewOption *opt=simplelistview_get(SIMPLELISTVIEW(dlg->listview));
		if(opt->key)
		{
			if(Query_Dialog(_("Are you sure you want to delete this preset?"),dlg->window))
			{
				simplelistview_set_index(SIMPLELISTVIEW(dlg->listview),0);
				remove(opt->key);
				dlg->build_presetlist();
			}
		}
		dlg->blockupdates=false;
	}


	static void preset_changed(GtkWidget *widget,gpointer user_data)
	{
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		if(dlg->blockupdates)
			return;
		SimpleListViewOption *opt=simplelistview_get(SIMPLELISTVIEW(dlg->listview));
		CMYKConversionPreset p;
		if(opt->key)
		{
			Debug[TRACE] << "Got key: " << opt->key << " (" << opt->displayname << ")" << std::endl;
			p.Load(opt->key);
			p.Retrieve(dlg->opts);
			dlg->update_dialog();
		}
		// We update the description field even if there's no preset to load - that way we get the default
		// preset description when the user clicks "New preset", but all other options remain as previously set.
		gtk_entry_set_text(GTK_ENTRY(dlg->description),p.FindString("DisplayName"));
		usedevicelink_changed(widget,user_data);
	}

	static void	profile_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from ProfileSelector" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		ProfileSelector *c=PROFILESELECTOR(dlg->ps);
		const char *val=profileselector_get_filename(c);
		if(val)
		{
			dlg->opts.SetOutProfile(val);
			Debug[TRACE] << "Selected: " << val << endl;
			dlg->SetSensitive();
		}
		else
			Debug[WARN] << "No profile selected... " << endl;

		dlg->updatedevicelinklist();
	}

	static void	inprofile_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from ProfileSelector" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		ProfileSelector *c=PROFILESELECTOR(dlg->irps);
		const char *val=profileselector_get_filename(c);
		if(val)
		{
			dlg->opts.SetInRGBProfile(val);
			Debug[TRACE] << "Selected: " << val << endl;
		}
		else
			Debug[WARN] << "No profile selected... " << endl;

		c=PROFILESELECTOR(dlg->icps);
		val=profileselector_get_filename(c);
		if(val)
		{
			dlg->opts.SetInCMYKProfile(val);
			Debug[TRACE] << "Selected: " << val << endl;
		}
		else
			Debug[WARN] << "No profile selected... " << endl;


		dlg->updatedevicelinklist();
	}

	static void	intent_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from IntentSelector" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		IntentSelector *is=INTENTSELECTOR(dlg->is);

		LCMSWrapper_Intent intent=LCMSWrapper_Intent(intentselector_getintent(is));

		dlg->opts.SetIntent(intent);

		Debug[TRACE] << "Intent " << intent << ": " << dlg->opts.profilemanager.GetIntentName(intent) << endl;
		dlg->updatedevicelinklist();
	}


	static void width_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received value-changed signal from Width widget" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;

		float val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
		dlg->opts.SetWidth(int(val));
	}


	static void	combo_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from combo" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		SimpleCombo *c=SIMPLECOMBO(dlg->combo);

		int idx=simplecombo_get_index(c);

		dlg->opts.SetMode(convmodes[idx]);

		switch(convmodes[idx])
		{
			case CMYKCONVERSIONMODE_HOLDBLACK:
			case CMYKCONVERSIONMODE_HOLDGREY:
				gtk_widget_set_sensitive(dlg->widthbutton,true);
				break;
			default:
				gtk_widget_set_sensitive(dlg->widthbutton,false);
				break;
		}

		Debug[TRACE] << "Conversion mode set to: " << convmodes[idx] << endl;
	}


	static void	devicelink_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from devicelink" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;

		dlg->blockupdates=true;

		SimpleCombo *c=SIMPLECOMBO(dlg->devicelink);

		const char *dl=simplecombo_get(c);
		if(dl)
		{
			Debug[TRACE] << "Setting devicelink to " << dl << endl;
			dlg->opts.SetDeviceLink(dl);
		}
		else
		{
			std::string result=DeviceLink_Dialog(dlg->core,dlg->window);
			if(result.size())
				dlg->opts.SetDeviceLink(result.c_str());
			dlg->updatedevicelinklist();
		}
		dlg->blockupdates=false;
	}


	static void	ignoreembedded_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received toggled signal from check button" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		bool state=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlg->ignore));

		dlg->opts.SetIgnoreEmbedded(state);

		Debug[TRACE] << "Set IgnoreEmbedded to " << state << endl;
	}


	static void usedevicelink_changed(GtkWidget *widget,gpointer user_data)
	{
		try
		{
			Debug[TRACE] << "Received toggled signal from dl check button" << endl;
			CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
			if(dlg->blockupdates)
				return;
			bool state=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlg->usedevicelink));

			dlg->opts.SetUseDeviceLink(state);

			gtk_widget_set_sensitive(dlg->devicelink,state);
			dlg->updatedevicelinklist();
			if(state)
			{
				dlg->devicelink_changed(dlg->devicelink,dlg);
			}
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
		}
	}


	void updatedevicelinklist()
	{
		SimpleComboOptions scopts;
		CMSProfile *srcrgb=opts.profilemanager.GetProfile(opts.GetInRGBProfile());
		CMSProfile *srccmyk=opts.profilemanager.GetProfile(opts.GetInCMYKProfile());
		CMSProfile *dst=opts.profilemanager.GetProfile(opts.GetOutProfile());
		LCMSWrapper_Intent intent=opts.GetIntent();
		if(srcrgb)
		{
			DeviceLinkList dllist(srcrgb,dst,intent);
			for(unsigned int idx=0;idx<dllist.size();++idx)
			{
				DeviceLinkList_Entry *dl=&dllist[idx];
				scopts.Add(dl->filename.c_str(),TruncateUTF8(dl->displayname.c_str(),42).c_str());
			}
		}
		if(srccmyk)
		{
			DeviceLinkList dllist(srccmyk,dst,intent);
			for(unsigned int idx=0;idx<dllist.size();++idx)
			{
				DeviceLinkList_Entry *dl=&dllist[idx];
				scopts.Add(dl->filename.c_str(),TruncateUTF8(dl->displayname.c_str(),42).c_str());
			}
		}
		scopts.Add(NULL,_("Other..."));
		simplecombo_set_opts(SIMPLECOMBO(devicelink),scopts);
		std::string dl=opts.GetDeviceLink();
		simplecombo_set(SIMPLECOMBO(devicelink),dl.c_str());
	}

	static CMYKConversionMode convmodes[];
};


CMYKConversionMode CMYKConversionOptsDialog::convmodes[]=
{
	CMYKCONVERSIONMODE_NORMAL,
	CMYKCONVERSIONMODE_HOLDBLACK,
	CMYKCONVERSIONMODE_HOLDGREY,
	CMYKCONVERSIONMODE_OVERPRINT
};


void CMYKConversionOptions_Dialog(CMYKTool_Core &core,GtkWidget *parent)
{
	CMYKConversionOptsDialog dialog(core,parent);
	dialog.Run();
}


