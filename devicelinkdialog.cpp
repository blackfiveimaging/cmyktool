#include <gtk/gtk.h>

#include "debug.h"

#include "devicelinkdialog.h"


class DeviceLinkDialog
{
	public:
	DeviceLinkDialog(CMYKConversionOptions &opts,GtkWidget *parent) : opts(opts), parent(parent)
	{
		window=gtk_dialog_new_with_buttons(_("DeviceLink manager"),
			GTK_WINDOW(parent),GtkDialogFlags(0),
			GTK_STOCK_OK,GTK_RESPONSE_OK,
			NULL);

		GtkWidget *vbox = GTK_DIALOG(window)->vbox;

		GtkWidget *hbox = gtk_hbox_new(FALSE,8);
		gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,8);
		gtk_widget_show(hbox);

		vbox=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,0);
		gtk_widget_show(vbox);

		gtk_widget_show(window);
	}
	~DeviceLinkDialog()
	{
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
	CMYKConversionOptions &opts;
	GtkWidget *parent;
	GtkWidget *window;
};


void DeviceLink_Dialog(CMYKConversionOptions &opts,GtkWidget *parent)
{
	DeviceLinkDialog dlg(opts,parent);
	dlg.Run();
}

