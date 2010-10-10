#include <iostream>

#include <gtk/gtk.h>

#include "errordialogqueue.h"
#include "pathsupport.h"

#include "imagesource.h"
#include "imagesource_util.h"
#include "imagesource_hsv.h"
#include "imagesource_hsvtorgb.h"
#include "imagesource_huerotate.h"
#include "imagesource_flatten.h"
#include "devicencolorant.h"
#include "imagesource_cms.h"
#include "imagesource_devicen_preview.h"
#include "imagesource_gamma.h"

#include "generaldialogs.h"

#include "cachedimage.h"

#include "pixbuf_from_imagesource.h"
#include "pixbufview.h"
#include "profilemanager.h"

#include "tiffsave.h"
#include "progressbar.h"
#include "profileselector.h"
#include "patheditor.h"
#include "simplecombo.h"

#include "rsplwrapper.h"

#include "jobqueue.h"
#include "cmtransformworker.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;

#if 0
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
#endif


class DuoTone_Options
{
	public:
	DuoTone_Options() : rotation(0), ggamma(1.0), gcontrast(1.0), cgamma(1.0), ccontrast(1.0), cmypreview(4,0), kpreview(4,0)
	{
		cmypreview[0]=0;
		cmypreview[1]=IS_SAMPLEMAX;
		cmypreview[2]=(IS_SAMPLEMAX*5)/6;
		cmypreview[3]=0;
		kpreview[3]=IS_SAMPLEMAX;
	}
	~DuoTone_Options()
	{
	}
	int GetRotation()
	{
		return(rotation);
	}
	void SetRotation(int r)
	{
		rotation=r;
	}
	ISDeviceNValue &GetCMY()
	{
		return(cmypreview);
	}
	void SetCMY(ISDeviceNValue &col)
	{
		cmypreview=col;
	}
	ISDeviceNValue &GetK()
	{
		return(kpreview);
	}
	void SetGGamma(double g)
	{
		ggamma=g;
	}
	double GetGGamma()
	{
		return(ggamma);
	}
	void SetGContrast(double g)
	{
		gcontrast=g;
	}
	double GetGContrast()
	{
		return(gcontrast);
	}
	void SetCGamma(double g)
	{
		cgamma=g;
	}
	double GetCGamma()
	{
		return(cgamma);
	}
	void SetCContrast(double g)
	{
		ccontrast=g;
	}
	double GetCContrast()
	{
		return(ccontrast);
	}
	void NormalizeCMY()
	{
		int max=cmypreview[0];
		if(cmypreview[1]>max)
			max=cmypreview[1];
		if(cmypreview[2]>max)
			max=cmypreview[2];
		max/=4;
		cmypreview[0]=(cmypreview[0]*(IS_SAMPLEMAX/4))/max;
		cmypreview[1]=(cmypreview[1]*(IS_SAMPLEMAX/4))/max;
		cmypreview[2]=(cmypreview[2]*(IS_SAMPLEMAX/4))/max;
	}
	protected:
	int rotation;
	double ggamma;
	double gcontrast;
	double cgamma;
	double ccontrast;
	ISDeviceNValue cmypreview;
	ISDeviceNValue kpreview;
};




class ImageSource_DuoTone : public ImageSource
{
	public:
	ImageSource_DuoTone(ImageSource *src,DuoTone_Options &opts) : ImageSource(src), source(src)
	{
		if(STRIP_ALPHA(source->type)!=IS_TYPE_RGB)
			throw "DuoTone only supports RGB input!";
		if(HAS_ALPHA(source->type))
			source=new ImageSource_Flatten(source);

		cgamma=opts.GetCGamma();
		ggamma=opts.GetGGamma();
		ccontrast=opts.GetCContrast();
		gcontrast=opts.GetGContrast();

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

			t=ccontrast*(IS_GAMMA(g,cgamma));
			if(t>IS_SAMPLEMAX)
				t=IS_SAMPLEMAX;
			rowbuffer[x*2]=t;

			t=gcontrast*(IS_GAMMA(IS_SAMPLEMAX-r,ggamma));
			if(t>IS_SAMPLEMAX)
				t=IS_SAMPLEMAX;
			rowbuffer[x*2+1]=t;
		}
		currentrow=row;
		return(rowbuffer);
	}
	protected:
	ImageSource *source;
	double ggamma;
	double cgamma;
	double gcontrast;
	double ccontrast;
};


class ImageSource_DuoToneToCMYK : public ImageSource
{
	public:
	ImageSource_DuoToneToCMYK(ImageSource *source,ISDeviceNValue &col1,ISDeviceNValue &col2) : ImageSource(source), source(source),col1(col1),col2(col2)
	{
		type=IS_TYPE_CMYK;
		samplesperpixel=4;
		MakeRowBuffer();
	}
	~ImageSource_DuoToneToCMYK()
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


#define TARGET_URI_LIST 1

static GtkTargetEntry dnd_file_drop_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
static gint dnd_file_drop_types_count = 1;



class DuoToner : public ConfigFile
{
	public:
	DuoToner() : ConfigFile(), profilemanager(this,"[Colour Management]"), factory(profilemanager)
	{
		char *fn=substitute_xdgconfighome("$XDG_CONFIG_HOME/duotoner.conf");
		configname=fn;
		ParseConfigFile(fn);
		free(fn);
	}
	~DuoToner()
	{
	}
	void SaveConfig()
	{
		SaveConfigFile(configname.c_str());
	}
	ImageSource *Process(ImageSource *src, DuoTone_Options &opts)
	{
		src=ToRGB(src);
		src=new ImageSource_HSV(src);
		src=new ImageSource_HueRotate(src,opts.GetRotation());
		src=new ImageSource_HSVToRGB(src);
		src=new ImageSource_DuoTone(src,opts);
		src=new ImageSource_DuoToneToCMYK(src,opts.GetCMY(),opts.GetK());
		return(src);
	}
	ImageSource *ToRGB(ImageSource *src,CMTransformFactory *factory=NULL)
	{
		if(!factory)
			factory=&this->factory;
		if(STRIP_ALPHA(src->type)!=IS_TYPE_RGB)
		{
			CMSProfile *p=src->GetEmbeddedProfile();
			CMSTransform *t=NULL;
			if(p)
				t=factory->GetTransform(CM_COLOURDEVICE_DEFAULTRGB,p);
			else
				t=factory->GetTransform(CM_COLOURDEVICE_DEFAULTRGB,STRIP_ALPHA(src->type));

			if(t)
				return(new ImageSource_CMS(src,t));
			else
				throw "Conversion to RGB failed - check ICC profiles.";
		}
		return(src);
	}
	ImageSource *ToMonitor(ImageSource *src,CMTransformFactory *factory=NULL)
	{
		if(!factory)
			factory=&this->factory;
		if(STRIP_ALPHA(src->type)!=IS_TYPE_RGB)
		{
			CMSProfile *p=src->GetEmbeddedProfile();
			CMSTransform *t=NULL;
			LCMSWrapper_Intent intents[]=
			{
				LCMSWRAPPER_INTENT_PERCEPTUAL,
				LCMSWRAPPER_INTENT_ABSOLUTE_COLORIMETRIC,
				LCMSWRAPPER_INTENT_RELATIVE_COLORIMETRIC
			};
			int idx=profilemanager.FindInt("ProofMode");
			if(idx>2) idx=2;
			if(p)
				t=factory->GetTransform(CM_COLOURDEVICE_DISPLAY,p,intents[idx]);
			else
				t=factory->GetTransform(CM_COLOURDEVICE_DISPLAY,STRIP_ALPHA(src->type),intents[idx]);

			if(t)
				return(new ImageSource_CMS(src,t));
			else
				throw "Conversion to RGB failed - check ICC profiles.";
		}
		return(src);
	}
	protected:
	ProfileManager profilemanager;
	CMTransformFactory factory;
	std::string configname;
};


#define RESPONSE_SAVE 1

class ProfileDialog
{
	public:
	ProfileDialog(ProfileManager &pm, DuoToner &duotoner, GtkWidget *parent) : pm(pm), duotoner(duotoner), dialog(NULL), parent(parent)
	{
		dialog=gtk_dialog_new_with_buttons(_("Setup colour options..."),
		GTK_WINDOW(parent),GtkDialogFlags(0),
		GTK_STOCK_SAVE,RESPONSE_SAVE,
		GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK,GTK_RESPONSE_OK,
		NULL);
		gtk_window_set_default_size(GTK_WINDOW(dialog),600,400);

		GtkWidget *vbox = GTK_DIALOG(dialog)->vbox;
		
		pe = patheditor_new(&pm);
		g_signal_connect(pe,"changed",G_CALLBACK(paths_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),pe,TRUE,TRUE,8);
		gtk_widget_show(pe);

		GtkWidget *table=gtk_table_new(2,2,FALSE);
		gtk_box_pack_start(GTK_BOX(vbox),table,FALSE,FALSE,8);
		gtk_widget_show(table);
		

		GtkWidget *tmp;
		tmp=gtk_label_new(_("Press profile:"));
		gtk_table_attach_defaults(GTK_TABLE(table),tmp,0,1,0,1);
		gtk_widget_show(tmp);

		tmp=gtk_label_new(_("Proofing mode:"));
		gtk_table_attach_defaults(GTK_TABLE(table),tmp,0,1,1,2);
		gtk_widget_show(tmp);

		ps = profileselector_new(&pm,IS_TYPE_CMYK);
		g_signal_connect(ps,"changed",G_CALLBACK(cmykprofile_changed),this);
		gtk_table_attach_defaults(GTK_TABLE(table),ps,1,2,0,1);
		gtk_widget_show(ps);

		SimpleComboOptions co;
		co.Add("",_("None"));
		co.Add("",_("Simulate Print"));
		co.Add("",_("Simulate Print, Adapt White"));

		sc=simplecombo_new(co);
		gtk_table_attach_defaults(GTK_TABLE(table),sc,1,2,1,2);
		gtk_widget_show(sc);

		const char *defprof=pm.FindString("DefaultCMYKProfile");
		if(defprof)
			profileselector_set_filename(PROFILESELECTOR(ps),defprof);
		simplecombo_set_index(SIMPLECOMBO(sc),pm.FindInt("ProofMode"));
	}
	~ProfileDialog()
	{
		if(dialog)
			gtk_widget_destroy(dialog);
	}
	void Run()
	{
		char *savedpaths=pm.GetPaths();
		
		gtk_widget_show_all(dialog);

		bool done=false;
		while(!done)
		{
			gint result=gtk_dialog_run(GTK_DIALOG(dialog));
			switch(result)
			{
				case GTK_RESPONSE_CANCEL:
					pm.ClearPaths();
					pm.AddPath(savedpaths);
					done=true;
					break;
				case RESPONSE_SAVE:
				case GTK_RESPONSE_OK:
					done=true;
					pm.SetInt("ProofMode",simplecombo_get_index(SIMPLECOMBO(sc)));
					pm.SetString("DefaultCMYKProfile",profileselector_get_filename(PROFILESELECTOR(ps)));

					char *tmppaths=pm.GetPaths();
					pm.SetString("ProfilePath",tmppaths);
					free(tmppaths);
					if(result==RESPONSE_SAVE)
						duotoner.SaveConfig();
					break;
			}
		}
		if(savedpaths)
			free(savedpaths);
	}
	private:
	ProfileManager &pm;
	DuoToner &duotoner;
	GtkWidget *dialog;
	GtkWidget *ps;
	GtkWidget *pe;
	GtkWidget *parent;
	GtkWidget *sc;

	static void	cmykprofile_changed(GtkWidget *widget,gpointer user_data)
	{
		cerr << "Received changed signal from ProfileSelector" << endl;
		ProfileDialog *dlg=(ProfileDialog *)user_data;
		ProfileSelector *c=PROFILESELECTOR(dlg->ps);
		const char *val=profileselector_get_filename(c);
		if(val)
			cerr << "Selected: " << val << endl;
		else
			cerr << "No profile selected... " << endl;
	}

	static void	paths_changed(GtkWidget *widget,gpointer user_data)
	{
		cerr << "Received changed signal from PathEditor" << endl;
		ProfileDialog *dlg=(ProfileDialog *)user_data;
		patheditor_get_paths(PATHEDITOR(dlg->pe),&dlg->pm);
		cerr << "Updated path list" << endl;
		profileselector_refresh(PROFILESELECTOR(dlg->ps));
		cerr << "Refreshed ProfileManager" << endl;
	}
};


class HighResPreview : public Job, public ThreadSync
{
	public:
	HighResPreview(DuoToner &dt,DuoTone_Options &opts,std::string filename,void (*completefunc)(GdkPixbuf *pb,void *userdata),void *ud)
		: Job(_("Rendering preview")), ThreadSync(), dt(dt), opts(opts), filename(filename),
		tempimage(NULL), completefunc(completefunc), userdata(ud)
	{
	}
	~HighResPreview()
	{
	}
	void Run(Worker *w)
	{
		CMTransformWorker *cw=(CMTransformWorker *)w;

		Debug[TRACE] << endl << "In HighResPreview's Run() method" << endl;

		// wait briefly here and bail out if cancelled.
		for(int i=0;i<25;++i)
		{
#ifdef WIN32
			Sleep(10);
#else
			usleep(10000);
#endif
			if(GetJobStatus()==JOBSTATUS_CANCELLED)
			{
				Debug[TRACE] << "Got break signal while pausing - Releasing" << endl;
				return;
			}
		}

		try
		{
			ImageSource *is=ISLoadImage(filename.c_str());
			is=dt.Process(is,opts);
			is=dt.ToMonitor(is,cw->factory);
			tempimage=new CachedImage(is);

			g_timeout_add(1,CleanupFunc,this);

			WaitCondition();
			delete tempimage;
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
			ErrorDialogs.AddMessage(err);
		}
	}
	static gboolean CleanupFunc(gpointer ud)
	{
		HighResPreview *t=(HighResPreview *)ud;

		Debug[TRACE] << "In cleanup function - creating pixbuf" << endl;

		if(t->GetJobStatus()!=JOBSTATUS_CANCELLED)
		{
			ImageSource *is=t->tempimage->GetImageSource();
			GdkPixbuf *pixbuf=pixbuf_from_imagesource(is,255,255,255);

			t->completefunc(pixbuf,t->userdata);
			g_object_unref(G_OBJECT(pixbuf));
		}
		t->Broadcast();
		return(FALSE);
	}
	protected:
	DuoToner &dt;
	DuoTone_Options &opts;
	std::string filename;
	CachedImage *tempimage;
	void (*completefunc)(GdkPixbuf *pb,void *userdata);
	void *userdata;
};


class GUI : public DuoToner, public JobDispatcher
{
	public:
	GUI() : DuoToner(), JobDispatcher(0), pview(NULL), imposter(NULL), impostersize(256), colorants(), hrpreview(NULL)
	{
		new CMTransformWorker(*this,profilemanager);
		new DeviceNColorant(colorants,"Red");
		new DeviceNColorant(colorants,"Black");

		window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_default_size(GTK_WINDOW(window),800,640);
		gtk_window_set_title (GTK_WINDOW (window), _("DuoToner"));
		gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			(GtkSignalFunc) gtk_main_quit, NULL);

		GtkWidget *hbox=gtk_hbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(window),hbox);
		gtk_widget_show(GTK_WIDGET(hbox));

		gtk_drag_dest_set(GTK_WIDGET(hbox),
				  GtkDestDefaults(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP),
				  dnd_file_drop_types, dnd_file_drop_types_count,
	                          GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
		g_signal_connect(G_OBJECT(hbox), "drag_data_received",
				 G_CALLBACK(get_dnd_data), this);


		pview=pixbufview_new(NULL,false);
		pixbufview_set_scale(PIXBUFVIEW(pview),true);

		gtk_box_pack_start(GTK_BOX(hbox),pview,TRUE,TRUE,8);
		gtk_widget_show(pview);
		gtk_widget_show(window);

		GtkWidget *tmp;

		tmp=gtk_vseparator_new();
		gtk_box_pack_start(GTK_BOX(hbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);
		

		GtkWidget *vbox=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(hbox),vbox,FALSE,FALSE,8);
		gtk_widget_show(vbox);


		tmp=gtk_button_new_with_label("Load image...");
		g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(loadimage_clicked),this);
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,8);
		gtk_widget_show(tmp);

		tmp=gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,8);
		gtk_widget_show(tmp);

		tmp=gtk_label_new(_("Hue Rotation"));
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		rotation=gtk_hscale_new_with_range(0.0,359.0,1.0);
		g_signal_connect(G_OBJECT(rotation),"value-changed",G_CALLBACK(sliders_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),rotation,FALSE,FALSE,0);
		gtk_widget_show(rotation);

		tmp=gtk_label_new(_("Grey gamma"));
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		greygamma=gtk_hscale_new_with_range(0.3,3.0,0.05);
		gtk_range_set_value(GTK_RANGE(greygamma),1.0);
		g_signal_connect(G_OBJECT(greygamma),"value-changed",G_CALLBACK(sliders_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),greygamma,FALSE,FALSE,0);
		gtk_widget_show(greygamma);

		tmp=gtk_label_new(_("Grey contrast"));
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		greycontrast=gtk_hscale_new_with_range(0.3,3.0,0.05);
		gtk_range_set_value(GTK_RANGE(greycontrast),1.0);
		g_signal_connect(G_OBJECT(greycontrast),"value-changed",G_CALLBACK(sliders_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),greycontrast,FALSE,FALSE,0);
		gtk_widget_show(greycontrast);

		tmp=gtk_label_new(_("Colour gamma"));
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		colgamma=gtk_hscale_new_with_range(0.3,3.0,0.05);
		gtk_range_set_value(GTK_RANGE(colgamma),1.0);
		g_signal_connect(G_OBJECT(colgamma),"value-changed",G_CALLBACK(sliders_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),colgamma,FALSE,FALSE,0);
		gtk_widget_show(colgamma);

		tmp=gtk_label_new(_("Colour contrast"));
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		colcontrast=gtk_hscale_new_with_range(0.3,3.0,0.05);
		gtk_range_set_value(GTK_RANGE(colcontrast),1.0);
		g_signal_connect(G_OBJECT(colcontrast),"value-changed",G_CALLBACK(sliders_changed),this);
		gtk_box_pack_start(GTK_BOX(vbox),colcontrast,FALSE,FALSE,0);
		gtk_widget_show(colcontrast);

		tmp=gtk_label_new(_("DuoTone colour"));
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		GdkColor col;
		ISDeviceNValue &cmy=opts.GetCMY();
		col.red=IS_SAMPLEMAX-cmy[0];
		col.green=IS_SAMPLEMAX-cmy[1];
		col.blue=IS_SAMPLEMAX-cmy[2];

		colselector=gtk_color_button_new();
		gtk_color_button_set_color(GTK_COLOR_BUTTON(colselector),&col);
		gtk_box_pack_start(GTK_BOX(vbox),colselector,FALSE,FALSE,0);
		g_signal_connect(G_OBJECT(colselector),"color-set",G_CALLBACK(sliders_changed),this);
		gtk_widget_show(colselector);


		tmp=gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,8);
		gtk_widget_show(tmp);

		tmp=gtk_button_new_with_label(_("Save DuoTone Image..."));
		g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(saveclicked),this);
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		tmp=gtk_vbox_new(FALSE,0);
		gtk_box_pack_start(GTK_BOX(vbox),tmp,TRUE,TRUE,0);
		gtk_widget_show(tmp);

		tmp=gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		tmp=gtk_button_new_with_label(_("Settings..."));
		g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(settingsclicked),this);
		gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,8);
		gtk_widget_show(tmp);

	}
	~GUI()
	{
		if(imposter)
			delete imposter;
	}
	void SetImage(std::string fn)
	{
		try
		{
			if(imposter)
				delete imposter;
			imposter=NULL;

			filename=fn;
			ImageSource *is=ISLoadImage(fn.c_str());
			width=is->width;
			height=is->height;
			int w=impostersize;
			int h=(impostersize*is->height)/is->width;
			if(h>impostersize)
			{
				h=impostersize;
				w=(impostersize*is->width)/is->height;
			}
			is=ISScaleImageBySize(is,w,h);
			imposter=new CachedImage(is);
			Update();
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}
	void Update()
	{
		if(imposter)
		{
			ImageSource *is=imposter->GetImageSource();
			is=Process(is,opts);
			is=ToMonitor(is);
			is=ISScaleImageBySize(is,width,height,IS_SCALING_NEARESTNEIGHBOUR);

			GdkPixbuf *pb=pixbuf_from_imagesource(is);
			delete is;

			pixbufview_set_pixbuf(PIXBUFVIEW(pview),pb);
			g_object_unref(G_OBJECT(pb));
			if(hrpreview)
				CancelJob(hrpreview);
			AddJob(hrpreview=new HighResPreview(*this,opts,filename,drawhrpreview,this));
		}
	}
	void Save(std::string outfn)
	{
		try
		{
			ProgressBar prog(_("Saving..."),window);
			ImageSource *is=ISLoadImage(filename.c_str());
			DuoTone_Options opt2=opts;
			opt2.NormalizeCMY();
			is=Process(is,opt2);
			TIFFSaver saver(outfn.c_str(),is);
			saver.SetProgress(&prog);
			saver.Save();
			delete is;
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}

	static void drawhrpreview(GdkPixbuf *pb,void *userdata)
	{
			GUI *gui=(GUI *)userdata;
			pixbufview_set_pixbuf(PIXBUFVIEW(gui->pview),pb);
	}

	static void loadimage_clicked(GtkWidget *wid,gpointer ud)
	{
		try
		{
			GUI *gui=(GUI *)ud;

			char *filename=File_Dialog(_("Load image..."),NULL,gui->window,true);
			if(filename)
			{
				gui->SetImage(filename);
				free(filename);
			}
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}
	static void settingsclicked(GtkWidget *wid,gpointer ud)
	{
		try
		{
			GUI *gui=(GUI *)ud;
			ProfileDialog dlg(gui->profilemanager,*gui,gui->window);
			dlg.Run();
			gui->Update();
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}
	static void saveclicked(GtkWidget *wid,gpointer ud)
	{
		try
		{
			GUI *gui=(GUI *)ud;
			if(gui->filename.size())
			{
				char *deffn=BuildFilename(gui->filename.c_str(),"-duotone","tif");
				char *actualfn=File_Save_Dialog(_("Please choose a filename for the duotone image..."),deffn,gui->window);
				if(actualfn)
				{
					gui->Save(actualfn);
					free(actualfn);
				}
				free(deffn);
			}
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}
	static void sliders_changed(GtkWidget *wid,gpointer ud)
	{
		GUI *gui=(GUI *)ud;
		GdkColor col;
		try
		{
			gtk_color_button_get_color(GTK_COLOR_BUTTON(gui->colselector),&col);
			ISDeviceNValue cmy(4,0);
			cmy[0]=IS_SAMPLEMAX-col.red;
			cmy[1]=IS_SAMPLEMAX-col.green;
			cmy[2]=IS_SAMPLEMAX-col.blue;
			gui->opts.SetCMY(cmy);
			gui->opts.SetRotation(gtk_range_get_value(GTK_RANGE(gui->rotation)));
			gui->opts.SetGGamma(gtk_range_get_value(GTK_RANGE(gui->greygamma)));
			gui->opts.SetCGamma(gtk_range_get_value(GTK_RANGE(gui->colgamma)));
			gui->opts.SetGContrast(gtk_range_get_value(GTK_RANGE(gui->greycontrast)));
			gui->opts.SetCContrast(gtk_range_get_value(GTK_RANGE(gui->colcontrast)));
			gui->Update();
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context,
	     gint x, gint y, GtkSelectionData *selection_data, guint info,
	     guint time, gpointer data)
	{
		gchar *temp;
		gchar *urilist=temp=g_strdup((const gchar *)selection_data->data);

		GUI *ui=(GUI *)data;

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

					ui->SetImage(filename);
				}
			}
		}

		if(temp)
			g_free(temp);
	}
	protected:
	GtkWidget *window;
	GtkWidget *pview;
	GtkWidget *rotation;
	GtkWidget *greygamma;
	GtkWidget *colgamma;
	GtkWidget *greycontrast;
	GtkWidget *colcontrast;
	GtkWidget *colselector;
	CachedImage *imposter;
	int width;
	int height;
	int impostersize;
	DeviceNColorantList colorants;
	DuoTone_Options opts;
	std::string filename;
	Job *hrpreview;
};


int main(int argc,char**argv)
{
	Debug.SetLevel(TRACE);

	gtk_init(&argc,&argv);

#ifdef WIN32
	char *logname=substitute_homedir("$HOME" SEARCHPATH_SEPARATOR_S ".duotoner_errorlog");
	Debug.SetLogFile(logname);
	delete logname;
#endif

	gtk_set_locale();

#ifdef WIN32
	bindtextdomain(PACKAGE,"locale");
#else
	bindtextdomain(PACKAGE,LOCALEDIR);
#endif
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

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

