#include <iostream>
#include <cstring>

#include <libgen.h>
#include <gtk/gtk.h>

#include "support/debug.h"
#include "support/jobqueue.h"
#include "support/rwmutex.h"
#include "support/thread.h"
#include "support/pathsupport.h"
#include "support/dirtreewalker.h"
#include "support/util.h"

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
#include "miscwidgets/simplecombo.h"
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

#define PRESET_OTHER_ESCAPE "<other>"
#define PRESET_PREVIOUS_ESCAPE "previous"

using namespace std;

class UITab_RenderThread;

class CMYKTool_Core : public ConfigFile, public ConfigDB
{
	public:
	CMYKTool_Core() : ConfigFile(), ConfigDB(Template), profilemanager(this,"[ColourManagement]"),
		dispatcher(0),factory(profilemanager), convopts(profilemanager)
	{
		new ConfigDBHandler(this,"[CMYKTool]",this);
		profilemanager.SetInt("DefaultCMYKProfileActive",1);

		char *fn=substitute_xdgconfighome("$XDG_CONFIG_HOME/cmyktool/cmyktool.conf");
		ParseConfigFile(fn);
		confname=fn;
		free(fn);

		dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
		dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
		dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
	}
	~CMYKTool_Core()
	{
	}
	void ProcessImage(const char *filename)
	{
		cerr << "*** Processing image" << endl;
	}
	void SaveConfig()
	{
		SaveConfigFile(confname.c_str());
	}
	protected:
	ProfileManager profilemanager;
	JobDispatcher dispatcher;
	CMTransformFactory factory;
	CMYKConversionOptions convopts;
	string confname;
	static ConfigTemplate Template[];
};


ConfigTemplate CMYKTool_Core::Template[]=
{
	ConfigTemplate("Win_X",20),
	ConfigTemplate("Win_Y",20),
	ConfigTemplate("Win_W",600),
	ConfigTemplate("Win_H",400),
	ConfigTemplate()
};


class TestUI : public CMYKTool_Core
{
	public:
	TestUI();
	~TestUI();
	void BuildComboOpts(SimpleComboOptions &opts);
	void AddImage(const char *filename);
	void ProcessImage(const char *filename);
	static void combochanged(GtkWidget *wid,gpointer userdata);
	static void processsingle(GtkWidget *wid,gpointer userdata);
	static void batchprocess(GtkWidget *wid,gpointer userdata);
	static void showconversiondialog(GtkWidget *wid,gpointer userdata);
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, GtkSelectionData *selection_data, guint info, guint time, gpointer data);
	GtkWidget *window;
	protected:
	GtkWidget *imgsel;
	GtkWidget *notebook;
	GtkWidget *combo;
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


TestUI::TestUI()
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


	// Presets

	SimpleComboOptions opts;
	BuildComboOpts(opts);
	combo=simplecombo_new(opts);
	gtk_box_pack_start(GTK_BOX(vbox),combo,FALSE,FALSE,0);
	simplecombo_set(SIMPLECOMBO(combo),PRESET_PREVIOUS_ESCAPE);
	g_signal_connect(G_OBJECT(combo),"changed",G_CALLBACK(combochanged),this);
	gtk_widget_show(combo);

//	GtkWidget *tmp=gtk_button_new_with_label("Conversion Options...");
//	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
//	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(showconversiondialog),this);
//	gtk_widget_show(tmp);


	GtkWidget *tmp=gtk_button_new_with_label("Batch Process...");
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(batchprocess),this);
	gtk_widget_show(tmp);


	// Notebook


	notebook=gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook),true);
	gtk_box_pack_start(GTK_BOX(hbox),notebook,TRUE,TRUE,0);
	gtk_widget_show(GTK_WIDGET(notebook));

	// Force loading of the "previous" preset...
	const char *key=simplecombo_get(SIMPLECOMBO(combo));
	if(strcmp(key,PRESET_OTHER_ESCAPE)!=0)
		combochanged(combo,this);
}


TestUI::~TestUI()
{
	CMYKConversionPreset p;
	p.Store(convopts);
	p.SetString("DisplayName",_("Previous settings"));
	p.Save(PRESET_PREVIOUS_ESCAPE);
}


void TestUI::BuildComboOpts(SimpleComboOptions &opts)
{
	char *configdir=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);
	DirTreeWalker dtw(configdir);
	const char *fn;

	while((fn=dtw.NextFile()))
	{
		CMYKConversionPreset p;
		p.Load(fn);
		const char *dn=p.FindString("DisplayName");
		if(!(dn && strlen(dn)>0))
			dn=_("<unknown>");
		opts.Add(fn,dn,fn);
	}
	free(configdir);

	opts.Add(PRESET_OTHER_ESCAPE,_("Other..."),_("Create a new preset"),true);
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


void TestUI::combochanged(GtkWidget *wid,gpointer userdata)
{
	try
	{
		TestUI *ui=(TestUI *)userdata;
		const char *key=simplecombo_get(SIMPLECOMBO(ui->combo));
		if(strcmp(key,PRESET_OTHER_ESCAPE)==0)
		{
			CMYKConversionOptions_Dialog(ui->convopts,ui->window);
		}
		else
		{
			CMYKConversionPreset p;
			p.Load(key);
			p.Retrieve(ui->convopts);
		}
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
			ui->ProcessImage(fn);
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

	char *configdir=substitute_xdgconfighome("$XDG_CONFIG_HOME/cmyktool");
	CreateDirIfNeeded(configdir);
	free(configdir);
	configdir=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);
	CreateDirIfNeeded(configdir);
	free(configdir);

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

