#include <iostream>

#include <gtk/gtk.h>

#include "support/debug.h"

#include "miscwidgets/simplecombo.h"

#include "profilemanager/profilemanager.h"
#include "profilemanager/profileselector.h"
#include "profilemanager/intentselector.h"

#include "conversionoptsdialog.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;

#define RESPONSE_SAVE 1


class CMYKConversionOptsDialog
{
	public:
	CMYKConversionOptsDialog(CMYKConversionOptions &opts, GtkWidget *parent) : opts(opts)
	{
		window=gtk_dialog_new_with_buttons(_("Colour conversion options"),
			GTK_WINDOW(parent),GtkDialogFlags(0),
			GTK_STOCK_SAVE,RESPONSE_SAVE,
			GTK_STOCK_OK,GTK_RESPONSE_OK,
			NULL);

		GtkWidget *vbox = GTK_DIALOG(window)->vbox;

		GtkWidget *hbox = gtk_hbox_new(FALSE,8);
		gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,8);
		gtk_widget_show(hbox);

		GtkWidget *table=gtk_table_new(2,4,FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table),6);
		gtk_table_set_col_spacing(GTK_TABLE(table),0,6);
		gtk_box_pack_start(GTK_BOX(hbox),table,FALSE,FALSE,6);
		gtk_widget_show(table);

		GtkWidget *label;

		// Output profile

		label=gtk_label_new(_("Output Profile:"));
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,0,1);
		gtk_widget_show(label);

		ps = profileselector_new(&opts.profilemanager);
		g_signal_connect(ps,"changed",G_CALLBACK(profile_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),ps,1,2,0,1);
		gtk_widget_show(ps);


		// Rendering intent

		label=gtk_label_new(_("Rendering intent:"));
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,1,2);
		gtk_widget_show(label);

		is = intentselector_new(&opts.profilemanager);
		g_signal_connect(is,"changed",G_CALLBACK(intent_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),is,1,2,1,2);
		gtk_widget_show(is);


		// Conversion mode

		label=gtk_label_new(_("Conversion mode:"));
		gtk_table_attach_defaults(GTK_TABLE(table),label,0,1,2,3);
		gtk_widget_show(label);

		SimpleComboOptions scopts;
		scopts.Add("",_("Normal"),_("A regular conversion to CMYK."));
		scopts.Add("",_("Hold Black"),_("Maps pure black input to CMYK (0,0,0,100)"));
		scopts.Add("",_("Hold Grey"),_("Maps grey input (R=G=B) to K channel only"));
		scopts.Add("",_("Overprint"),_("Maps pure black to CMYK (0,0,0,100) and copies C, M and Y from the most recent non-black pixel."));

		combo=simplecombo_new(scopts);
		g_signal_connect(combo,"changed",G_CALLBACK(combo_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),combo,1,2,2,3);
		gtk_widget_show(combo);

		for(int i=0;i<int(CMYKCONVERSIONMODE_MAX);++i)
		{
			if(convmodes[i]==opts.GetMode())
				simplecombo_set_index(SIMPLECOMBO(combo),i);
		}

		gtk_widget_show(window);

		intentselector_setintent(INTENTSELECTOR(is),opts.GetIntent());

		profileselector_set_filename(PROFILESELECTOR(ps),opts.GetOutProfile());
		SetSensitive();

		gint result=gtk_dialog_run(GTK_DIALOG(window));
		switch(result)
		{
			case RESPONSE_SAVE:
				{
					CMYKConversionPreset p;
					p.Store(opts);
					p.SetString("DisplayName",_("New preset"));
					p.Save(PRESET_NEW_ESCAPE);
				}
				break;
			default:
				break;
		}
	}
	~CMYKConversionOptsDialog()
	{
		gtk_widget_destroy(window);
	}
	void SetSensitive()
	{
		gtk_widget_set_sensitive(GTK_WIDGET(combo),opts.GetOutputType()==IS_TYPE_CMYK);
	}
	private:
	CMYKConversionOptions &opts;
	GtkWidget *window;
	GtkWidget *ps;
	GtkWidget *is;
	GtkWidget *combo;

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
	}


	static void	intent_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from IntentSelector" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		IntentSelector *is=INTENTSELECTOR(dlg->is);

		LCMSWrapper_Intent intent=LCMSWrapper_Intent(intentselector_getintent(is));

		dlg->opts.SetIntent(intent);

		Debug[TRACE] << "Intent " << intent << ": " << dlg->opts.profilemanager.GetIntentName(intent) << endl;
	}


	static void	combo_changed(GtkWidget *widget,gpointer user_data)
	{
		Debug[TRACE] << "Received changed signal from combo" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		SimpleCombo *c=SIMPLECOMBO(dlg->combo);

		int idx=simplecombo_get_index(c);

		dlg->opts.SetMode(convmodes[idx]);

		Debug[TRACE] << "Conversion mode set to: " << convmodes[idx] << endl;
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


void CMYKConversionOptions_Dialog(CMYKConversionOptions &opts,GtkWidget *parent)
{
	CMYKConversionOptsDialog dialog(opts,parent);
}


