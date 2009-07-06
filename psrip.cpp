#include <iostream>
#include <cstdio>

#include <gtk/gtk.h>

#include "imagesource/imagesource_util.h"
#include "imagesource/pixbuf_from_imagesource.h"
#include "miscwidgets/imageselector.h"
#include "miscwidgets/pixbufview.h"
#include "miscwidgets/generaldialogs.h"
#include "support/progressbar.h"
#include "support/progresstext.h"
#include "psrip.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;


class PSRipUI : public SearchPathHandler
{
	public:
	PSRipUI() : SearchPathHandler(), window(NULL), imgsel(NULL), pbview(NULL)
	{
#ifdef WIN32
		AddPath("c:/gs/bin/;c:/Program Files/gs/bin");
#else
		AddPath("/usr/bin");
#endif

		GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (window), _("PSRip Test"));
		gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			(GtkSignalFunc) gtk_main_quit, NULL);
		gtk_widget_show(window);

		GtkWidget *hbox=gtk_hbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(window),hbox);
		gtk_widget_show(GTK_WIDGET(hbox));

		imgsel=imageselector_new(NULL,true,false);
		gtk_box_pack_start(GTK_BOX(hbox),imgsel,FALSE,FALSE,0);
		gtk_widget_show(imgsel);

		pbview=pixbufview_new(NULL,false);
		gtk_box_pack_start(GTK_BOX(hbox),pbview,TRUE,TRUE,0);
		gtk_widget_show(pbview);

		gtk_signal_connect (GTK_OBJECT (imgsel), "changed",
			G_CALLBACK(image_changed), this);
	}
	~PSRipUI()
	{
		if(session)
			delete session;
	}
	void Rip(const char *filename)
	{
		ProgressBar *prog=new ProgressBar("Ripping file...",false,window);
		session=new PSRip(*this,filename,prog,IS_TYPE_CMYK);

		TempFile *temp=session->FirstTempFile();
		while(temp)
		{
			cerr << "Got file: " << temp->Filename() << endl;
			imageselector_set_filename(IMAGESELECTOR(imgsel),temp->Filename());
			temp=temp->NextTempFile();
		}
		delete prog;
	}
	static void image_changed(GtkWidget *widget,gpointer userdata)
	{
		PSRipUI *rip=(PSRipUI *)userdata;
		const char *filename=imageselector_get_filename(IMAGESELECTOR(rip->imgsel));
		try
		{
			cerr << "Adding image " << filename << endl;
			ImageSource *is=ISLoadImage(filename);
			GdkPixbuf *pb=pixbuf_from_imagesource(is);
			pixbufview_set_pixbuf(PIXBUFVIEW(rip->pbview),pb);
		}
		catch(const char *err)
		{
			ErrorMessage_Dialog(err);
		}
	}
	protected:
	GtkWidget *window;
	GtkWidget *imgsel;
	GtkWidget *pbview;
	GdkPixbuf *currentimage;
	PSRip *session;
};


int main(int argc,char *argv[])
{
	gtk_init(&argc,&argv);

	if(argc>1)
	{
		try
		{
			PSRipUI ripper;
			ripper.Rip(argv[1]);

			gtk_main();

		}
		catch(const char *err)
		{
			cerr << "Error: " << err << endl;
		}
	}
	return(0);
}

