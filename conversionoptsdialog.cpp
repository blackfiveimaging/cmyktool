#include <iostream>

#include <gtk/gtk.h>

#include "support/debug.h"

#include "profilemanager/profilemanager.h"
#include "profilemanager/profileselector.h"
#include "profilemanager/intentselector.h"

#include "conversionoptsdialog.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;


class CMYKConversionOptsDialog
{
	public:
	CMYKConversionOptsDialog(CMYKConversionOptions &opts, GtkWidget *parent) : opts(opts)
	{
		window=gtk_dialog_new_with_buttons(_("Colour conversion options"),
			GTK_WINDOW(parent),GtkDialogFlags(0),
			GTK_STOCK_OK,GTK_RESPONSE_OK,
			NULL);

		GtkWidget *vbox = GTK_DIALOG(window)->vbox;
		
		ps = profileselector_new(&opts.profilemanager,IS_TYPE_CMYK);
		g_signal_connect(ps,"changed",G_CALLBACK(profile_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),ps,FALSE,FALSE,8);
		gtk_widget_show(ps);

		is = intentselector_new(&opts.profilemanager);

		g_signal_connect(is,"changed",G_CALLBACK(intent_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),is,FALSE,FALSE,8);
		gtk_widget_show(is);

		gtk_widget_show(window);

		intentselector_setintent(INTENTSELECTOR(is),opts.GetIntent());

		profileselector_set_filename(PROFILESELECTOR(ps),opts.GetOutProfile());

		gtk_dialog_run(GTK_DIALOG(window));
	}
	~CMYKConversionOptsDialog()
	{
		gtk_widget_destroy(window);
	}
	private:
	CMYKConversionOptions &opts;
	GtkWidget *window;
	GtkWidget *ps;
	GtkWidget *is;

	static void	profile_changed(GtkWidget *widget,gpointer user_data)
	{
		cerr << "Received changed signal from ProfileSelector" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		ProfileSelector *c=PROFILESELECTOR(dlg->ps);
		const char *val=profileselector_get_filename(c);
		if(val)
		{
			dlg->opts.SetOutProfile(val);
			Debug[TRACE] << "Selected: " << val << endl;
		}
		else
			Debug[WARN] << "No profile selected... " << endl;
	}


	static void	intent_changed(GtkWidget *widget,gpointer user_data)
	{
		cerr << "Received changed signal from IntentSelector" << endl;
		CMYKConversionOptsDialog *dlg=(CMYKConversionOptsDialog *)user_data;
		IntentSelector *is=INTENTSELECTOR(dlg->is);

		LCMSWrapper_Intent intent=LCMSWrapper_Intent(intentselector_getintent(is));

		dlg->opts.SetIntent(intent);

		Debug[TRACE] << "Intent " << intent << ": " << dlg->opts.profilemanager.GetIntentName(intent) << endl;
	}

};


void CMYKConversionOptions_Dialog(CMYKConversionOptions &opts,GtkWidget *parent)
{
	CMYKConversionOptsDialog dialog(opts,parent);
}


