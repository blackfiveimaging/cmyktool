#include <iostream>
#include <cstring>

#include <libgen.h>
#include <gtk/gtk.h>

#include "support/debug.h"
#include "support/rwmutex.h"
#include "support/thread.h"
#include "support/pathsupport.h"
#include "support/dirtreewalker.h"
#include "support/util.h"

#include "imageutils/tiffsave.h"
#include "imagesource/imagesource_util.h"
#include "imagesource/imagesource_promote.h"
#include "imagesource/imagesource_cms.h"
#include "imagesource/imagesource_montage.h"
#include "imagesource/imagesource_gdkpixbuf.h"
#include "imagesource/pixbuf_from_imagesource.h"

#include "miscwidgets/pixbufview.h"
#include "miscwidgets/colorantselector.h"
#include "miscwidgets/imageselector.h"
#include "miscwidgets/generaldialogs.h"
#include "miscwidgets/errordialogqueue.h"
#include "miscwidgets/uitab.h"
#include "miscwidgets/simplecombo.h"
#include "miscwidgets/pixbuf_from_imagedata.h"

#include "pixbufthumbnail/egg-pixbuf-thumbnail.h"

#include "progressbar.h"

#include "gfx/rgb.cpp"
#include "gfx/cmyk.cpp"
#include "gfx/profile.cpp"

#include "cachedimage.h"

#include "conversionopts.h"
#include "conversionoptsdialog.h"

#include "cmyktool_core.h"
#include "dialogs.h"
#include "cmykuitab.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

#define PRESET_OTHER_ESCAPE "<other>"

using namespace std;

class UITab_RenderThread;


class TestUI : public CMYKTool_Core
{
	public:
	TestUI();
	virtual ~TestUI();
	int BuildComboOpts(SimpleComboOptions &opts);  // Returns index of "Previous" option
	void AddImage(const char *filename);
	virtual void ProcessImage(const char *filename);
	virtual void SaveConfig();
	static void combochanged(GtkWidget *wid,gpointer userdata);
	static void convert(GtkWidget *wid,gpointer userdata);
	static void addimages(GtkWidget *wid,gpointer userdata);
//	static void batchprocess(GtkWidget *wid,gpointer userdata);
	static void showconversiondialog(GtkWidget *wid,gpointer userdata);
	static void showpreferencesdialog(GtkWidget *wid,gpointer userdata);
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, GtkSelectionData *selection_data, guint info, guint time, gpointer data);
	GtkWidget *window;
	protected:
	GtkWidget *imgsel;
	GtkWidget *notebook;
	GtkWidget *combo;
	GdkPixbuf *rgbpb;
	GdkPixbuf *cmykpb;
	GdkPixbuf *profpb;
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
//				ui->ProcessImage(filename);
			}
		}
	}
}


TestUI::TestUI() : CMYKTool_Core()
{
	profilemanager.SetInt("DefaultCMYKProfileActive",1);

	rgbpb=PixbufFromImageData(rgb_data,sizeof(rgb_data));
	cmykpb=PixbufFromImageData(cmyk_data,sizeof(cmyk_data));
	profpb=PixbufFromImageData(profile_data,sizeof(profile_data));

	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),FindInt("Win_W"),FindInt("Win_H"));
	gtk_window_move(GTK_WINDOW(window),FindInt("Win_X"),FindInt("Win_Y"));

	gtk_window_set_title (GTK_WINDOW (window), _("CMYKTool"));
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


	// Add Images button...
	GtkWidget *tmp;

	tmp=gtk_button_new_with_label(_("Add Images..."));
	gtk_signal_connect (GTK_OBJECT (tmp), "clicked",
		(GtkSignalFunc) addimages,this);
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	gtk_widget_show(tmp);	


	// ImageSelector

	imgsel=imageselector_new(NULL,GTK_SELECTION_MULTIPLE,false);
	gtk_signal_connect (GTK_OBJECT (imgsel), "double-clicked",
		(GtkSignalFunc) convert,this);
	gtk_box_pack_start(GTK_BOX(vbox),imgsel,TRUE,TRUE,0);
	gtk_widget_show(imgsel);


	// Convert button...


	tmp=gtk_button_new_with_label("Convert");
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(convert),this);
	gtk_widget_show(tmp);


	// Presets

	tmp=gtk_label_new(_("Using conversion preset:"));
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	gtk_widget_show(tmp);

	SimpleComboOptions opts;
	int previdx=BuildComboOpts(opts);
	combo=simplecombo_new(opts);
	gtk_box_pack_start(GTK_BOX(vbox),combo,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(combo),"changed",G_CALLBACK(combochanged),this);
	simplecombo_set_index(SIMPLECOMBO(combo),previdx);
	combochanged(combo,this);	// Load previous settings.
	gtk_widget_show(combo);


	tmp=gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,8);
	gtk_widget_show(tmp);


	// Preferences button


	tmp=gtk_button_new_with_label("Preferences...");
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(showpreferencesdialog),this);
	gtk_widget_show(tmp);


//	GtkWidget *tmp=gtk_button_new_with_label("Batch Process...");
//	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
//	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(batchprocess),this);
//	gtk_widget_show(tmp);


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
	if(rgbpb)
		g_object_unref(G_OBJECT(rgbpb));
	if(cmykpb)
		g_object_unref(G_OBJECT(cmykpb));
	if(profpb)
		g_object_unref(G_OBJECT(profpb));
}


void TestUI::SaveConfig()
{
	if(window)
	{
		gint x,y,w,h;
		gtk_window_get_position(GTK_WINDOW(window),&x,&y);
		gtk_window_get_size(GTK_WINDOW(window),&w,&h);

		SetInt("Win_X",x);
		SetInt("Win_Y",y);
		SetInt("Win_W",w);
		SetInt("Win_H",h);
	}

	CMYKTool_Core::SaveConfig();
}

#define PRESET_MAXCHARS 17

// Builds a set of ComboOpts for the presets.
// Returns the index of the special "Previous" item, if found.

int TestUI::BuildComboOpts(SimpleComboOptions &opts)
{
	int previdx=0;
	int idx=1;

	char *configdir=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);
	DirTreeWalker dtw(configdir);
	const char *fn;

	opts.Add(PRESET_NONE_ESCAPE,_("None"),_("No conversion"),true);

	while((fn=dtw.NextFile()))
	{
		CMYKConversionPreset p;
		p.Load(fn);
		const char *dn=p.FindString("DisplayName");
		if(!(dn && strlen(dn)>0))
			dn=_("<unknown>");

		if(strcmp(p.FindString("PresetID"),PRESET_PREVIOUS_ESCAPE)==0)
			previdx=idx;

		if(strlen(dn)<=PRESET_MAXCHARS)
			opts.Add(fn,dn,dn);
		else
		{
			char buf[PRESET_MAXCHARS+4];
			char *p=buf;
			strncpy(p,dn,PRESET_MAXCHARS);
			p[PRESET_MAXCHARS]=p[PRESET_MAXCHARS+1]=p[PRESET_MAXCHARS+2]='.';
			p[PRESET_MAXCHARS+3]=0;
			opts.Add(fn,p,dn);
		}
		++idx;
	}
	free(configdir);

	opts.Add(PRESET_OTHER_ESCAPE,_("Other..."),_("Create a new preset"),true);

	return(previdx);
}


void TestUI::ProcessImage(const char *filename)
{
	new CMYKUITab(window,notebook,convopts,dispatcher,filename);
}


void TestUI::convert(GtkWidget *wid,gpointer userdata)
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
		else if(strcmp(key,PRESET_NONE_ESCAPE)==0)
		{
			ui->convopts.SetMode(CMYKCONVERSIONMODE_NONE);
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


#if 0
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
#endif



// Preview widget for file chooser

static void updatepreview(GtkWidget *fc,void *ud)
{
	GtkWidget *preview=GTK_WIDGET(ud);
	gchar *fn=gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(fc));
	bool active=false;
	if(fn)
	{
		GError *err=NULL;
		GdkPixbuf *pb=egg_pixbuf_get_thumbnail_for_file(fn,EGG_PIXBUF_THUMBNAIL_NORMAL,&err);
		if(pb)
		{
			gtk_image_set_from_pixbuf(GTK_IMAGE(preview),pb);
			g_object_unref(pb);
			active=true;		
		}	
	}
	gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(fc),active);
}


void TestUI::addimages(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;

	GtkWidget *sel = gtk_file_chooser_dialog_new (_("Open File"),
		GTK_WINDOW(GTK_WINDOW(ui->window)),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(sel),TRUE);
//	if(ui->prevfile)
//		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(sel),ui->prevfile);
//	else
//	{
#ifdef WIN32
		char *dirname=substitute_homedir("$HOME\\My Documents\\My Pictures");
#else
		char *dirname=substitute_homedir("$HOME");
#endif
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(sel),dirname);	
//	}
//	g_free(ui->prevfile);
//	ui->prevfile=NULL;

	GtkWidget *preview=gtk_image_new();
	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(sel),GTK_WIDGET(preview));
	g_signal_connect(G_OBJECT(sel),"selection-changed",G_CALLBACK(updatepreview),preview);

	if (gtk_dialog_run (GTK_DIALOG (sel)) == GTK_RESPONSE_ACCEPT)
	{
		GSList *filenames=gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(sel));
		GSList *current=filenames;

		gtk_widget_destroy (sel);

		ProgressBar *prog=new ProgressBar(_("Adding images..."),true,ui->window);

		int count=0;
		while(current)
		{
			++count;
			current=g_slist_next(current);
		}

		int c=0;
		current=filenames;
		if(filenames)
		{
			while(current)
			{
				if(!prog->DoProgress(c,count))
					break;
				ui->AddImage((char *)current->data);
				current=g_slist_next(current);
				++c;
			}
			g_slist_free(filenames);
		}

		delete prog;
	}
	else
		gtk_widget_destroy (sel);
}


void TestUI::showconversiondialog(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;
	CMYKConversionOptions_Dialog(ui->convopts,ui->window);
}


void TestUI::showpreferencesdialog(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;
	PreferencesDialog(GTK_WINDOW(ui->window),*ui);
}


void TestUI::AddImage(const char *filename)
{
	Debug.SetLevel(TRACE);
	try
	{
		ImageSource_Montage *mon=new ImageSource_Montage(IS_TYPE_RGB);
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

			if(STRIP_ALPHA(is->type==IS_TYPE_GREY))
				is=new ImageSource_Promote(is,IS_TYPE_RGB);

			// We overlay icons depending on colourspace and whether there's an embedded profile.

			// We check the colourspace before applying the conversion to the monitor's profile, since that's RGB!

			ImageSource *profsymbol=NULL;
			if(is->GetEmbeddedProfile())
				profsymbol=new ImageSource_GdkPixbuf(profpb);

			ImageSource *emblem=NULL;
			switch(STRIP_ALPHA(is->type))
			{
				case IS_TYPE_RGB:
					emblem=new ImageSource_GdkPixbuf(rgbpb);
					break;
				case IS_TYPE_CMYK:
					emblem=new ImageSource_GdkPixbuf(cmykpb);
					break;
				default:
					break;
			}

			CMSTransform *transform=factory.GetTransform(CM_COLOURDEVICE_DISPLAY,is);
			if(transform)
				is=new ImageSource_CMS(is,transform);


			if(is->width<128)
			{
				if(profsymbol)
					mon->Add(profsymbol,0,35);
				if(emblem)
					mon->Add(emblem,0,10);
				mon->Add(is,10,0);
			}
			else
			{
				if(profsymbol)
					mon->Add(profsymbol,35,0);
				if(emblem)
					mon->Add(emblem,10,0);
				mon->Add(is,0,10);
			}
			GdkPixbuf *pb=pixbuf_from_imagesource(mon);
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

	bindtextdomain(PACKAGE,LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

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

