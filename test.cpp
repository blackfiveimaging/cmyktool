#include <iostream>

#include <libgen.h>
#include <gtk/gtk.h>

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
#include "profilemanager/profilemanager.h"
#include "cachedimage.h"

#include "conversionopts.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)


using namespace std;


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
	CMTransformFactory factory;
	GtkWidget *imgsel;
	GtkWidget *notebook;
	friend class UITab;
	friend class UITab_CacheThread;
	friend class UITab_RenderThread;
};


class UITab : public PTMutex
{
	public:
	UITab(TestUI &parent,const char *filename);
	~UITab();
	void SetImage(const char *filename);
	static void ColorantsChanged(GtkWidget *wid,gpointer userdata);
	static void deleteclicked(GtkWidget *wid,gpointer userdata);
	static void setclosebuttonsize(GtkWidget *wid,GtkStyle *style,gpointer userdata);
	static gboolean mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata);
	protected:
	TestUI &parent;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *colsel;
	GtkWidget *pbview;
	GtkWidget *popup;
	bool popupshown;
	CachedImage *image;
	DeviceNColorantList *collist;
	friend class UITab_CacheThread;
	friend class UITab_RenderThread;
};


class UITab_CacheThread : public ThreadFunction
{
	public:
	UITab_CacheThread(UITab &tab,ImageSource *src) : ThreadFunction(), tab(tab), thread(this), src(src)
	{
		thread.Start();
		thread.WaitSync();
	}
	~UITab_CacheThread()
	{
	}
	int Entry(Thread &t)
	{
		tab.ObtainMutex();
		thread.SendSync();

		tab.image=new CachedImage(src);
		tab.ReleaseMutex();

		g_timeout_add(1,CleanupFunc,this);
		return(0);
	}
	static gboolean CleanupFunc(gpointer ud)
	{
		UITab_CacheThread *t=(UITab_CacheThread *)ud;
		delete t;
		return(FALSE);
	}
	protected:
	UITab &tab;
	Thread thread;
	ImageSource *src;
};


class UITab_RenderThread : public ThreadFunction
{
	public:
	UITab_RenderThread(UITab &tab) : ThreadFunction(), tab(tab), thread(this), pixbuf(NULL)
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
	UITab &tab;
	Thread thread;
	ImageSource *src;
	GdkPixbuf *pixbuf;
};


UITab::UITab(TestUI &parent,const char *filename) : PTMutex(), parent(parent), colsel(NULL), pbview(NULL), image(NULL), collist(NULL)
{
	label=gtk_hbox_new(FALSE,0);
	char *fn=SafeStrdup(filename);
	GtkWidget *tmp=gtk_label_new(basename(fn));
	free(fn);
	gtk_box_pack_start(GTK_BOX(label),tmp,TRUE,TRUE,0);

	GtkWidget *closeimg=gtk_image_new_from_stock(GTK_STOCK_CLOSE,GTK_ICON_SIZE_MENU);
	tmp=gtk_button_new();
	gtk_widget_set_name(tmp,"tab-close-button");
	gtk_button_set_image(GTK_BUTTON(tmp),closeimg);
	gtk_button_set_relief(GTK_BUTTON(tmp),GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(deleteclicked),this);
	g_signal_connect(G_OBJECT(tmp),"style-set",G_CALLBACK(setclosebuttonsize),this);
	gtk_box_pack_start(GTK_BOX(label),tmp,FALSE,FALSE,4);
	gtk_widget_show_all(label);
	
	hbox=gtk_hbox_new(FALSE,0);
	gtk_widget_show(GTK_WIDGET(hbox));
	g_signal_connect(G_OBJECT(hbox),"motion-notify-event",G_CALLBACK(mousemove),this);
	gtk_notebook_append_page(GTK_NOTEBOOK(parent.notebook),GTK_WIDGET(hbox),label);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(parent.notebook),-1);
    gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK(parent.notebook),hbox, TRUE, TRUE,GTK_PACK_START);

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

	char *path;
	char *pathrev;
	unsigned int pathlen;

	gtk_widget_path(tmp,&pathlen,&path,&pathrev);
	cerr << "Path to button: " << path << endl;

	SetImage(filename);
}

UITab::~UITab()
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


void UITab::SetImage(const char *filename)
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
		ImageSource *is=ISLoadImage(filename);

		CMYKConversionOptions opts;
		char *p;
		if(STRIP_ALPHA(is->type)==IS_TYPE_RGB)
		{
			if((p=parent.profilemanager.SearchPaths("sRGB Color Space Profile.icm")));
			{
				opts.SetInProfile(p);
				free(p);
			}
		}
		else
		{
			if((p=parent.profilemanager.SearchPaths("USWebCoatedSWOP.icc")));
			{
				opts.SetInProfile(p);
				free(p);
			}
		}
		if((p=parent.profilemanager.SearchPaths("USWebCoatedSWOP.icc")));
		{
			opts.SetOutProfile(p);
			free(p);
		}

		is=opts.Apply(is);

		collist=new DeviceNColorantList(is->type);
		colorantselector_set_colorants(COLORANTSELECTOR(colsel),collist);

		new UITab_CacheThread(*this,is);
		new UITab_RenderThread(*this);
	}
	catch (const char *err)
	{
		ErrorMessage_Dialog(err,parent.window);
	}
}


void UITab::ColorantsChanged(GtkWidget *wid,gpointer userdata)
{
	UITab *ob=(UITab *)userdata;
	new UITab_RenderThread(*ob);
}


void UITab::deleteclicked(GtkWidget *wid,gpointer userdata)
{
	UITab *ui=(UITab *)userdata;
	// Disable the close button here in case it gets clicked again while we're
	// waiting for thread termination.
	gtk_widget_set_sensitive(wid,FALSE);
	delete ui;
}


void UITab::setclosebuttonsize(GtkWidget *wid,GtkStyle *style,gpointer userdata)
{
	UITab *ui=(UITab *)userdata;
	GtkSettings *settings=gtk_widget_get_settings(wid);
	int x,y;
	gtk_icon_size_lookup_for_settings(settings,GTK_ICON_SIZE_MENU,&x,&y);
	gtk_widget_set_size_request(wid,x+2,y+2);
}


gboolean UITab::mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata)
{
	UITab *ui=(UITab *)userdata;

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
	factory(profilemanager), notebook(NULL)
{
	profilemanager.SetInt("DefaultCMYKProfileActive",1);

	gtk_rc_parse_string("style \"mystyle\"\n"
						"{\n"
						"	GtkWidget::focus-padding = 0\n"
						"	GtkWidget::focus-line-width = 0\n"
						"	xthickness = 0\n"
						"	ythickness = 0\n"
						"}\n"
						"widget \"*.tab-close-button\" style \"mystyle\"\n");


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
		new UITab(*ui,fn);
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

