#include <iostream>

#include <gtk/gtk.h>

#include "imagesource.h"
#include "imagesource_util.h"
#include "imagesource_hsv.h"
#include "imagesource_hsvtorgb.h"
#include "imagesource_huerotate.h"
#include "imagesource_flatten.h"
#include "devicencolorant.h"
#include "imagesource_cms.h"
#include "imagesource_devicen_preview.h"

#include "cachedimage.h"

#include "pixbuf_from_imagesource.h"
#include "pixbufview.h"
#include "profilemanager.h"

#include "rsplwrapper.h"

#include "config.h"
#define _(x) gettext(x)

using namespace std;


class RGBToCMY : public RSPLWrapper
{
	public:
	RGBToCMY(CMSProfile *cmykprofile) : RSPLWrapper(3,3)
	{
		CMSProfile rgb;

		CMSTransform trans(cmykprofile,&rgb);

		// Create a 6x6x6 grid.
		int dim=6;
		int count=dim*dim*dim;
		double cmyin[count*3];
		double rgbout[count*3];
		for(int c=0;c<dim;++c)
		{
			for(int m=0;m<dim;++m)
			{
				for(int y=0;y<dim;++y)
				{
					ISDataType cmyk[4];
					cmyk[0]=(c*IS_SAMPLEMAX)/(dim-1);
					cmyk[1]=(m*IS_SAMPLEMAX)/(dim-1);
					cmyk[2]=(y*IS_SAMPLEMAX)/(dim-1);
					cmyk[3]=0;
					ISDataType in[3];
					trans.Transform(cmyk,in,1);
					int idx=((c*dim+m)*dim+y)*3;
					cmyin[idx]=double(cmyk[0])/double(IS_SAMPLEMAX);
					cmyin[idx+1]=double(cmyk[1])/IS_SAMPLEMAX;
					cmyin[idx+2]=double(cmyk[2])/IS_SAMPLEMAX;
					rgbout[idx]=double(in[0])/IS_SAMPLEMAX;
					rgbout[idx+1]=double(in[1])/IS_SAMPLEMAX;
					rgbout[idx+2]=double(in[2])/IS_SAMPLEMAX;
					Debug[TRACE] << cmyin[idx] << ", " << cmyin[idx+1] << ", " << cmyin[idx+2] << " -> " <<
						rgbout[idx] << ", " << rgbout[idx+1] << ", " << rgbout[idx+2] << endl;
				}
			}
		}
		Populate(count,cmyin,rgbout,0.05);
	}
	virtual ~RGBToCMY()
	{
	}
};


class ImageSource_DuoTone : public ImageSource
{
	public:
	ImageSource_DuoTone(ImageSource *src) : ImageSource(src), source(src)
	{
		if(STRIP_ALPHA(source->type)!=IS_TYPE_RGB)
			throw "DuoTone only supports RGB input!";
		if(HAS_ALPHA(source->type))
			source=new ImageSource_Flatten(source);

		type=IS_TYPE_DEVICEN;
		samplesperpixel=2;

		MakeRowBuffer();
	}
	~ImageSource_DuoTone()
	{
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(currentrow==row)
			return(rowbuffer);

		ISDataType *src=source->GetRow(row);
		for(int x=0;x<width;++x)
		{
			int r=src[x*3];
			int g=src[x*3+1];
			int b=src[x*3+2];
			int t=(g+b)/2;

			if(r>t)
				g=t;
			else
				r=g=(r + g + b)/3;

			g=r-g;

			rowbuffer[x*2]=g;
			rowbuffer[x*2+1]=IS_SAMPLEMAX-r;
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
};


class ImageSource_DuoTone_Preview : public ImageSource
{
	public:
	ImageSource_DuoTone_Preview(ImageSource *source,ISDeviceNValue &col1,ISDeviceNValue &col2) : ImageSource(source), source(source),col1(col1),col2(col2)
	{
		type=IS_TYPE_CMYK;
		samplesperpixel=4;
		MakeRowBuffer();
	}
	~ImageSource_DuoTone_Preview()
	{
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(currentrow==row)
			return(rowbuffer);

		ISDataType *src=source->GetRow(row);
		for(int x=0;x<width;++x)
		{
			int a=src[x*2]/4;
			int b=src[x*2+1]/4;
			int c=(col1[0]*a)/(IS_SAMPLEMAX/4);
			int m=(col1[1]*a)/(IS_SAMPLEMAX/4);
			int y=(col1[2]*a)/(IS_SAMPLEMAX/4);
			int k=(col2[3]*b)/(IS_SAMPLEMAX/4);
			rowbuffer[x*4]=c;
			rowbuffer[x*4+1]=m;
			rowbuffer[x*4+2]=y;
			rowbuffer[x*4+3]=k;
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
	ISDeviceNValue &col1;
	ISDeviceNValue &col2;
};


class GUI : public ConfigFile
{
	public:
	GUI() : ConfigFile(), profilemanager(this,"[Colour Management]"), pview(NULL), imposter(NULL), colorants(),
		cmypreview(4,0), kpreview(4,0), transform(NULL)
	{
		profilemanager.SetInt("DefaultCMYKProfileActive",1);
		CMSProfile *prof=profilemanager.GetDefaultProfile(IS_TYPE_CMYK);
		if(!prof)
			throw "Can't open default CMYK profile";
		CMSProfile *monitorprof=profilemanager.GetProfile(CM_COLOURDEVICE_DISPLAY);
		if(!monitorprof)
			monitorprof=profilemanager.GetProfile(CM_COLOURDEVICE_DEFAULTRGB);
		if(monitorprof)
		{
			transform=new CMSTransform(prof,monitorprof);
			delete monitorprof;
		}
		else
			throw "Can't get profile for display!";
		delete prof;

		cmypreview[0]=0;
		cmypreview[1]=IS_SAMPLEMAX;
		cmypreview[2]=(IS_SAMPLEMAX*5)/6;
		cmypreview[3]=0;
		kpreview[3]=IS_SAMPLEMAX;

		Debug[TRACE] << "CMY preview: " << cmypreview[0] << ", " << cmypreview[1] << ", " << cmypreview[2] << std::endl;

		new DeviceNColorant(colorants,"Red");
		new DeviceNColorant(colorants,"Black");

		GtkWidget *win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (win), _("PixBufView Test"));
		gtk_signal_connect (GTK_OBJECT (win), "delete_event",
			(GtkSignalFunc) gtk_main_quit, NULL);

		GtkWidget *vbox=gtk_vbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(win),vbox);
		gtk_widget_show(GTK_WIDGET(vbox));

		pview=pixbufview_new(NULL,false);

		gtk_box_pack_start(GTK_BOX(vbox),pview,TRUE,TRUE,0);
		gtk_widget_show(pview);
		gtk_widget_show(win);

		rotation=gtk_hscale_new_with_range(0.0,359.0,1.0);
		g_signal_connect(G_OBJECT(rotation),"value-changed",G_CALLBACK(slider_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),rotation,FALSE,FALSE,0);
		gtk_widget_show(rotation);

	}
	~GUI()
	{
		if(imposter)
			delete imposter;
		if(transform)
			delete transform;
	}
	void SetImage(std::string fn)
	{
		if(imposter)
			delete imposter;
		imposter=NULL;

		filename=fn;
		ImageSource *is=ISLoadImage(fn.c_str());
		int w=256;
		int h=(256*is->height)/is->width;
		if(h>256)
		{
			h=256;
			w=(256*is->width)/is->height;
		}
		is=ISScaleImageBySize(is,w,h);
		imposter=new CachedImage(is);
		Update();
	}
	void Update()
	{
		int r=gtk_range_get_value(GTK_RANGE(rotation));
		ImageSource *is=imposter->GetImageSource();
		is=Process(is,r);

		GdkPixbuf *pb=pixbuf_from_imagesource(is);
		delete is;

		pixbufview_set_pixbuf(PIXBUFVIEW(pview),pb);
		g_object_unref(G_OBJECT(pb));
	}
	ImageSource *Process(ImageSource *src, int rotation)
	{
		src=new ImageSource_HSV(src);
		src=new ImageSource_HueRotate(src,rotation);
		src=new ImageSource_HSVToRGB(src);
		src=new ImageSource_DuoTone(src);
//		src=new ImageSource_DeviceN_Preview(src,&colorants);
		src=new ImageSource_DuoTone_Preview(src,cmypreview,kpreview);
		src=new ImageSource_CMS(src,transform);
		return(src);
	}
	static void slider_changed(GtkWidget *wid,gpointer ud)
	{
		GUI *gui=(GUI *)ud;
		gui->Update();
	}
	protected:
	ProfileManager profilemanager;
	GtkWidget *window;
	GtkWidget *pview;
	GtkWidget *rotation;
	CachedImage *imposter;
	DeviceNColorantList colorants;
	ISDeviceNValue cmypreview;
	ISDeviceNValue kpreview;
	CMSTransform *transform;
	std::string filename;
};


int main(int argc,char**argv)
{
	Debug.SetLevel(TRACE);

	gtk_init(&argc,&argv);

	try
	{
		GUI test;
		if(argc>1)
			test.SetImage(argv[1]);

		gtk_main();
	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}

	return(0);
}

