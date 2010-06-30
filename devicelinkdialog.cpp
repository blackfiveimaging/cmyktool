#include <gtk/gtk.h>

#include "simplelistview.h"
#include "debug.h"

#include "devicelink.h"
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
		gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,8);
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
	
	GtkWidget *parent;
	GtkWidget *window;
};


void DeviceLink_Dialog(CMYKConversionOptions &opts,GtkWidget *parent)
{
	DeviceLinkDialog dlg(opts,parent);
	dlg.Run();
}

