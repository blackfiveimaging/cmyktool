#include <iostream>
#include <cstdio>

#include <gtk/gtk.h>

#include "imagesource_util.h"
#include "pixbuf_from_imagesource.h"
#include "imageselector.h"
#include "pixbufview.h"
#include "generaldialogs.h"
#include "progressbar.h"
#include "progresstext.h"
#include "thread.h"
#include "psripui.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;


class PSRipUIThread : public ThreadFunction, public Thread
{
	public:
	PSRipUIThread(PSRipUI &ripui,const char *filename) : ThreadFunction(), Thread(this), ripui(ripui), session(NULL), prog(NULL)
	{
		prog=new ProgressBar(_("Ripping File..."),true,ripui.window);
		session=new PSRip;
		Start();
		WaitSync();
//		sync.Wait();
		cerr << "Subthread up, running and subscribed - starting RIP" << endl;
		session->Rip(filename,ripui.options);
		cerr << "Rip started" << endl;
		sync.ObtainMutex();
		sync.Broadcast();
		sync.ReleaseMutex();
	}
	~PSRipUIThread()
	{
		if(prog)
			delete prog;
		if(session)
			delete session;
	}
	int Entry(Thread &t)
	{
		cerr << "RipUI thread running..." << endl;
		session->Event.Subscribe();
		sync.ObtainMutex();
		SendSync();
		sync.WaitCondition();
		sync.ReleaseMutex();
		g_timeout_add(100,pulse,this);
		while(!session->TestFinished())
		{
			cerr << "RipUIThread: Waiting on event" << endl;
			if(session->Event.QueryAndWait())
			{
				cerr << "RipUIThread: Event detected - triggering update" << endl;
				g_timeout_add(1,updateimgselfunc,this);
			}
#ifdef WIN32
			Sleep(100);
#else
			usleep(100000);
#endif
		}
		g_timeout_add(1,updateimgselfunc,this);
		return(0);
	}
	protected:
	static gboolean pulse(gpointer ud)
	{
		PSRipUIThread *t=(PSRipUIThread *)ud;
		if(t->session->TestFinished())
		{
			if(t->prog)
				delete t->prog;
			t->prog=NULL;
			return(FALSE);
		}
		if(t->prog)
		{
			if(!t->prog->DoProgress(0,0))
				t->session->Stop();
		}
		return(TRUE);
	}
	static gboolean updateimgselfunc(gpointer ud)
	{
		PSRipUIThread *t=(PSRipUIThread *)ud;
		Debug[TRACE] << "Event received - updating image list..." << endl;

		t->session->TempFileTracker::ObtainMutex();
//		Debug[TRACE] << "Session contains " << t->session->size() << " images " << endl;
		for(unsigned int idx=0;idx<t->session->size();++idx)
		{
			TempFile *temp=(*t->session)[idx];
//			Debug[TRACE] << "Attempting to add " << temp->Filename() << endl;
			imageselector_add_filename(IMAGESELECTOR(t->ripui.imgsel),temp->Filename());
		}
		t->session->TempFileTracker::ReleaseMutex();
		return(FALSE);
	}
	ThreadCondition sync;
	PSRipUI &ripui;
	PSRip *session;
	Progress *prog;
};


PSRipUI::PSRipUI() : SearchPathHandler(), window(NULL), imgsel(NULL), pbview(NULL), monitorthread(NULL)
{
#ifdef WIN32
	AddPath("c:/gs/gs8.64/bin;c:/Program Files/gs/bin;c:/Program Files/gs/gs8.64/bin");
#else
	AddPath("/usr/bin");
#endif
	GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),600,450);
	gtk_window_set_title (GTK_WINDOW (window), _("PSRip Test"));
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		(GtkSignalFunc) gtk_main_quit, NULL);
	gtk_widget_show(window);

	GtkWidget *hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),hbox);
	gtk_widget_show(GTK_WIDGET(hbox));

	imgsel=imageselector_new(NULL,GTK_SELECTION_SINGLE,false);
	gtk_box_pack_start(GTK_BOX(hbox),imgsel,FALSE,FALSE,0);
	gtk_widget_show(imgsel);

	pbview=pixbufview_new(NULL,false);
	gtk_box_pack_start(GTK_BOX(hbox),pbview,TRUE,TRUE,0);
	gtk_widget_show(pbview);

	gtk_signal_connect (GTK_OBJECT (imgsel), "changed",
		G_CALLBACK(image_changed), this);
}


PSRipUI::~PSRipUI()
{
	if(monitorthread)
		delete monitorthread;
}


void PSRipUI::Rip(const char *filename)
{
	if(monitorthread)
		delete monitorthread;
	monitorthread=NULL;

	monitorthread=new PSRipUIThread(*this,filename);
}


void PSRipUI::image_changed(GtkWidget *widget,gpointer userdata)
{
	PSRipUI *rip=(PSRipUI *)userdata;
	const char *filename=imageselector_get_filename(IMAGESELECTOR(rip->imgsel));
	try
	{
		cerr << "Adding image " << filename << endl;
		ImageSource *is=ISLoadImage(filename);
		if(is)
		{
			GdkPixbuf *pb=pixbuf_from_imagesource(is);
			pixbufview_set_pixbuf(PIXBUFVIEW(rip->pbview),pb);
			g_object_unref(G_OBJECT(pb));
			delete is;
		}
	}
	catch(const char *err)
	{
		ErrorMessage_Dialog(err);
	}
}


