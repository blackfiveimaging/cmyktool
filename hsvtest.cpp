#include <iostream>

#include <gtk/gtk.h>

#include "imagesource.h"
#include "imagesource_util.h"
#include "pixbuf_from_imagesource.h"
#include "pixbufview.h"
#include "profilemanager.h"

#include "rspl/rspl.h"

#include "config.h"
#define _(x) gettext(x)

using namespace std;


class RSPLWrapper
{
	public:
	RSPLWrapper(int ind, int outd) : interp(NULL), ind(ind), outd(outd)
	{
		interp=new_rspl(0,ind,outd); // Create a new RSPL structure
		for(int i=0;i<ind;++i)
		{
			gres[i]=16;
			avgdev[i]=0.1;
		}
	}
	virtual ~RSPLWrapper()
	{
		if(interp)
			interp->del(interp);
	}
	virtual bool Populate(int count, double *in, double *out,double smoothing)
	{
		cow *myco=new cow[count];
		datai ilow,ihigh;
		datao olow,ohigh;

		for(int i=0;i<ind;++i)
		{
			ilow[i]=ihigh[i]=in[i];
		}
		for(int i=0;i<outd;++i)
		{
			olow[i]=ohigh[i]=out[i];
		}

		for(int i=0;i<count;++i)
		{
			for(int j=0;j<ind;++j)
			{
				myco[i].p[j]=in[i*ind+j];
				if(in[i*ind+j]<ilow[j]) ilow[j]=in[i*ind+j];
				if(in[i*ind+j]>ihigh[j]) ihigh[j]=in[i*ind+j];
			}
			for(int j=0;j<outd;++j)
			{
				myco[i].v[j]=out[i*outd+j];
				if(out[i*outd+j]<olow[j]) olow[j]=out[i*outd+j];
				if(out[i*outd+j]>ohigh[j]) ohigh[j]=out[i*outd+j];
			}
			myco[i].w=1.0;
		}
		// Increase weights of endpoints...
		myco[0].w=10.0;
		myco[count-1].w=10.0;

		// Add some "headroom"
		for(int j=0;j<ind;++j)
		{
			ilow[j]-=1.0;
			ihigh[j]+=1.0;
		}

		for(int j=0;j<outd;++j)
		{
			olow[j]-=1.0;
			ohigh[j]+=1.0;
		}

		interp->fit_rspl_w(interp,
		           RSPL_EXTRAFIT,		/* Non-mon and clip flags */
		           myco,				/* Test points */
		           count,				/* Number of test points */
		           ilow, ihigh, gres,	/* Low, high, resolution of grid */
		           olow, ohigh,			/* Default data scale */
		           smoothing,			/* Smoothing */
		           avgdev,				/* Average deviation */
		           NULL);				/* iwidth */
		delete[] myco;
		return(true);
	}

	virtual void Interp(double *in, double *out)
	{
		co point;
		for(int i=0;i<ind;++i)
			point.p[i]=*in++;
		for(int i=0;i<outd;++i)
			point.v[i]=0.0;
		interp->interp(interp,&point);
		for(int i=0;i<outd;++i)
			*out++=point.v[i];
	}

	virtual bool ReverseInterp(double *out, double *in)
	{
		co point;
		for(int i=0;i<ind;++i)
			point.p[i]=0;
		for(int i=0;i<outd;++i)
			point.v[i]=*out++;
		int result=interp->rev_interp(interp,RSPL_WILLCLIP|RSPL_NEARCLIP,1,NULL,NULL,&point);
		for(int i=0;i<outd;++i)
			*in++=point.p[i];
		if(result==0)
		{
			Debug[TRACE] << "ReverseInterp: no solutions found" << endl;
			return(false);
		}
		return(true);
	}
	protected:
	rspl *interp;
	int ind,outd;
	int gres[MXDI];
	double avgdev[MXDO];
};


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
					rgbout[idx+1]=double(in[1])/IS_SAMPLEMAX-0.5;
					rgbout[idx+2]=double(in[2])/IS_SAMPLEMAX-0.5;
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


class ImageSource_HSV : public ImageSource
{
	public:
	ImageSource_HSV(ImageSource *src) : ImageSource(src), source(src)
	{
		Debug[TRACE] << "Source image type " << source->type << std::endl;
		switch(source->type)
		{
			case IS_TYPE_RGB:
				type=IS_TYPE_HSV;
				break;
			case IS_TYPE_RGBA:
				type=IS_TYPE_HSVA;
				break;
			default:
				throw "HSV conversion can only take RGB(A) input";
				break;
		}
		Debug[TRACE] << "HSV image type " << type << std::endl;
		MakeRowBuffer();
	}
	~ImageSource_HSV()
	{
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(row==currentrow)
			return(rowbuffer);
		ISDataType *src=source->GetRow(row);

		switch(type)
		{
			case IS_TYPE_HSV:
				for(int x=0;x<width;++x)
				{
					int r=src[3*x];
					int g=src[3*x+1];
					int b=src[3*x+2];
					int M=r;
					if(M<g) M=g;
					if(M<b) M=b;

					int m=r;
					if(m>g) m=g;
					if(m>b) m=b;

					int c=M-m;
					int h=0;
					if(c)
					{
						if(M==r)
							h=(((g-b)*8192)/c);
						if(M==g)
							h=(((b-r)*8192)/c)+16384;
						if(M==b)
							h=(((r-g)*8192)/c)+32768;
						if(h<0)
							h+=(6*8192);
					}

					int s=0;
					if(M>4)
						s=((IS_SAMPLEMAX/4)*c)/(M/4);
					rowbuffer[3*x]=h;
					rowbuffer[3*x+1]=s;
					rowbuffer[3*x+2]=M;
				}
				break;
			case IS_TYPE_HSVA:
				for(int x=0;x<width;++x)
				{
					int r=src[4*x];
					int g=src[4*x+1];
					int b=src[4*x+2];
					int a=0;
					int M=r;
					if(M<g) M=g;
					if(M<b) M=b;

					int m=r;
					if(m>g) m=g;
					if(m>b) m=b;

					int c=M-m;
					int h=0;
					if(c)
					{
						if(M==r)
							h=(((g-b)*8192)/c);
						if(M==g)
							h=(((b-r)*8192)/c)+16384;
						if(M==b)
							h=(((r-g)*8192)/c)+32768;
						if(h<0)
							h+=(6*8192);
					}

					int s=0;
					if(M>4)
						s=((IS_SAMPLEMAX/4)*c)/(M/4);
					rowbuffer[4*x]=h;
					rowbuffer[4*x+1]=s;
					rowbuffer[4*x+2]=M;
					rowbuffer[4*x+3]=a;
				}
				break;
			default:
				throw "Bad type - how did that happen?";
				break;
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
};


class ImageSource_HSVToRGB : public ImageSource
{
	public:
	ImageSource_HSVToRGB(ImageSource *src) : ImageSource(src), source(src)
	{
		switch(source->type)
		{
			case IS_TYPE_HSV:
				type=IS_TYPE_RGB;
				break;
			case IS_TYPE_HSVA:
				type=IS_TYPE_RGBA;
				break;
			default:
				throw "HSV conversion only works with HSV or HSVA input!";
				break;
		}
		MakeRowBuffer();
	}
	~ImageSource_HSVToRGB()
	{
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(row==currentrow)
			return(rowbuffer);
		ISDataType *src=source->GetRow(row);

		switch(source->type)
		{
			case IS_TYPE_HSV:
				for(int x=0;x<width;++x)
				{
					int h=src[3*x];
					int s=src[3*x+1];
					int v=src[3*x+2];

					int c=((s/2)*(v/2))/(IS_SAMPLEMAX/4);

					int r=0;
					int g=0;
					int b=0;
					int t=(h % 0x4000) - 0x2000;
					if(t<0)
						t=-t;
					t=(c*(0x2000-t))/0x2000;

					if(h<0x2000)
					{
						r=c; g=t; b=0;
					}
					else if(h<0x4000)
					{
						r=t; g=c; b=0;
					}
					else if(h<0x6000)
					{
						r=0; g=c; b=t;
					}
					else if(h<0x8000)
					{
						r=0; g=t; b=c;
					}
					else if(h<0xa000)
					{
						r=t; g=0; b=c;
					}
					else if(h<0xc000)
					{
						r=c; g=0; b=t;
					}
					r+=v-c;
					g+=v-c;
					b+=v-c;
					if(r>IS_SAMPLEMAX) r=IS_SAMPLEMAX;
					if(r<0) r=0;
					if(g>IS_SAMPLEMAX) g=IS_SAMPLEMAX;
					if(g<0) g=0;
					if(b>IS_SAMPLEMAX) b=IS_SAMPLEMAX;
					if(b<0) b=0;
					rowbuffer[3*x]=r;
					rowbuffer[3*x+1]=g;
					rowbuffer[3*x+2]=b;
				}
				break;
			case IS_TYPE_HSVA:
				for(int x=0;x<width;++x)
				{
					int h=src[4*x];
					int s=src[4*x+1];
					int v=src[4*x+2];
					int a=src[4*x+3];

					int c=((s/2)*(v/2))/(IS_SAMPLEMAX/4);

					int r=0;
					int g=0;
					int b=0;
					int t=(h % 0x4000) - 0x2000;
					if(t<0)
						t=-t;
					t=(c*(0x2000-t))/0x2000;

					if(h<0x2000)
					{
						r=c; g=t; b=0;
					}
					else if(h<0x4000)
					{
						r=t; g=c; b=0;
					}
					else if(h<0x6000)
					{
						r=0; g=c; b=t;
					}
					else if(h<0x8000)
					{
						r=0; g=t; b=c;
					}
					else if(h<0xa000)
					{
						r=t; g=0; b=c;
					}
					else if(h<0xc000)
					{
						r=c; g=0; b=t;
					}
					r+=v-c;
					g+=v-c;
					b+=v-c;
					if(r>IS_SAMPLEMAX) r=IS_SAMPLEMAX;
					if(r<0) r=0;
					if(g>IS_SAMPLEMAX) g=IS_SAMPLEMAX;
					if(g<0) g=0;
					if(b>IS_SAMPLEMAX) b=IS_SAMPLEMAX;
					if(b<0) b=0;
					rowbuffer[4*x]=r;
					rowbuffer[4*x+1]=g;
					rowbuffer[4*x+2]=b;
					rowbuffer[4*x+3]=a;
				}
				break;
			default:
				throw "Bad type - how did that happen?";
				break;
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
};


class ImageSource_HueRotate : public ImageSource
{
	public:
	ImageSource_HueRotate(ImageSource *src,int degrees) : ImageSource(src), source(src), offset(0)
	{
		if(STRIP_ALPHA(type)!=IS_TYPE_HSV)
			throw "Hue rotation only works with HSV data!";
		offset=(0xc000*degrees)/360;
		MakeRowBuffer();
	}
	~ImageSource_HueRotate()
	{
		if(source)
			delete source;
	}
	ISDataType *GetRow(int row)
	{
		if(row==currentrow)
			return(rowbuffer);
		ISDataType *src=source->GetRow(row);
		switch(type)
		{
			case IS_TYPE_HSV:
				for(int x=0;x<width;++x)
				{
					int h=src[x*samplesperpixel];
					int s=src[x*samplesperpixel+1];
					int v=src[x*samplesperpixel+2];
					h+=offset;
					if(h>=0xc000)
						h-=0xc000;
					rowbuffer[x*samplesperpixel]=h;
					rowbuffer[x*samplesperpixel+1]=s;
					rowbuffer[x*samplesperpixel+2]=v;
				}
				break;
			case IS_TYPE_HSVA:
				for(int x=0;x<width;++x)
				{
					int h=src[x*samplesperpixel];
					int s=src[x*samplesperpixel+1];
					int v=src[x*samplesperpixel+2];
					int a=src[x*samplesperpixel+3];
					h+=offset;
					if(h>=0xc000)
						h-=0xc000;
					rowbuffer[x*samplesperpixel]=h;
					rowbuffer[x*samplesperpixel+1]=s;
					rowbuffer[x*samplesperpixel+2]=v;
					rowbuffer[x*samplesperpixel+3]=a;
				}
				break;
				break;
			default:
				throw "ImageSource_HueRotate: Bad image type!";
				break;
		}
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
	int offset;
};

int main(int argc,char **argv)
{
	Debug.SetLevel(TRACE);

	gtk_init(&argc,&argv);

	try
	{
		GtkWidget *win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (win), _("PixBufView Test"));
		gtk_signal_connect (GTK_OBJECT (win), "delete_event",
			(GtkSignalFunc) gtk_main_quit, NULL);

		GtkWidget *vbox=gtk_vbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(win),vbox);
		gtk_widget_show(GTK_WIDGET(vbox));

		GtkWidget *pview=pixbufview_new(NULL,false);
//		g_signal_connect(G_OBJECT(pview),"mousemove",G_CALLBACK(mouse_move),pview);

		gtk_box_pack_start(GTK_BOX(vbox),pview,TRUE,TRUE,0);
		gtk_widget_show(pview);
		gtk_widget_show(win);

//		SimpleComboOptions opts;

		for(int i=1;i<argc;++i)
		{
			ImageSource *is=ISLoadImage(argv[i]);
			is=new ImageSource_HSV(is);
			is=new ImageSource_HueRotate(is,60);
			is=new ImageSource_HSVToRGB(is);
//			is=new ImageSource_HReflect(is);
			GdkPixbuf *pb=pixbuf_from_imagesource(is);
			delete is;

			pixbufview_add_page(PIXBUFVIEW(pview),pb);
			g_object_unref(G_OBJECT(pb));

//			opts.Add("",argv[i]);
		}

//		GtkWidget *combo=simplecombo_new(opts);
//		g_signal_connect(G_OBJECT(combo),"changed",G_CALLBACK(change_page),pview);
//		gtk_box_pack_start(GTK_BOX(vbox),combo,FALSE,FALSE,0);
//		gtk_widget_show(combo);

		gtk_main();

	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}

	return(0);
}
