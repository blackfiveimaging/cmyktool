#include <iostream>

#include <libgen.h>
#include <gtk/gtk.h>

#include "support/debug.h"
#include "support/jobqueue.h"
#include "support/progressbar.h"
#include "support/rwmutex.h"
#include "support/thread.h"
#include "imageutils/tiffsave.h"
#include "imagesource/imagesource_util.h"
#include "imagesource/imagesource_cms.h"
#include "imagesource/pixbuf_from_imagesource.h"
#include "miscwidgets/pixbufview.h"
#include "miscwidgets/colorantselector.h"
#include "miscwidgets/imageselector.h"
#include "miscwidgets/generaldialogs.h"
#include "miscwidgets/uitab.h"
#include "profilemanager/profilemanager.h"
#include "cachedimage.h"

#include "conversionopts.h"
#include "cmtransformworker.h"
#include "conversionoptsdialog.h"

#include "cmykuitab.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)


using namespace std;

class UITab_RenderThread;

class TestUI : public ConfigFile
{
	public:
	TestUI();
	~TestUI();
	void AddImage(const char *filename);
	static void ProcessImage(GtkWidget *wid,gpointer userdata);
	static void batchprocess(GtkWidget *wid,gpointer userdata);
	static void showconversiondialog(GtkWidget *wid,gpointer userdata);
	GtkWidget *window;
	protected:
	ProfileManager profilemanager;
	JobDispatcher dispatcher;
	CMTransformFactory factory;
	GtkWidget *imgsel;
	GtkWidget *notebook;
	CMYKConversionOptions convopts;
};


TestUI::TestUI() : ConfigFile(), profilemanager(this,"[ColourManagement]"),
	dispatcher(0), factory(profilemanager), notebook(NULL), convopts(profilemanager)
{
	profilemanager.SetInt("DefaultCMYKProfileActive",1);

	dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
	dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
	dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));


	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),600,450);
	gtk_window_set_title (GTK_WINDOW (window), _("TestUI"));
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		(GtkSignalFunc) gtk_main_quit, NULL);
	gtk_widget_show(window);


	GtkWidget *hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),hbox);
	gtk_widget_show(hbox);

	GtkWidget *vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),vbox,FALSE,FALSE,0);
	gtk_widget_show(vbox);


	imgsel=imageselector_new(NULL,GTK_SELECTION_MULTIPLE,false);
	gtk_signal_connect (GTK_OBJECT (imgsel), "double-clicked",
		(GtkSignalFunc) ProcessImage,this);
	gtk_box_pack_start(GTK_BOX(vbox),imgsel,TRUE,TRUE,0);
	gtk_widget_show(imgsel);


	GtkWidget *tmp=gtk_button_new_with_label("Conversion Options...");
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(showconversiondialog),this);
	gtk_widget_show(tmp);


	tmp=gtk_button_new_with_label("Batch Process...");
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(batchprocess),this);
	gtk_widget_show(tmp);


	notebook=gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook),true);
	gtk_box_pack_start(GTK_BOX(hbox),notebook,TRUE,TRUE,0);
	gtk_widget_show(GTK_WIDGET(notebook));
}


TestUI::~TestUI()
{
}


void TestUI::ProcessImage(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;

	int idx=0;
	const char *fn;

	while((fn=imageselector_get_filename(IMAGESELECTOR(ui->imgsel),idx++)))
		new CMYKUITab(ui->window,ui->notebook,ui->convopts,ui->dispatcher,fn);
}


void TestUI::batchprocess(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;
	int idx=0;
	const char *fn=NULL;
	while((fn=imageselector_get_filename(IMAGESELECTOR(ui->imgsel),idx++)))
	{
		cerr << "Batch Process: Got filename " << fn << endl;
		new CMYKUITab(ui->window,ui->notebook,ui->convopts,ui->dispatcher,fn);
	}
	cerr << "Batch Process: processed " << idx << " filenames" << endl;
}


void TestUI::showconversiondialog(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;
	CMYKConversionOptions_Dialog(ui->convopts,ui->window);
}


void TestUI::AddImage(const char *filename)
{
	Debug.SetLevel(TRACE);
	try
	{
		ImageSource *is=ISLoadImage(filename);
		if(is)
		{
			int h=128;
			int w=(is->width*h)/is->height;
			if(w>h)
			{
				w=128;
				h=(is->height*w)/is->width;
			}
			is=ISScaleImageBySize(is,w,h,IS_SCALING_DOWNSAMPLE);

			CMSTransform *transform=factory.GetTransform(CM_COLOURDEVICE_DISPLAY,is);
			if(transform)
				is=new ImageSource_CMS(is,transform);

			GdkPixbuf *pb=pixbuf_from_imagesource(is);
			if(pb)
				imageselector_add_filename(IMAGESELECTOR(imgsel),filename,pb);
		}
	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
}


int main(int argc,char **argv)
{
	gtk_init(&argc,&argv);

	Debug.SetLevel(WARN);

	try
	{
		TestUI ui;

		ProgressBar *prog=new ProgressBar(_("Adding images..."),true,ui.window);

		for(int i=1;i<argc;++i)
		{
			ui.AddImage(argv[i]);
			if(!prog->DoProgress(i,argc))
				i=argc;
		}
		delete prog;

		gtk_main();

	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	return(0);
}

