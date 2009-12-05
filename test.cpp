#include <iostream>

#include <libgen.h>
#include <gtk/gtk.h>

#include "support/debug.h"
#include "support/jobqueue.h"
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
#include "miscwidgets/errordialogqueue.h"
#include "miscwidgets/uitab.h"
#include "progressbar.h"
#include "profilemanager/profilemanager.h"
#include "cachedimage.h"
#include "dialogs.h"

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
	void ProcessImage(const char *filename);
	static void processsingle(GtkWidget *wid,gpointer userdata);
	static void batchprocess(GtkWidget *wid,gpointer userdata);
	static void showconversiondialog(GtkWidget *wid,gpointer userdata);
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, GtkSelectionData *selection_data, guint info, guint time, gpointer data);
	GtkWidget *window;
	protected:
	ProfileManager profilemanager;
	JobDispatcher dispatcher;
	CMTransformFactory factory;
	GtkWidget *imgsel;
	GtkWidget *notebook;
	CMYKConversionOptions convopts;
};


#define TARGET_URI_LIST 1

static GtkTargetEntry dnd_file_drop_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
static gint dnd_file_drop_types_count = 1;

void TestUI::get_dnd_data(GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y,
				     GtkSelectionData *selection_data, guint info,
				     guint time, gpointer data)
{
	gchar *urilist=g_strdup((const gchar *)selection_data->data);

	TestUI *ui=(TestUI *)data;

	while(*urilist)
	{
		if(strncmp(urilist,"file:",5))
		{
			g_print("Warning: only local files (file://) are currently supported\n");
			while(*urilist && *urilist!='\n' && *urilist!='\r')
				++urilist;
			while(*urilist=='\n' || *urilist=='\r')
				*urilist++;
		}
		else
		{
			gchar *uri=urilist;
			while(*urilist && *urilist!='\n' && *urilist!='\r')
				++urilist;
			if(*urilist)
			{
				while(*urilist=='\n' || *urilist=='\r')
					*urilist++=0;
				gchar *filename=g_filename_from_uri(uri,NULL,NULL);

				ui->AddImage(filename);
				ui->ProcessImage(filename);
			}
		}
	}
}



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

	gtk_drag_dest_set(GTK_WIDGET(hbox),
			  GtkDestDefaults(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP),
			  dnd_file_drop_types, dnd_file_drop_types_count,
                          GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
	g_signal_connect(G_OBJECT(hbox), "drag_data_received",
			 G_CALLBACK(get_dnd_data), this);


	// Leftmost column - imageselector + buttons

	GtkWidget *vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),vbox,FALSE,FALSE,0);
	gtk_widget_show(vbox);


	imgsel=imageselector_new(NULL,GTK_SELECTION_MULTIPLE,false);
	gtk_signal_connect (GTK_OBJECT (imgsel), "double-clicked",
		(GtkSignalFunc) processsingle,this);
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


	// Notebook


	notebook=gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook),true);
	gtk_box_pack_start(GTK_BOX(hbox),notebook,TRUE,TRUE,0);
	gtk_widget_show(GTK_WIDGET(notebook));

	PreferencesDialog(GTK_WINDOW(window),profilemanager);
}


TestUI::~TestUI()
{
}


void TestUI::ProcessImage(const char *filename)
{
	new CMYKUITab(window,notebook,convopts,dispatcher,filename);
}


void TestUI::processsingle(GtkWidget *wid,gpointer userdata)
{
	try
	{
		TestUI *ui=(TestUI *)userdata;

		int idx=0;
		const char *fn;

		while((fn=imageselector_get_filename(IMAGESELECTOR(ui->imgsel),idx++)))
			ui->ProcessImage(fn);
	}
	catch (const char *err)
	{
		ErrorDialogs.AddMessage(err);
	}
}


void TestUI::batchprocess(GtkWidget *wid,gpointer userdata)
{
	try
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
	catch (const char *err)
	{
		ErrorDialogs.AddMessage(err);
	}
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
		ErrorDialogs.AddMessage(err);
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

