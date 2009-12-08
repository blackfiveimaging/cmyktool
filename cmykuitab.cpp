#include <iostream>

#include <libgen.h>
#include <gtk/gtk.h>

#include "support/debug.h"
#include "support/jobqueue.h"
#include "support/rwmutex.h"
#include "support/thread.h"
#include "support/util.h"

#include "imageutils/tiffsave.h"
#include "imagesource/imagesource_util.h"
#include "imagesource/imagesource_cms.h"
#include "imagesource/pixbuf_from_imagesource.h"
#include "cachedimage.h"

#include "miscwidgets/pixbufview.h"
#include "miscwidgets/coloranttoggle.h"
#include "miscwidgets/imageselector.h"
#include "miscwidgets/generaldialogs.h"
#include "miscwidgets/errordialogqueue.h"
#include "miscwidgets/uitab.h"
#include "progressbar.h"

#include "profilemanager/profilemanager.h"


#include "conversionopts.h"
#include "cmtransformworker.h"

#include "cmykuitab.h"

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


///////////////////////  User interface /////////////////////////



class UITab_RenderJob : public Job, public ThreadSync, public Progress
{
	public:
	UITab_RenderJob(CMYKUITab &tab) : Job(), ThreadSync(), Progress(), tab(tab), pixbuf(NULL)
	{
		tab.Ref();
	}
	~UITab_RenderJob()
	{
	}
	void Run(Worker *w)
	{
		CMTransformWorker *cw=(CMTransformWorker *)w;

		try
		{
			ImageSource *is=tab.image->GetImageSource();
			is=new ImageSource_ColorantMask(is,tab.collist);

			CMSTransform *transform=cw->factory->GetTransform(CM_COLOURDEVICE_DISPLAY,is);
			if(transform)
				is=new ImageSource_CMS(is,transform);
			else
				Debug[WARN] << "RenderJob: Couldn't create transform" << endl;

			pixbuf=pixbuf_from_imagesource(is,255,255,255,this);
			delete is;
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
			ErrorDialogs.AddMessage(err);
		}
		if(pixbuf)
		{
			Debug[TRACE] << "Triggering cleanup function" << endl;
			g_timeout_add(1,CleanupFunc,this);
			WaitCondition();
		}
		else
		{
			tab.UnRef();
		}
	}
	static gboolean CleanupFunc(gpointer ud)
	{
		UITab_RenderJob *t=(UITab_RenderJob *)ud;

		Debug[TRACE] << "In cleanup function - drawing pixbuf" << endl;

		if(t->pixbuf)
		{
			pixbufview_set_pixbuf(PIXBUFVIEW(t->tab.pbview),t->pixbuf);
			g_object_unref(G_OBJECT(t->pixbuf));
			t->pixbuf=NULL;
		}
//		gtk_widget_set_sensitive(t->tab.colsel,TRUE);
		t->tab.UnRef();
		t->Broadcast();
		return(FALSE);
	}
	bool DoProgress(int i, int maxi)
	{
		return(GetJobStatus()!=JOBSTATUS_CANCELLED);
	}
	protected:
	CMYKUITab &tab;
	ImageSource *src;
	GdkPixbuf *pixbuf;
};


class UITab_CacheJob : public Job
{
	public:
	UITab_CacheJob(CMYKUITab &tab,const char *filename) : Job(), tab(tab), filename(NULL)
	{
		tab.Ref();
		this->filename=SafeStrdup(filename);
	}
	~UITab_CacheJob()
	{
		free(filename);
	}
	void Run(Worker *w)
	{
		CMTransformWorker *cw=(CMTransformWorker *)w;

		try
		{
			ImageSource *is=ISLoadImage(filename);

			is=tab.convopts.Apply(is,NULL,cw->factory);

			tab.image=new CachedImage(is);

			UITab_RenderJob renderjob(tab);
			renderjob.Run(w);
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
			ErrorDialogs.AddMessage(err);
			tab.UnRef();	// If we encountered an error, delete the tab.
		}
		tab.UnRef();	// Release the reference obtained when the job was created.
	}
	protected:
	CMYKUITab &tab;
	char *filename;
};


////// CMYKUITab member functions ///////


CMYKUITab::	CMYKUITab(GtkWidget *parent,GtkWidget *notebook,CMYKConversionOptions &opts,JobDispatcher &dispatcher,const char *filename)
	: UITab(notebook),  parent(parent), dispatcher(dispatcher), colsel(NULL), pbview(NULL), image(NULL), collist(NULL),
	convopts(opts), filename(NULL)
{
	hbox=GetBox();
	g_signal_connect(G_OBJECT(hbox),"motion-notify-event",G_CALLBACK(mousemove),this);

	GtkWidget *vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,0);
	gtk_widget_show(vbox);

	pbview=pixbufview_new(NULL,false);
	gtk_box_pack_start(GTK_BOX(vbox),pbview,TRUE,TRUE,0);
	gtk_widget_show(pbview);

	GtkWidget *hbox2=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(vbox),hbox2,FALSE,FALSE,0);
	gtk_widget_show(hbox2);

//	colsel=coloranttoggle_new(NULL);
//	gtk_signal_connect (GTK_OBJECT (colsel), "changed",
//		(GtkSignalFunc) ColorantsChanged, this);
//	gtk_box_pack_start(GTK_BOX(hbox2),colsel,FALSE,FALSE,0);
//	gtk_widget_show(colsel);

	GtkWidget *tmp=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox2),tmp,TRUE,TRUE,0);
	gtk_widget_show(tmp);


	GtkWidget *savebutton=gtk_button_new_with_label(_("Save..."));
	g_signal_connect(G_OBJECT(savebutton),"clicked",G_CALLBACK(Save),this);
	gtk_box_pack_start(GTK_BOX(hbox2),savebutton,FALSE,FALSE,0);
	gtk_widget_show(savebutton);
	
	popupshown=false;
	popup=gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_default_size(GTK_WINDOW(popup),10,10);
	gtk_window_set_transient_for(GTK_WINDOW(popup),GTK_WINDOW(parent));

	colsel=coloranttoggle_new(NULL);
	gtk_signal_connect (GTK_OBJECT (colsel), "changed",
		(GtkSignalFunc) ColorantsChanged, this);
	gtk_container_add(GTK_CONTAINER(popup),colsel);
	gtk_widget_show(colsel);

	SetImage(filename);
}


CMYKUITab::~CMYKUITab()
{
	if(collist)
		delete collist;
	if(image)
		delete image;
	if(filename)
		free(filename);
}


void CMYKUITab::SetImage(const char *fname)
{
	if(image)
		delete image;
	image=NULL;
	if(collist)
		delete collist;
	collist=NULL;
	if(filename)
		free(filename);
	filename=NULL;

	try
	{
		filename=SafeStrdup(fname);

		collist=new DeviceNColorantList(IS_TYPE_CMYK);
		coloranttoggle_set_colorants(COLORANTTOGGLE(colsel),collist);

		char *fn=SafeStrdup(fname);
		SetText(basename(fn));
		free(fn);

		dispatcher.AddJob(new UITab_CacheJob(*this,filename));
	}
	catch (const char *err)
	{
		ErrorDialogs.AddMessage(err);
	}
}


void CMYKUITab::ColorantsChanged(GtkWidget *wid,gpointer userdata)
{
	CMYKUITab *ob=(CMYKUITab *)userdata;
//	gtk_widget_set_sensitive(ob->colsel,FALSE);

	// Cancel existing render job.  This is harmless if the job's completed already,
	// even if the object's been deleted
	if(ob->renderjob)
		ob->dispatcher.CancelJob(ob->renderjob);

	ob->dispatcher.AddJob(ob->renderjob=new UITab_RenderJob(*ob));
}


void CMYKUITab::Save(GtkWidget *widget,gpointer userdata)
{
	CMYKUITab *ob=(CMYKUITab *)userdata;
	char *tmpfn=BuildFilename(ob->filename,"-CMYK","tif");	
	char *newfn=File_Save_Dialog(_("Save image - please choose filename"),tmpfn,ob->parent);
	if(newfn)
	{
		ProgressBar p(_("Saving"),true,ob->parent);
		ImageSource *is=ob->image->GetImageSource();
		TIFFSaver t(newfn,is);
		t.SetProgress(&p);
		t.Save();
		delete is;
		free(newfn);
	}
	free(tmpfn);
}


gboolean CMYKUITab::mousemove(GtkWidget *widget,GdkEventMotion *event, gpointer userdata)
{
	CMYKUITab *ui=(CMYKUITab *)userdata;

	int x,y;
	GdkModifierType mods;
	gdk_window_get_pointer (widget->window, &x, &y, &mods);
	x-=widget->allocation.x;
	y-=widget->allocation.y;
	int w,h;
//	gtk_window_get_size(GTK_WINDOW(ui->parent),&w,&h);
	w=widget->allocation.width;
	h=widget->allocation.height;

	if(ui->popupshown && (x>(w/2) || y<((7*h)/8)))
	{
		gtk_widget_hide_all(ui->popup);
		ui->popupshown=false;
	}

	if(!ui->popupshown && x<(w/2) && y>((7*h)/8))
	{
		int winx,winy;
		int pw,ph;
		gtk_window_get_position(GTK_WINDOW(ui->parent),&winx,&winy);
		gtk_window_get_size(GTK_WINDOW(ui->popup),&pw,&ph);
		gtk_window_move(GTK_WINDOW(ui->popup),winx+widget->allocation.x,winy+widget->allocation.y+h-ph);
		gtk_widget_show_all(ui->popup);
		ui->popupshown=true;
	}

	return(FALSE);
}


