#include <iostream>

#include <gtk/gtk.h>

#include "support/progresstext.h"
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


class TestUI : public ConfigFile
{
	public:
	TestUI() : ConfigFile(), profilemanager(this,"[ColourManagement]"), factory(profilemanager),
		colsel(NULL), pbview(NULL), image(NULL), collist(NULL)
	{
		profilemanager.SetInt("DefaultCMYKProfileActive",1);

		window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_default_size(GTK_WINDOW(window),600,450);
		gtk_window_set_title (GTK_WINDOW (window), _("TestUI"));
		gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			(GtkSignalFunc) gtk_main_quit, NULL);
		gtk_widget_show(window);

		GtkWidget *hbox=gtk_hbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(window),hbox);
		gtk_widget_show(GTK_WIDGET(hbox));

		colsel=colorantselector_new(NULL);
		gtk_signal_connect (GTK_OBJECT (colsel), "changed",
			(GtkSignalFunc) ColorantsChanged, this);
		gtk_box_pack_start(GTK_BOX(hbox),colsel,FALSE,FALSE,0);
		gtk_widget_show(colsel);

		pbview=pixbufview_new(NULL,false);
		gtk_box_pack_start(GTK_BOX(hbox),pbview,TRUE,TRUE,0);
		gtk_widget_show(pbview);
	}

	~TestUI()
	{
		if(collist)
			delete collist;
		if(image)
			delete image;
	}

	void SetImage(const char *filename)
	{
		ImageSource *is=ISLoadImage(filename);

		if(collist)
			delete collist;
		collist=new DeviceNColorantList(is->type);
		colorantselector_set_colorants(COLORANTSELECTOR(colsel),collist);

		image=new CachedImage(is);
		Redraw();
	}

	void Redraw()
	{
		ImageSource *is=image->GetImageSource();
		is=new ImageSource_ColorantMask(is,collist);

		CMSTransform *transform=factory.GetTransform(CM_COLOURDEVICE_DISPLAY,is->type);
		if(transform)
			is=new ImageSource_CMS(is,transform);
		else
			cerr << "Couldn't create transform" << endl;

		GdkPixbuf *pb=pixbuf_from_imagesource(is);
		pixbufview_set_pixbuf(PIXBUFVIEW(pbview),pb);
		g_object_unref(G_OBJECT(pb));
		delete is;
	}

	static void ColorantsChanged(GtkWidget *wid,gpointer userdata)
	{
		TestUI *ob=(TestUI *)userdata;
		ob->Redraw();
	}
	protected:
	ProfileManager profilemanager;
	CMTransformFactory factory;
	GtkWidget *window;
	GtkWidget *colsel;
	GtkWidget *pbview;
	CachedImage *image;
	DeviceNColorantList *collist;
};


int main(int argc,char **argv)
{
	gtk_init(&argc,&argv);
	try
	{
		TestUI ui;

		if(argc>1)
			ui.SetImage(argv[1]);

		gtk_main();

	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	return(0);
}

