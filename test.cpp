#include <iostream>

#include <gtk/gtk.h>

#include "support/progresstext.h"
#include "support/rwmutex.h"
#include "support/thread.h"
#include "imageutils/tiffsave.h"
#include "imagesource/imagesource_util.h"
#include "imagesource/imagesource_cms.h"
#include "imagesource/pixbuf_from_imagesource.h"
#include "miscwidgets/pixbufview.h"
#include "miscwidgets/colorantselector.h"
#include "profilemanager/profilemanager.h"
#include "cachedimage.h"

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
	protected:
	ProfileManager profilemanager;
	CMTransformFactory factory;
	GtkWidget *window;
	GtkWidget *notebook;
	friend class UITab;
	friend class UITab_CacheThread;
	friend class UITab_RenderThread;
};


class UITab : public RWMutex
{
	public:
	UITab(TestUI &parent,const char *filename);
	~UITab();
	void SetImage(const char *filename);
	void Redraw();
	static void ColorantsChanged(GtkWidget *wid,gpointer userdata);
	static void deleteclicked(GtkWidget *wid,gpointer userdata);
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
	UITab_RenderThread(UITab &tab) : ThreadFunction(), tab(tab), thread(this)
	{
		thread.Start();
	}
	~UITab_RenderThread()
	{
	}
	int Entry(Thread &t)
	{
		// The render thread doesn't need to block until the mutex is owned;
		tab.ObtainMutex();

		ImageSource *is=tab.image->GetImageSource();
		is=new ImageSource_ColorantMask(is,tab.collist);

		CMSTransform *transform=tab.parent.factory.GetTransform(CM_COLOURDEVICE_DISPLAY,is->type);
		if(transform)
			is=new ImageSource_CMS(is,transform);
		else
			cerr << "Couldn't create transform" << endl;

		pixbuf=pixbuf_from_imagesource(is);
		delete is;

		tab.ReleaseMutex();

		g_timeout_add(1,CleanupFunc,this);
		return(0);
	}
	static gboolean CleanupFunc(gpointer ud)
	{
		UITab_RenderThread *t=(UITab_RenderThread *)ud;

		pixbufview_set_pixbuf(PIXBUFVIEW(t->tab.pbview),t->pixbuf);
		g_object_unref(G_OBJECT(t->pixbuf));

		delete t;
		return(FALSE);
	}
	protected:
	UITab &tab;
	Thread thread;
	ImageSource *src;
	GdkPixbuf *pixbuf;
};


UITab::UITab(TestUI &parent,const char *filename) : RWMutex(), parent(parent), colsel(NULL), pbview(NULL), image(NULL), collist(NULL)
{
	label=gtk_hbox_new(FALSE,0);
	GtkWidget *tmp=gtk_label_new(filename);
	gtk_box_pack_start(GTK_BOX(label),tmp,TRUE,TRUE,0);

	GtkWidget *closeimg=gtk_image_new_from_stock(GTK_STOCK_STOP,GTK_ICON_SIZE_MENU);
	tmp=gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(tmp),closeimg);
	gtk_button_set_relief(GTK_BUTTON(tmp),GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(deleteclicked),this);
	gtk_box_pack_start(GTK_BOX(label),tmp,FALSE,FALSE,4);
	gtk_widget_show_all(label);
	
	hbox=gtk_hbox_new(FALSE,0);
	g_signal_connect(G_OBJECT(hbox),"motion-notify-event",G_CALLBACK(mousemove),this);
	gtk_notebook_append_page(GTK_NOTEBOOK(parent.notebook),GTK_WIDGET(hbox),label);
	gtk_widget_show(GTK_WIDGET(hbox));

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

UITab::~UITab()
{
	ObtainMutex();
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

	ImageSource *is=ISLoadImage(filename);

	collist=new DeviceNColorantList(is->type);
	colorantselector_set_colorants(COLORANTSELECTOR(colsel),collist);

	new UITab_CacheThread(*this,is);
	new UITab_RenderThread(*this);
//	ObtainMutex();
//	ReleaseMutex();
//	Redraw();
}

void UITab::Redraw()
{
	new UITab_RenderThread(*this);
#if 0
	ImageSource *is=image->GetImageSource();
	is=new ImageSource_ColorantMask(is,collist);

	CMSTransform *transform=parent.factory.GetTransform(CM_COLOURDEVICE_DISPLAY,is->type);
	if(transform)
		is=new ImageSource_CMS(is,transform);
	else
		cerr << "Couldn't create transform" << endl;

	GdkPixbuf *pb=pixbuf_from_imagesource(is);
	pixbufview_set_pixbuf(PIXBUFVIEW(pbview),pb);
	g_object_unref(G_OBJECT(pb));
	delete is;
#endif
}

void UITab::ColorantsChanged(GtkWidget *wid,gpointer userdata)
{
	UITab *ob=(UITab *)userdata;
	ob->Redraw();
}

void UITab::deleteclicked(GtkWidget *wid,gpointer userdata)
{
	UITab *ui=(UITab *)userdata;
	delete ui;
}

gboolean UITab::mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata)
{
	UITab *ui=(UITab *)userdata;

	int x,y;
	GdkModifierType mods;
	gdk_window_get_pointer (widget->window, &x, &y, &mods);
	int w,h;
	gtk_window_get_size(GTK_WINDOW(ui->parent.window),&w,&h);

	if(ui->popupshown && x<(w-w/20))
	{
		gtk_widget_hide_all(ui->popup);
		ui->popupshown=false;
	}

	if(!ui->popupshown && x>(w-w/20))
	{
		int winx,winy;
		int pw,ph;
		gtk_window_get_position(GTK_WINDOW(ui->parent.window),&winx,&winy);
		gtk_window_get_size(GTK_WINDOW(ui->popup),&pw,&ph);
		gtk_window_move(GTK_WINDOW(ui->popup),winx+w-pw,winy);
		gtk_widget_show_all(ui->popup);
		ui->popupshown=true;
	}

	return(FALSE);
}


TestUI::TestUI() : ConfigFile(), profilemanager(this,"[ColourManagement]"),
	factory(profilemanager), notebook(NULL)
{
	profilemanager.SetInt("DefaultCMYKProfileActive",1);

	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),600,450);
	gtk_window_set_title (GTK_WINDOW (window), _("TestUI"));
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		(GtkSignalFunc) gtk_main_quit, NULL);
	gtk_widget_show(window);

	notebook=gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook),true);
	gtk_container_add(GTK_CONTAINER(window),notebook);
	gtk_widget_show(GTK_WIDGET(notebook));
}

TestUI::~TestUI()
{
}

void TestUI::AddImage(const char *filename)
{
	new UITab(*this,filename);
}


int main(int argc,char **argv)
{
	gtk_init(&argc,&argv);
	try
	{
		TestUI ui;

		for(int i=1;i<argc;++i)
			ui.AddImage(argv[i]);
		cerr << "**********************" << endl;
		cerr << "All images added " << endl;
		cerr << "**********************" << endl;
		gtk_main();

	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	return(0);
}

