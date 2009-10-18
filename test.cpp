#include <iostream>

#include <libgen.h>
#include <gtk/gtk.h>

#include "support/jobqueue.h"
#include "support/progressbar.h"
#include "support/rwmutex.h"
#include "support/thread.h"
#include "support/util.h"
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

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)


using namespace std;


////////////// ImageSource to filter by colorant mask //////////////

class ImageSource_ColorantMask : public ImageSource
{
	public:
	ImageSource_ColorantMask(ImageSource *source,DeviceNColorantList *list) : ImageSource(source), source(source)
	{
		int colcount=list->GetColorantCount();
		if(colcount!=samplesperpixel)
			throw "ImageSource_ColorantMask: Colorant count must match the number of channels!";

		channelmask=new bool[colcount];
		DeviceNColorant *col=list->FirstColorant();
		int idx=0;
		while(col)
		{
			channelmask[idx]=col->GetEnabled();
			col=col->NextColorant();
			++idx;
		}
		MakeRowBuffer();
	}
	~ImageSource_ColorantMask()
	{
		if(channelmask)
			delete[] channelmask;
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(currentrow==row)
			return(rowbuffer);
		switch(STRIP_ALPHA(type))
		{
			case IS_TYPE_RGB:
				for(int x=0;x<width*samplesperpixel;++x)
					rowbuffer[x]=0;
				break;
			default:
				for(int x=0;x<width*samplesperpixel;++x)
					rowbuffer[x]=0;
				break;
		}

		ISDataType *src=source->GetRow(row);

		for(int x=0;x<width;++x)
		{
			for(int s=0;s<samplesperpixel;++s)
			{
				if(channelmask[s])
					rowbuffer[x*samplesperpixel+s]=src[x*samplesperpixel+s];
			}
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	bool *channelmask;
	ImageSource *source;
};


//////////////  Conversion Worker Thread - ///////////////


class ConversionWorker : public Worker
{
	public:
	ConversionWorker(JobQueue &queue,ProfileManager &pm) : Worker(queue), profilemanager(pm)
	{
		factory=profilemanager.GetTransformFactory();
	}
	virtual ~ConversionWorker()
	{
		WaitCompletion();
		if(factory)
			delete factory;
	}
	ProfileManager &profilemanager;
	CMTransformFactory *factory;
};


///////////////////////  User interface /////////////////////////


class UITab;
class UITab_CacheThread;
class UITab_RenderThread;

class TestUI : public ConfigFile
{
	public:
	TestUI();
	~TestUI();
	void AddImage(const char *filename);
	static void ProcessImage(GtkWidget *wid,gpointer userdata);
	GtkWidget *window;
	protected:
	ProfileManager profilemanager;
	JobDispatcher dispatcher;
	CMTransformFactory factory;
	GtkWidget *imgsel;
	GtkWidget *notebook;
	friend class ImgUITab;
	friend class UITab_CacheJob;
	friend class UITab_RenderThread;
};


class ImgUITab : public UITab, public PTMutex
{
	public:
	ImgUITab(TestUI &parent,const char *filename);
	~ImgUITab();
	void SetImage(const char *filename);
	static void ColorantsChanged(GtkWidget *wid,gpointer userdata);
	static gboolean mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata);
	protected:
	TestUI &parent;
	GtkWidget *hbox;
	GtkWidget *colsel;
	GtkWidget *pbview;
	GtkWidget *popup;
	bool popupshown;
	CachedImage *image;
	DeviceNColorantList *collist;
	CMYKConversionOptions convopts;
	friend class UITab_CacheJob;
	friend class UITab_RenderThread;
};


class UITab_CacheJob : public Job
{
	public:
	UITab_CacheJob(ImgUITab &tab,const char *filename) : Job(), tab(tab), filename(NULL)
	{
		this->filename=SafeStrdup(filename);
	}
	~UITab_CacheJob()
	{
		free(filename);
	}
	void Run(Worker *w)
	{
		tab.ObtainMutex();
		CMYKConversionOptions convopts(tab.parent.profilemanager);
		try
		{
			ImageSource *is=ISLoadImage(filename);

			char *p;
			if(STRIP_ALPHA(is->type)==IS_TYPE_RGB)
			{
				if((p=tab.parent.profilemanager.SearchPaths("sRGB Color Space Profile.icm")));
				{
					convopts.SetInProfile(p);
					free(p);
				}
			}
			else
			{
				if((p=tab.parent.profilemanager.SearchPaths("USWebCoatedSWOP.icc")));
				{
					tab.convopts.SetInProfile(p);
					free(p);
				}
			}
			if((p=tab.parent.profilemanager.SearchPaths("USWebCoatedSWOP.icc")));
			{
				tab.convopts.SetOutProfile(p);
				free(p);
			}

			is=tab.convopts.Apply(is,NULL,&tab.parent.factory);

			tab.image=new CachedImage(is);
		}
		catch(const char *err)
		{
			cerr << "Error: " << err << endl;
		}
		tab.ReleaseMutex();

		delete this;
	}
	protected:
	ImgUITab &tab;
	char *filename;
};


class UITab_RenderThread : public ThreadFunction
{
	public:
	UITab_RenderThread(ImgUITab &tab) : ThreadFunction(), tab(tab), thread(this), pixbuf(NULL)
	{
		thread.Start();
	}
	~UITab_RenderThread()
	{
	}
	int Entry(Thread &t)
	{
		// The main thread doesn't need to block until the mutex is owned;
		tab.ObtainMutex();

		try
		{
			ImageSource *is=tab.image->GetImageSource();
			is=new ImageSource_ColorantMask(is,tab.collist);

			CMSTransform *transform=tab.parent.factory.GetTransform(CM_COLOURDEVICE_DISPLAY,is);
			if(transform)
				is=new ImageSource_CMS(is,transform);
			else
				cerr << "Couldn't create transform" << endl;

			pixbuf=pixbuf_from_imagesource(is);
			delete is;
		}
		catch(const char *err)
		{
			cerr << "Error: " << err << endl;
		}

		g_timeout_add(1,CleanupFunc,this);
		t.WaitSync();
		tab.ReleaseMutex();
		return(0);
	}
	static gboolean CleanupFunc(gpointer ud)
	{
		// FIXME - it's currently possible for the main thread to delete a tab in between this being triggered and it actually happening.
		UITab_RenderThread *t=(UITab_RenderThread *)ud;

		if(t->pixbuf)
		{
			pixbufview_set_pixbuf(PIXBUFVIEW(t->tab.pbview),t->pixbuf);
			g_object_unref(G_OBJECT(t->pixbuf));
		}
		t->thread.SendSync();
		delete t;
		return(FALSE);
	}
	protected:
	ImgUITab &tab;
	Thread thread;
	ImageSource *src;
	GdkPixbuf *pixbuf;
};


ImgUITab::ImgUITab(TestUI &parent,const char *filename)
	: UITab(parent.notebook), PTMutex(), parent(parent), colsel(NULL), pbview(NULL), image(NULL), collist(NULL), convopts(parent.profilemanager)
{
	hbox=GetBox();
	g_signal_connect(G_OBJECT(hbox),"motion-notify-event",G_CALLBACK(mousemove),this);

	pbview=pixbufview_new(NULL,false);
	gtk_box_pack_start(GTK_BOX(hbox),pbview,TRUE,TRUE,0);
	gtk_widget_show(pbview);


	popupshown=false;
	popup=gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_default_size(GTK_WINDOW(popup),100,180);
	gtk_window_set_transient_for(GTK_WINDOW(popup),GTK_WINDOW(parent.window));

	colsel=colorantselector_new(NULL);
	gtk_signal_connect (GTK_OBJECT (colsel), "changed",
		(GtkSignalFunc) ColorantsChanged, this);
	gtk_container_add(GTK_CONTAINER(popup),colsel);
	gtk_widget_show(colsel);

	SetImage(filename);
}

ImgUITab::~ImgUITab()
{
	// Need to do this in an Attempt / event pump loop, so that the idle-func of a thread
	// can execute and cause the mutex to be released.
	while(!AttemptMutex())
	{
		gtk_main_iteration_do(TRUE);
	}
	int page=gtk_notebook_page_num(GTK_NOTEBOOK(parent.notebook),hbox);
	gtk_notebook_remove_page(GTK_NOTEBOOK(parent.notebook),page);
	if(collist)
		delete collist;
	if(image)
		delete image;
}


void ImgUITab::SetImage(const char *filename)
{
	ObtainMutex();
	if(image)
		delete image;
	image=NULL;
	if(collist)
		delete collist;
	collist=NULL;
	ReleaseMutex();

	try
	{
		collist=new DeviceNColorantList(IS_TYPE_CMYK);
		colorantselector_set_colorants(COLORANTSELECTOR(colsel),collist);

		char *fn=SafeStrdup(filename);
		SetText(basename(fn));
		free(fn);

		parent.dispatcher.PushJob(new UITab_CacheJob(*this,filename));
		new UITab_RenderThread(*this);
	}
	catch (const char *err)
	{
		ErrorMessage_Dialog(err,parent.window);
	}
}


void ImgUITab::ColorantsChanged(GtkWidget *wid,gpointer userdata)
{
	ImgUITab *ob=(ImgUITab *)userdata;
	new UITab_RenderThread(*ob);
}


gboolean ImgUITab::mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata)
{
	ImgUITab *ui=(ImgUITab *)userdata;

	int x,y;
	GdkModifierType mods;
	gdk_window_get_pointer (widget->window, &x, &y, &mods);
	int w,h;
	gtk_window_get_size(GTK_WINDOW(ui->parent.window),&w,&h);

	if(ui->popupshown && (x<(w-w/20) || y<(h/2)))
	{
		gtk_widget_hide_all(ui->popup);
		ui->popupshown=false;
	}

	if(!ui->popupshown && x>(w-w/20) && y>(h/2))
	{
		int winx,winy;
		int pw,ph;
		gtk_window_get_position(GTK_WINDOW(ui->parent.window),&winx,&winy);
		gtk_window_get_size(GTK_WINDOW(ui->popup),&pw,&ph);
		gtk_window_move(GTK_WINDOW(ui->popup),winx+w-pw,winy+h-ph);
		gtk_widget_show_all(ui->popup);
		ui->popupshown=true;
	}

	return(FALSE);
}


TestUI::TestUI() : ConfigFile(), profilemanager(this,"[ColourManagement]"),
	dispatcher(0), factory(profilemanager), notebook(NULL)
{
	profilemanager.SetInt("DefaultCMYKProfileActive",1);

	dispatcher.AddWorker(new ConversionWorker(dispatcher,profilemanager));
	dispatcher.AddWorker(new ConversionWorker(dispatcher,profilemanager));
	dispatcher.AddWorker(new ConversionWorker(dispatcher,profilemanager));


	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),600,450);
	gtk_window_set_title (GTK_WINDOW (window), _("TestUI"));
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		(GtkSignalFunc) gtk_main_quit, NULL);
	gtk_widget_show(window);


	GtkWidget *hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),hbox);
	gtk_widget_show(hbox);

	imgsel=imageselector_new(NULL,GTK_SELECTION_MULTIPLE,false);
	gtk_signal_connect (GTK_OBJECT (imgsel), "double-clicked",
		(GtkSignalFunc) ProcessImage,this);
	gtk_box_pack_start(GTK_BOX(hbox),imgsel,FALSE,FALSE,0);
	gtk_widget_show(imgsel);

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
	const char *fn=imageselector_get_filename(IMAGESELECTOR(ui->imgsel));
	if(fn)
		new ImgUITab(*ui,fn);
}


void TestUI::AddImage(const char *filename)
{
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

