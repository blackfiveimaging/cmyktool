#include <iostream>

#include <libgen.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "debug.h"
#include "jobqueue.h"
#include "rwmutex.h"
#include "thread.h"
#include "util.h"

#include "tiffsaver.h"
#include "jpegsaver.h"
#include "imagesource_util.h"
#include "imagesource_cms.h"
#include "imagesource_colorantmask.h"
#include "imagesource_dither.h"
#include "pixbuf_from_imagesource.h"
#include "cachedimage.h"

#include "simplecombo.h"
#include "pixbufview.h"
#include "coloranttoggle.h"
#include "imageselector.h"
#include "generaldialogs.h"
#include "errordialogqueue.h"
#include "uitab.h"
#include "pixbuf_from_imagedata.h"
#include "spinner.h"
#include "progressbar.h"

#include "profilemanager.h"


#include "conversionopts.h"
#include "cmtransformworker.h"

#include "cmykuitab.h"

#include "gfx/chain1.cpp"
#include "gfx/chain2.cpp"
#include "gfx/sigma.cpp"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)


using namespace std;





///////////////////////  User interface /////////////////////////



class UITab_RenderJob : public Job, public ThreadSync, public Progress
{
	public:
	UITab_RenderJob(CMYKUITab &tab) : Job(_("Rendering preview")), ThreadSync(), Progress(), tab(tab), tempimage(NULL), page(0)
	{
		tab.Ref();
		unrefondelete=true;	// We need to be sure the reference will be released if the job gets cancelled before running.

		page=tab.view.displaymode;
	}
	~UITab_RenderJob()
	{
		if(unrefondelete)
			tab.UnRef();
	}
	void Run(Worker *w)
	{
		tab.spinner.Start();
		CMTransformWorker *cw=(CMTransformWorker *)w;

		unrefondelete=false;	// Once the job has definitely started it can handle unref.

		Debug[TRACE] << endl << "In RenderJob's Run() method" << endl;

		try
		{
			ImageSource *is=tab.image->GetImageSource();

			// We only want to do the ColorantMask thing in "inspect" mode...
			if(tab.view.displaymode==CMYKDISPLAY_INSPECT)
				is=new ImageSource_ColorantMask(is,tab.collist);
			Debug[TRACE] << "Image colourspace: " << is->type << endl;

			// If we're in "inspect" mode, we don't want to convert from the image's colourspace
			// to the monitor's - assuming of course that the image is RGB.
			// If the image is CMYK we have to convert.
			if(tab.view.displaymode==CMYKDISPLAY_INSPECT && STRIP_ALPHA(is->type)==IS_TYPE_RGB)
				is->SetEmbeddedProfile(NULL);

			Debug[TRACE] << "Opts output space " << tab.core.GetOptions().GetOutProfile() << endl;

			LCMSWrapper_Intent intent=LCMSWRAPPER_INTENT_DEFAULT;
			switch(tab.view.displaymode)
			{
				case CMYKDISPLAY_INSPECT:
					break;
				case CMYKDISPLAY_PROOF:
					intent=LCMSWRAPPER_INTENT_ABSOLUTE_COLORIMETRIC;
					break;
				case CMYKDISPLAY_PROOF_ADAPT_WHITE:
					intent=LCMSWRAPPER_INTENT_RELATIVE_COLORIMETRIC_BPC;
					break;
			}

			RefCountPtr<CMSTransform> transform(cw->factory->GetTransform(CM_COLOURDEVICE_DISPLAY,is,intent));
			if(transform)
				is=new ImageSource_CMS(is,transform);
			else
				Debug[WARN] << "RenderJob: Couldn't create transform" << endl;

			// Create a cached image to hold the data since converting to pixbuf from a sub-thread seems 
			// to be a recipe for disaster.

			// Note that on 32-bit systems when processing huge images it's entirely possible for this
			// to fail due to address space exhaustion!
			try
			{
				tempimage=new CachedImage(RefCountPtr<ImageSource>(is),this);
			}
			catch(const char *err)
			{
				// If creating the cached image failed we try again having scaled down.
				is=ISScaleImageBySize(is,is->width/4,is->height/4);
				tempimage=new CachedImage(RefCountPtr<ImageSource>(is),this);
			}
//			delete is;
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
			ErrorDialogs.AddMessage(err);
		}
		if(tempimage)
		{
			g_timeout_add(1,CleanupFunc,this);
			WaitCondition();
		}
		else
		{
			Debug[TRACE] << endl << "Adding reference from RenderJob Run() - no pixbuf created" << endl;
			tab.UnRef();
		}
		tab.spinner.Stop();
	}
	static gboolean CleanupFunc(gpointer ud)
	{
		UITab_RenderJob *t=(UITab_RenderJob *)ud;

		Debug[TRACE] << "In cleanup function - creating pixbuf" << endl;

		ImageSource *is=t->tempimage->GetImageSource();
		GdkPixbuf *pixbuf=pixbuf_from_imagesource(is,255,255,255,t);
		delete t->tempimage;

		Debug[TRACE] << "In cleanup function - drawing pixbuf" << endl;

		if(pixbuf)
		{
			Debug[TRACE] << "Drawing rendered pixbuf on page " << t->page << endl;
			pixbufview_set_pixbuf(PIXBUFVIEW(t->tab.pbview),pixbuf,t->page);
			pixbufview_set_page(PIXBUFVIEW(t->tab.pbview),t->page);
			g_object_unref(G_OBJECT(pixbuf));

			if(is->width==t->tab.view.w && is->height==t->tab.view.h)
			{
				pixbufview_set_offset(PIXBUFVIEW(t->tab.pbview),t->tab.view.xpan,t->tab.view.ypan);
				pixbufview_set_scale(PIXBUFVIEW(t->tab.pbview),t->tab.view.zoom);

	//			t->tab.SetView(t->tab.view);
			}
		}
		delete is;

		Debug[TRACE] << endl << "Removing reference from RenderJob callback" << endl;
		t->tab.UnRef();
		t->Broadcast();
		return(FALSE);
	}
	bool DoProgress(int i, int maxi)
	{
//		Debug[TRACE] << "In customised DoProgress function - JobStatus: " << GetJobStatus() << endl;
		return(GetJobStatus()!=JOBSTATUS_CANCELLED);
	}
	protected:
	CMYKUITab &tab;
	ImageSource *src;
	CachedImage *tempimage;
	int page;
	bool unrefondelete;
};


class UITab_CacheJob : public Job
{
	public:
	UITab_CacheJob(CMYKUITab &tab,const char *filename) : Job(_("Converting image")), tab(tab), filename(NULL)
	{
		Debug[TRACE] << endl << "Adding reference from CacheJob Constructor" << endl;

		tab.Ref();
		unrefondelete=true;	// If the job gets cancelled we need to be sure the reference will be released.

		this->filename=SafeStrdup(filename);
	}
	~UITab_CacheJob()
	{
		free(filename);
		Debug[TRACE] << endl << "Removing reference from CacheJob Destructor" << endl;

		if(unrefondelete)
			tab.UnRef();
	}
	void Run(Worker *w)
	{
		tab.spinner.Start();
		CMTransformWorker *cw=(CMTransformWorker *)w;

		unrefondelete=false;	// Once the job has definitely been started it can handle the unref.

		try
		{
			ImageSource *is=ISLoadImage(filename);

			is=tab.core.GetOptions().Apply(is,NULL,cw->factory);

			// FIXME - need mutex protection here...
			if(tab.image)
				delete tab.image;
			tab.image=new CachedImage(RefCountPtr<ImageSource>(is));

			Debug[TRACE] << "Cached image generated - triggering render job" << endl;
			UITab_RenderJob renderjob(tab);
			renderjob.Run(w);
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
			ErrorDialogs.AddMessage(err);
			Debug[TRACE] << endl << "Adding reference from the end of CacheJob Run method - error encountered" << endl;
			tab.UnRef();	// If we encountered an error, delete the tab.
		}
		Debug[TRACE] << endl << "Adding reference from the end of CacheJob Run method" << endl;
		tab.spinner.Stop();
		tab.UnRef();	// Release the reference obtained when the job was created.
	}
	protected:
	CMYKUITab &tab;
	char *filename;
	bool unrefondelete;
};


////// CMYKUITab member functions ///////


static void setlinkedview(GtkWidget *widget,gpointer userdata)
{
	Debug[TRACE] << "In CMYKUITab::setlinkedview" << endl;
	CMYKUITab_View *view=(CMYKUITab_View *)userdata;
	GQuark quark=g_quark_from_static_string("TabPointer");
	gpointer qdata=g_object_get_qdata(G_OBJECT(widget),quark);
	if(qdata)
	{
		CMYKUITab *tab=(CMYKUITab *)qdata;
		if(tab!=view->source && tab->GetLinked())
			tab->SetView(*view);
	}
}


void CMYKUITab::ViewChanged(GtkWidget *widget,gpointer userdata)
{
	Debug[TRACE] << "In CMYKUITab::ViewChanged" << endl;
	CMYKUITab *tab=(CMYKUITab *)userdata;
	if(!tab->GetLinked())
		return;

	if(tab->image)
	{
		CMYKUITab_View view=tab->GetView();
		gtk_container_foreach(GTK_CONTAINER(tab->notebook),setlinkedview,&view);
	}
}


void CMYKUITab::MouseMove(GtkWidget *widget,gpointer userdata)
{
	Debug[TRACE] << "In CMYKUITab::MouseMove" << endl;
	CMYKUITab *tab=(CMYKUITab *)userdata;
	if(!tab->image)
		return;

	int x=pixbufview_get_mousex(PIXBUFVIEW(tab->pbview));
	int y=pixbufview_get_mousey(PIXBUFVIEW(tab->pbview));

	Debug[TRACE] << "Calling GetPixel()" << endl;
	ISDeviceNValue val=tab->image->GetPixel(x,y);

	Debug[TRACE] << "Setting value()" << endl;
	coloranttoggle_set_value(COLORANTTOGGLE(tab->colsel),val);
	Debug[TRACE] << "Calculating sum..." << endl;

	int sum=0;
	for(int i=0;i<val.GetChannels();++i)
	{
		sum+=(100 * val[i])/IS_SAMPLEMAX;
	}
	char buf[10];
	sprintf(buf,"%d",sum<9999 ? sum : 9999 );
	gtk_label_set_text(GTK_LABEL(tab->sumlabel),buf);
}


void CMYKUITab::LinkToggled(GtkWidget *widget,gpointer userdata)
{
	CMYKUITab *tab=(CMYKUITab *)userdata;
	int val=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tab->linkbutton));
	if(val)
		gtk_button_set_image(GTK_BUTTON(tab->linkbutton),tab->chain1);
	else
		gtk_button_set_image(GTK_BUTTON(tab->linkbutton),tab->chain2);
}


void CMYKUITab::DisplayModeChanged(GtkWidget *widget,gpointer userdata)
{
	CMYKUITab *tab=(CMYKUITab *)userdata;
	CMYKTabDisplayMode mode=(CMYKTabDisplayMode)simplecombo_get_index(SIMPLECOMBO(tab->displaymode));
	tab->view.displaymode=mode;

	Debug[TRACE] << "Setting displaymode to " << mode << endl;

	gtk_widget_set_sensitive(tab->colsel,mode==CMYKDISPLAY_INSPECT);

	tab->SetDisplayMode(mode);

	ViewChanged(widget,userdata);
}


CMYKUITab::CMYKUITab(GtkWidget *parent,GtkWidget *notebook,CMYKTool_Core &core,const char *filename)
	: UITab(notebook), core(core), parent(parent), colsel(NULL), pbview(NULL), image(NULL), collist(NULL),
	filename(NULL), renderjob(NULL), chain1(NULL), chain2(NULL), view(this), spinner()
{
	// Get pixbufs for chain icon...
	GdkPixbuf *chain1pb=PixbufFromImageData(chain1_data,sizeof(chain1_data));
	GdkPixbuf *chain2pb=PixbufFromImageData(chain2_data,sizeof(chain2_data));

	chain1=gtk_image_new_from_pixbuf(chain1pb);
	chain2=gtk_image_new_from_pixbuf(chain2pb);

	g_object_ref(G_OBJECT(chain1));
	g_object_ref(G_OBJECT(chain2));

	linkbutton=gtk_toggle_button_new();
	g_signal_connect(G_OBJECT(linkbutton),"toggled",G_CALLBACK(LinkToggled),this);
	gtk_button_set_image(GTK_BUTTON(linkbutton),chain2);
	gtk_widget_show(chain1);

	AddTabButton(linkbutton);

	AddTabWidget(spinner.GetWidget());

	hbox=GetBox();
//	g_signal_connect(G_OBJECT(hbox),"motion-notify-event",G_CALLBACK(mousemove),this);

	// Add a quark to the tab, which we can use to retrieve a pointer to this tab
	// if we want to make other tabs match the view of this one.
	GQuark quark=g_quark_from_static_string("TabPointer");
	g_object_set_qdata(G_OBJECT(hbox),quark,this);

	GtkWidget *vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),vbox,TRUE,TRUE,0);
	gtk_widget_show(vbox);

	pbview=pixbufview_new(NULL,false);
	pixbufview_add_page(PIXBUFVIEW(pbview),NULL);
	pixbufview_add_page(PIXBUFVIEW(pbview),NULL);
	pixbufview_add_page(PIXBUFVIEW(pbview),NULL);

	gtk_box_pack_start(GTK_BOX(vbox),pbview,TRUE,TRUE,0);
	g_signal_connect(G_OBJECT(pbview),"changed",G_CALLBACK(ViewChanged),this);
	gtk_widget_show(pbview);

	GtkWidget *hbox2=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(vbox),hbox2,FALSE,FALSE,0);
	gtk_widget_show(hbox2);


	SimpleComboOptions sco;
	sco.Add("",_("Inspect"),_("Displays image data without conversion from target space to monitor, allowing you to see "
				 "how the data has been modified by the conversion."));
	sco.Add("",_("Simulate print, adapt white"),_("Simulates printed colours but uses the monitor's white point. "
				 "(Conversion from destination to monitor profiles, using Relative Colorimetric intent with BlackPoint Compensation.)"));
	sco.Add("",_("Simulate print"),_("Simulates printed colours, using paper white. "
				 "(Conversion from destination to monitor profiles, using Absolute Colorimetric intent.)"));

	displaymode=simplecombo_new(sco);
	g_signal_connect(G_OBJECT(displaymode),"changed",G_CALLBACK(DisplayModeChanged),this);
	gtk_box_pack_start(GTK_BOX(hbox2),displaymode,FALSE,FALSE,0);
	gtk_widget_show(displaymode);


	colsel=coloranttoggle_new(NULL);
	gtk_signal_connect (GTK_OBJECT (colsel), "changed",
		(GtkSignalFunc) ColorantsChanged, this);
	gtk_box_pack_start(GTK_BOX(hbox2),colsel,FALSE,FALSE,0);
	gtk_widget_show(colsel);

	gtk_signal_connect (GTK_OBJECT (pbview), "mousemove",(GtkSignalFunc)MouseMove, this);


	GtkWidget *tmp;


	GdkPixbuf *sigmapb=PixbufFromImageData(sigma_data,sizeof(sigma_data));
	tmp=gtk_image_new_from_pixbuf(sigmapb);
	gtk_box_pack_start(GTK_BOX(hbox2),tmp,FALSE,FALSE,2);
	gtk_widget_show(tmp);

	sumlabel=gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox2),sumlabel,FALSE,FALSE,2);
	gtk_widget_show(sumlabel);


	tmp=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox2),tmp,TRUE,TRUE,0);
	gtk_widget_show(tmp);


	GtkWidget *savebutton=gtk_button_new_with_label(_("Save..."));
	g_signal_connect(G_OBJECT(savebutton),"clicked",G_CALLBACK(Save),this);
	gtk_box_pack_start(GTK_BOX(hbox2),savebutton,FALSE,FALSE,0);
	gtk_widget_show(savebutton);

#if 0	
	popupshown=false;
	popup=gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_default_size(GTK_WINDOW(popup),10,10);
	gtk_window_set_transient_for(GTK_WINDOW(popup),GTK_WINDOW(parent));

	colsel=coloranttoggle_new(NULL);
	gtk_signal_connect (GTK_OBJECT (colsel), "changed",
		(GtkSignalFunc) ColorantsChanged, this);
	gtk_container_add(GTK_CONTAINER(popup),colsel);
	gtk_widget_show(colsel);
#endif

	SetImage(filename);

	SetLinked(true);

	simplecombo_set_index(SIMPLECOMBO(displaymode),view.displaymode);
}


CMYKUITab::~CMYKUITab()
{
	if(chain1)
		g_object_unref(G_OBJECT(chain1));
	if(chain2)
		g_object_unref(G_OBJECT(chain2));
	if(collist)
		delete collist;
	if(image)
		delete image;
	if(filename)
		free(filename);
}


bool CMYKUITab::GetLinked()
{
	return(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(linkbutton)));
}


static void attemptlinkedview(GtkWidget *widget,gpointer userdata)
{
	CMYKUITab *tab=(CMYKUITab *)userdata;
	GQuark quark=g_quark_from_static_string("TabPointer");
	gpointer qdata=g_object_get_qdata(G_OBJECT(widget),quark);
	if(qdata)
	{
		CMYKUITab *othertab=(CMYKUITab *)qdata;
		if(othertab->GetLinked())
		{
			CMYKUITab_View view=othertab->GetView();
			tab->SetView(view);
		}
	}
}


void CMYKUITab::SetLinked(bool linked)
{
	if(linked)
	{
		// Cycle through the tabs looking for one with a compatible view...
		gtk_container_foreach(GTK_CONTAINER(notebook),attemptlinkedview,this);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linkbutton),linked);
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

		ImageSource *is=ISLoadImage(fname);
		if(is)
		{
			collist=new DeviceNColorantList(core.GetOptions().GetOutputType(is->type));
			coloranttoggle_set_colorants(COLORANTTOGGLE(colsel),collist);

			char *fn=SafeStrdup(fname);
			SetTabText(basename(fn));
			free(fn);

			core.GetDispatcher().AddJob(new UITab_CacheJob(*this,filename));
			delete is;
		}
	}
	catch (const char *err)
	{
		ErrorDialogs.AddMessage(err);
	}
}


void CMYKUITab::ColorantsChanged(GtkWidget *wid,gpointer userdata)
{
	CMYKUITab *ob=(CMYKUITab *)userdata;
	Debug[TRACE] << "In ColorantsChanged" << endl;
//	gtk_widget_set_sensitive(ob->colsel,FALSE);

	ob->Redraw();
}

void CMYKUITab::Redraw()
{
	Debug[TRACE] << "In CMYKUITab::Redraw()" << endl;
	// Cancel existing render job.  This is harmless if the job's completed already,
	// even if the object's been deleted
	if(renderjob)
		core.GetDispatcher().CancelJob(renderjob);
	if(image)
		core.GetDispatcher().AddJob(renderjob=new UITab_RenderJob(*this));
}


/////////// Save Dialog ////////////

enum SaveDialog_Format {FORMAT_JPEG,FORMAT_JPEGDITHERED,FORMAT_TIFF,FORMAT_TIFFDITHERED,FORMAT_TIFF16};

class UITab_SaveDialog
{
	public:
	UITab_SaveDialog(CMYKUITab &tab) : tab(tab), dialog(NULL)
	{
		tab.Ref();

		GtkWidget *hbox;
	 	dialog=gtk_dialog_new_with_buttons(_("Save Image..."),
			GTK_WINDOW(tab.parent),GtkDialogFlags(0),
			GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE,GTK_RESPONSE_OK,
			NULL);

		hbox=gtk_hbox_new(FALSE,5);
		filechooser = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),hbox,TRUE,TRUE,5);
		gtk_box_pack_start(GTK_BOX(hbox),filechooser,TRUE,TRUE,5);
		gtk_widget_show(filechooser);
		gtk_widget_show(hbox);

		GtkWidget *table=gtk_table_new(2,3,FALSE);
		gtk_container_set_border_width(GTK_CONTAINER(table),5);
		gtk_table_set_col_spacing(GTK_TABLE(table),0,10);
		gtk_table_set_row_spacings(GTK_TABLE(table),5);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),table,FALSE,FALSE,5);
		gtk_widget_show(table);

		GtkAttachOptions gao = (GtkAttachOptions)(GTK_EXPAND|GTK_FILL);

		GtkWidget *label;
		int row=0;

		// Format combo...

		label=gtk_label_new(_("File format:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.95,0.5);
		gtk_table_attach(GTK_TABLE(table),label,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);

		SimpleComboOptions opts;
		opts.Add("JPEG",_("JPEG"),_("JPEG - Lossy format optimised for photographic images"));
		opts.Add("JPEGDithered",_("JPEG (dithered)"),_("JPEG - Lossy format with dithering to hide contouring artifacts"));
		opts.Add("TIFF",_("TIFF (8-bit)"),_("TIFF - 8-bits per sample, for increased compatibility"));
		opts.Add("TIFFDithered",_("TIFF (8-bit dithered)"),_("TIFF - 8-bits per sample, with dithering to hide contouring artifacts"));
		opts.Add("TIFF16",_("TIFF (16-bit)"),_("TIFF - 16-bits per sample, for maximum quality"));

		format=simplecombo_new(opts);
		g_signal_connect(G_OBJECT(format),"changed",G_CALLBACK(format_changed),this);
		gtk_table_attach(GTK_TABLE(table),format,1,2,row,row+1,gao,GTK_SHRINK,0,0);

		++row;

		// JPEG Quality setting...

		label=gtk_label_new(_("JPEG Quality:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.95,0.5);
		gtk_table_attach(GTK_TABLE(table),label,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
		gtk_widget_show(label);

		quality=gtk_spin_button_new_with_range(60,100,1);
		gtk_table_attach(GTK_TABLE(table),quality,1,2,row,row+1,gao,GTK_SHRINK,0,0);
		gtk_widget_show(quality);


		++row;

		Debug[TRACE] << "Creating label..." << endl;

		// Resolution override...

		label=gtk_label_new(_("Resolution:"));
		gtk_misc_set_alignment(GTK_MISC(label),0.95,0.5);
		gtk_table_attach(GTK_TABLE(table),label,0,1,row,row+1,GTK_SHRINK,GTK_SHRINK,0,0);
		gtk_widget_show(label);

		Debug[TRACE] << "Creating spin button..." << endl;

		resolution=gtk_spin_button_new_with_range(36,2400,1);
		gtk_table_attach(GTK_TABLE(table),resolution,1,2,row,row+1,gao,GTK_SHRINK,0,0);
		gtk_widget_show(resolution);


		++row;

		// Embed Profile setting...

		embedprofile=gtk_check_button_new_with_label(_("Embed ICC Profile"));
		gtk_table_attach(GTK_TABLE(table),embedprofile,1,2,row,row+1,gao,GTK_SHRINK,0,0);
		gtk_widget_show(embedprofile);

		++row;

		FetchDefaults();

		gtk_widget_show_all(dialog);
		g_signal_connect(G_OBJECT(dialog),"response",G_CALLBACK(response),this);
	}
	~UITab_SaveDialog()
	{
		if(dialog)
			gtk_widget_destroy(dialog);
		tab.UnRef();
	}
	static void format_changed(GtkWidget *widget, gpointer userdata)
	{
		UITab_SaveDialog *dlg=(UITab_SaveDialog *)userdata;
		
		SaveDialog_Format fmt=SaveDialog_Format(simplecombo_get_index(SIMPLECOMBO(dlg->format)));

		Debug[TRACE] << "Got format: " << fmt << endl;

		gtk_widget_set_sensitive(dlg->quality,fmt==FORMAT_JPEG || fmt==FORMAT_JPEGDITHERED);

		Debug[TRACE] << "Set widget sensitivity " << endl;

		const char *fn=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg->filechooser));
		if(!fn)
			fn=strdup(dlg->filename.c_str());
		if(fn)
		{
			Debug[TRACE] << "Got filename: " << fn << endl;
			char *newfn=BuildFilename(fn,NULL,(fmt==FORMAT_JPEG || fmt==FORMAT_JPEGDITHERED) ? "jpg" : "tif");
			Debug[TRACE] << "Built new filename: " << newfn << endl;

//			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),g_path_get_dirname(newfn));
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg->filechooser),g_path_get_basename(newfn));	// Yuk.

//			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg->filechooser),newfn);
			free(newfn);
		}
	}
	void FetchDefaults()
	{
		// Set default filename

		char *tmpfn;
		if(tab.core.GetOptions().GetOutputType()==IS_TYPE_CMYK)
			tmpfn=BuildFilename(tab.filename,"-CMYK","jpg");
		else
			tmpfn=BuildFilename(tab.filename,"-exported","jpg");
		filename=tmpfn;
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser),g_path_get_dirname(tmpfn));
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filechooser),g_path_get_basename(tmpfn));
		free(tmpfn);
		Debug[TRACE] << "Getting defaults" << endl;
		int qual=tab.core.FindInt("SaveJPEGQuality");
		Debug[TRACE] << "Got quality: " << qual << endl;

		// We fetch the image's native resolution, and if it's 72dpi we fetch the
		// resolution from the configdb.  This works around the GdkPixbuf loaders'
		// inability to pick up the resolution.

		ImageSource *is=tab.image->GetImageSource();
		int res=is->xres;
		if(res==72)
			res=tab.core.FindInt("SaveResolution");

		Debug[TRACE] << "Got resolution: " << res << endl;
		bool embedflag=bool(tab.core.FindInt("SaveEmbedProfile"));
		Debug[TRACE] << "Got embed: " << embedflag << endl;
		int fmt=tab.core.FindInt("SaveFormat");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(quality),qual);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(resolution),res);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(embedprofile),embedflag);
		simplecombo_set_index(SIMPLECOMBO(format),fmt);
		format_changed(format,this);
	}
	void StoreDefaults()
	{
	}
	static void response(GtkDialog *dialog, gint responseid, gpointer userdata)
	{
		UITab_SaveDialog *dlg=(UITab_SaveDialog *)userdata;
		bool done=false;
		switch(responseid)
		{
			case GTK_RESPONSE_OK:
				{
					const char *fn=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg->filechooser));
					if(CheckFileExists(fn))
					{
						if(!Query_Dialog(_("File exists - OK to overwrite?"),dlg->dialog))
							break;
					}

					SaveDialog_Format fmt=SaveDialog_Format(simplecombo_get_index(SIMPLECOMBO(dlg->format)));
					int quality=gtk_spin_button_get_value(GTK_SPIN_BUTTON(dlg->quality));
					int resolution=gtk_spin_button_get_value(GTK_SPIN_BUTTON(dlg->resolution));
					bool embedprofile=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlg->embedprofile));

					dlg->tab.core.SetInt("SaveFormat",int(fmt));
					dlg->tab.core.SetInt("SaveResolution",int(resolution));
					dlg->tab.core.SetInt("SaveJPEGQuality",int(quality));
					dlg->tab.core.SetInt("SaveEmbedProfile",int(embedprofile));

					ProgressBar p(_("Saving"),true,dlg->dialog);
					ImageSource *is=dlg->tab.image->GetImageSource();

					// Patch resolution
					is->xres=resolution;
					is->yres=resolution;

					if(!embedprofile)
						is->SetEmbeddedProfile(NULL);

					ImageSaver *imgsaver=NULL;
					switch(fmt)
					{
						case FORMAT_TIFFDITHERED:
							is=new ImageSource_Dither(is,8);
							// Fall through to TIFF...
						case FORMAT_TIFF:
							imgsaver=new TIFFSaver(fn,RefCountPtr<ImageSource>(is));
							break;
						case FORMAT_TIFF16:
							imgsaver=new TIFFSaver(fn,RefCountPtr<ImageSource>(is),true);
							break;
						case FORMAT_JPEGDITHERED:
							is=new ImageSource_Dither(is,8);
							// Fall through to JPEG...
						case FORMAT_JPEG:
							imgsaver=new JPEGSaver(fn,RefCountPtr<ImageSource>(is),quality);
							break;
					}

					imgsaver->SetProgress(&p);
					imgsaver->Save();

					delete imgsaver;

					done=true;
				}

				break;
			case GTK_RESPONSE_CANCEL:
			case GTK_RESPONSE_DELETE_EVENT:
				done=true;
				break;
		}
		if(done)
			delete dlg;
	}
	protected:
	CMYKUITab &tab;
	GtkWidget *dialog;
	GtkWidget *filechooser;
	GtkWidget *format;
	GtkWidget *quality;
	GtkWidget *resolution;
	GtkWidget *embedprofile;
	std::string filename;
};



void CMYKUITab::Save(GtkWidget *widget,gpointer userdata)
{
	CMYKUITab *ob=(CMYKUITab *)userdata;
	new UITab_SaveDialog(*ob);
#if 0
	char *tmpfn;
	if(ob->convopts.GetOutputType()==IS_TYPE_CMYK)
		tmpfn=BuildFilename(ob->filename,"-CMYK","tif");
	else
		tmpfn=BuildFilename(ob->filename,"-RGB","tif");
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
#endif
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


void CMYKUITab::SetView(CMYKUITab_View &view)
{
	if(image)
	{
		ImageSource *is=image->GetImageSource();

		if(is->width==view.w && is->height==view.h)
		{
			pixbufview_set_offset(PIXBUFVIEW(pbview),view.xpan,view.ypan);
			pixbufview_set_scale(PIXBUFVIEW(pbview),view.zoom);

			if(this->view.displaymode!=view.displaymode)
			{
				simplecombo_set_index(SIMPLECOMBO(displaymode),view.displaymode);
//				Debug[TRACE] << endl << "Copying displaymode of " << view.displaymode << endl;
				SetDisplayMode(view.displaymode);
			}
//			else
//				Debug[TRACE] << endl << "Already have displaymode of " << view.displaymode << endl;
		}
		delete is;
	}
	this->view=view;
}


CMYKUITab_View CMYKUITab::GetView()
{
	if(image)
	{
		int x=pixbufview_get_xoffset(PIXBUFVIEW(pbview));
		int y=pixbufview_get_yoffset(PIXBUFVIEW(pbview));
		bool zoom=pixbufview_get_scale(PIXBUFVIEW(pbview));
		ImageSource *is=image->GetImageSource();

		CMYKTabDisplayMode mode=(CMYKTabDisplayMode)simplecombo_get_index(SIMPLECOMBO(displaymode));
//		CMYKUITab_View view(this,is->width,is->height,x,y,zoom,mode);
		view.w=is->width;
		view.h=is->height;
		view.xpan=x;
		view.ypan=y;
		view.zoom=zoom;
		view.displaymode=mode;
		delete is;
		return(view);
	}
	else
		return(CMYKUITab_View(this));
}


void CMYKUITab::SetDisplayMode(CMYKTabDisplayMode mode)
{
	// We only need to trigger a redraw here if there's no image on the selected page already.
	if(!pixbufview_get_pixbuf(PIXBUFVIEW(pbview),mode))
	{
		Debug[TRACE] << "Can't find image on page " << mode << " - creating" << endl;
		Redraw();
	}
	else
	{
		Debug[TRACE] << "Found image on page " << mode << endl;
		pixbufview_set_page(PIXBUFVIEW(pbview),mode);
	}
}

