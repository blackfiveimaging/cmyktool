#include <iostream>
#include <cstring>

#include <libgen.h>
#include <gtk/gtk.h>
#include <getopt.h>

#ifdef WIN32
#include <w32api.h>
#define _WIN32_IE IE5
#define _WIN32_WINNT Windows2000
#include <shlobj.h>
#endif

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
#include "gfx/logo.cpp"

#include "cachedimage.h"

#include "conversionopts.h"
#include "conversionoptsdialog.h"
#include "devicelinkdialog.h"

#include "cmyktool_core.h"
#include "dialogs.h"
#include "cmykuitab.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)
#define N_(x) gettext_noop(x)

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
	void StartProgressBar();
	static void combochanged(GtkWidget *wid,gpointer userdata);
	static void convert(GtkWidget *wid,gpointer userdata);
	static void addimages(GtkWidget *wid,gpointer userdata);
	static void removeimages(GtkWidget *wid,gpointer userdata);
//	static void batchprocess(GtkWidget *wid,gpointer userdata);
//	static void showconversiondialog(GtkWidget *wid,gpointer userdata);
	static gboolean progressfunc(gpointer userdata);
	static void showpreferencesdialog(GtkWidget *wid,gpointer userdata);
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, GtkSelectionData *selection_data, guint info, guint time, gpointer data);
	GtkWidget *window;
	// Menu callback functions:
	static void edit_selectall(GtkAction *action,gpointer ob);
	static void edit_selectnone(GtkAction *action,gpointer ob);
	static void edit_preseteditor(GtkAction *action,gpointer ob);
	static void edit_devicelinkeditor(GtkAction *action,gpointer ob);
	static void file_quit(GtkAction *action,gpointer ob);
	static void showaboutdialog(GtkAction *action,gpointer userdata);
	protected:
	GtkUIManager *uimanager;
	GtkWidget *imgsel;
	GtkWidget *notebook;
	GtkWidget *combo;
	GtkWidget *progressbar;
	Callback *progresscallback;
	bool shuttingdown;
	bool timeoutrunning;
	GdkPixbuf *rgbpb;
	GdkPixbuf *cmykpb;
	GdkPixbuf *profpb;
	GdkPixbuf *logopb;
	static GtkActionEntry menu_entries[];
	static const char *menu_ui_description;
};


class ProgressCallback : public Callback
{
	public:
	ProgressCallback(TestUI &ui) : ui(ui)
	{
	}
	virtual ~ProgressCallback()
	{
	}
	virtual void Call()
	{
//		Debug[TRACE] << "**** Callback function called." << endl;
		ui.StartProgressBar();
	}
	protected:
	TestUI &ui;
};



GtkActionEntry TestUI::menu_entries[] = {
  { "FileMenu", NULL, N_("_File") },

  { "Quit", GTK_STOCK_QUIT, N_("_Quit"), "<control>Q", N_("Exit the program"), G_CALLBACK(file_quit) },

  { "EditMenu", NULL, N_("_Edit") },
  
  { "SelectAll", NULL, N_("Select _All"), "<control>A", N_("Select all images"), G_CALLBACK(edit_selectall) },
  { "SelectNone", NULL, N_("Select _None"), NULL, N_("Deselect all images"), G_CALLBACK(edit_selectnone) },
  { "PresetEditor", NULL, N_("Preset _Editor..."), NULL, N_("Open the preset editor"), G_CALLBACK(edit_preseteditor) },
  { "DeviceLinkEditor", NULL, N_("_Device Link Editor..."), NULL, N_("Open the Device Link editor"), G_CALLBACK(edit_devicelinkeditor) },
  { "Preferences", NULL, N_("Pre_ferences..."), NULL, N_("Set paths for colour profiles and external utilities"), G_CALLBACK(showpreferencesdialog) },

  { "HelpMenu", NULL, N_("_Help") },

  { "About", GTK_STOCK_ABOUT, N_("_About"), NULL, N_("Show the About dialog"), G_CALLBACK(showaboutdialog) }
};

const char *TestUI::menu_ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='EditMenu'>"
"      <menuitem action='SelectAll'/>"
"      <menuitem action='SelectNone'/>"
"      <separator/>"
"      <menuitem action='PresetEditor'/>"
"      <menuitem action='DeviceLinkEditor'/>"
"      <menuitem action='Preferences'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"</ui>";



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
	gchar *temp;
	gchar *urilist=temp=g_strdup((const gchar *)selection_data->data);

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

	if(temp)
		g_free(temp);
}


TestUI::TestUI() : CMYKTool_Core()
{
	profilemanager.SetInt("DefaultCMYKProfileActive",1);

	rgbpb=PixbufFromImageData(rgb_data,sizeof(rgb_data));
	cmykpb=PixbufFromImageData(cmyk_data,sizeof(cmyk_data));
	profpb=PixbufFromImageData(profile_data,sizeof(profile_data));
	logopb=PixbufFromImageData(logo_data,sizeof(logo_data));

	window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),FindInt("Win_W"),FindInt("Win_H"));
	gtk_window_move(GTK_WINDOW(window),FindInt("Win_X"),FindInt("Win_Y"));

	gtk_window_set_title (GTK_WINDOW (window), _("CMYKTool"));
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
		(GtkSignalFunc) gtk_main_quit, NULL);
	gtk_widget_show(window);

	GtkWidget *vbox=gtk_vbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(window),vbox);
	gtk_widget_show(vbox);


	// Build menus

	GtkAccelGroup *accel_group;
	uimanager = gtk_ui_manager_new ();

	GError *error=NULL;
	GtkActionGroup *action_group;
	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain(action_group,PACKAGE);
	gtk_action_group_add_actions (action_group, menu_entries, G_N_ELEMENTS (menu_entries), this);
	gtk_ui_manager_insert_action_group (uimanager, action_group, 0);
	
	if (!gtk_ui_manager_add_ui_from_string (uimanager, menu_ui_description, -1, &error))
		throw error->message;

	accel_group = gtk_ui_manager_get_accel_group(uimanager);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	GtkWidget *menubar = gtk_ui_manager_get_widget (uimanager, "/MainMenu");
	gtk_box_pack_start(GTK_BOX(vbox),menubar,FALSE,TRUE,0);
	gtk_widget_show(menubar);


	// HBox to contain image list and preview pane


	GtkWidget *hbox=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,0);
	gtk_widget_show(hbox);

	gtk_drag_dest_set(GTK_WIDGET(hbox),
			  GtkDestDefaults(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP),
			  dnd_file_drop_types, dnd_file_drop_types_count,
                          GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
	g_signal_connect(G_OBJECT(hbox), "drag_data_received",
			 G_CALLBACK(get_dnd_data), this);


	// Leftmost column - imageselector + buttons

	vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),vbox,FALSE,FALSE,0);
	gtk_widget_show(vbox);


	// Add Images button...
	GtkWidget *tmp;

	tmp=gtk_button_new_with_label(_("Add Images..."));
	gtk_signal_connect (GTK_OBJECT (tmp), "clicked",
		(GtkSignalFunc) addimages,this);
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	gtk_widget_show(tmp);	


	tmp=gtk_button_new_with_label(_("Remove Images"));
	gtk_signal_connect (GTK_OBJECT (tmp), "clicked",
		(GtkSignalFunc) removeimages,this);
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	gtk_widget_show(tmp);	


	// ImageSelector

	imgsel=imageselector_new(NULL,GTK_SELECTION_MULTIPLE,false);
	gtk_signal_connect (GTK_OBJECT (imgsel), "double-clicked",
		(GtkSignalFunc) convert,this);
	gtk_box_pack_start(GTK_BOX(vbox),imgsel,TRUE,TRUE,0);
	gtk_widget_show(imgsel);


	// Convert button...


	tmp=gtk_button_new_with_label(_("Convert"));
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



#if 0
	// Preferences button - redundant since we now have menus


	tmp=gtk_button_new_with_label(_("Preferences..."));
	gtk_box_pack_start(GTK_BOX(vbox),tmp,FALSE,FALSE,0);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(showpreferencesdialog),this);
	gtk_widget_show(tmp);
#endif

	progressbar=gtk_progress_bar_new();
	gtk_progress_bar_set_ellipsize(GTK_PROGRESS_BAR(progressbar),PANGO_ELLIPSIZE_END);
	gtk_box_pack_start(GTK_BOX(vbox),progressbar,FALSE,FALSE,0);
//	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(showpreferencesdialog),this);
	gtk_widget_show(progressbar);


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

	progresscallback=new ProgressCallback(*this);
	GetDispatcher().SetAddJobCallback(progresscallback);
	timeoutrunning=shuttingdown=false;
}


TestUI::~TestUI()
{
	shuttingdown=true;	
	while(timeoutrunning)
		gtk_main_iteration();
	if(progresscallback)
	{
		GetDispatcher().SetAddJobCallback(NULL);
		delete progresscallback;
	}
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

	PresetList list;
	opts.Add(PRESET_NONE_ESCAPE,_("None"),_("No conversion"),true);
	previdx=list.GetPreviousIndex()+1;

	for(unsigned int idx=0;idx<list.size();++idx)
	{
		const char *fn=list[idx].filename.c_str();
		const char *dn=list[idx].displayname.c_str();

		if(strlen(dn)<=PRESET_MAXCHARS)
			opts.Add(fn,dn,dn);
		else
		{
			char buf[PRESET_MAXCHARS*4+4];
			utf8ncpy(buf,dn,PRESET_MAXCHARS);
			int n=strlen(buf);
			buf[n]=buf[n+1]=buf[n+2]='.';
			buf[n+3]=0;
			opts.Add(fn,buf,dn);
		}
	}

	opts.Add(PRESET_OTHER_ESCAPE,_("Other..."),_("Create a new preset"),true);

	return(previdx);
}


void TestUI::ProcessImage(const char *filename)
{
	new CMYKUITab(window,notebook,*this,filename);
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
			CMYKConversionOptions_Dialog(*ui,ui->window);
			Debug[TRACE] << "Updating combo..." << endl;

			SimpleComboOptions opts;
			ui->BuildComboOpts(opts);
			simplecombo_set_opts(SIMPLECOMBO(ui->combo),opts);
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



// Menu callback functions

void TestUI::edit_selectall(GtkAction *action,gpointer ob)
{
	TestUI *ui=(TestUI *)ob;
	imageselector_select_all(IMAGESELECTOR(ui->imgsel));
}


void TestUI::edit_selectnone(GtkAction *action,gpointer ob)
{
	TestUI *ui=(TestUI *)ob;
	imageselector_select_none(IMAGESELECTOR(ui->imgsel));
}


void TestUI::file_quit(GtkAction *action,gpointer ob)
{
	gtk_main_quit();
}


void TestUI::edit_preseteditor(GtkAction *action,gpointer ob)
{
	TestUI *ui=(TestUI *)ob;
	CMYKConversionOptions_Dialog(*ui,ui->window);
}


void TestUI::edit_devicelinkeditor(GtkAction *action,gpointer ob)
{
	TestUI *ui=(TestUI *)ob;
	DeviceLink_Dialog(*ui,ui->window);
}


void TestUI::showaboutdialog(GtkAction *action,gpointer ob)
{
	TestUI *ui=(TestUI *)ob;
	const char *authors[]={"Alastair M. Robinson <amr@blackfiveservices.co.uk>",NULL};

	gtk_show_about_dialog (NULL,
		"program-name", "CMYKTool",
		"logo", ui->logopb,
		"title", _("About CMYKTool"),
		"translator-credits", _("translator-credits"),
		"authors", authors,
		"website", "http://www.blackfiveimaging.co.uk",
		"version", VERSION,
		"copyright", _("Copyright (c) 2009-2010 Alastair M. Robinson"),
		"comments",_("A utility for conversion of images using ICC colour profiles."),
		NULL);
}


void TestUI::StartProgressBar()
{
	if(!timeoutrunning && !shuttingdown)
		gtk_timeout_add(200,progressfunc,this);
}


gboolean TestUI::progressfunc(gpointer userdata)
{
	static int tick=0;
	bool result=true;
	TestUI *ui=(TestUI *)userdata;
	if(ui->shuttingdown)
		result=ui->timeoutrunning=false;
	else
	{
		ui->timeoutrunning=true;
		JobDispatcher &jd=ui->GetDispatcher();
		jd.ObtainMutex();
		int c=jd.JobCount(JOBSTATUS_QUEUED|JOBSTATUS_RUNNING);
		jd.ReleaseMutex();
//		Debug[TRACE] << "Jobcount is currently " << c << endl;
		if(c)
		{
			gtk_widget_show(ui->progressbar);
			gtk_progress_bar_pulse(GTK_PROGRESS_BAR(ui->progressbar));
			if((tick&7)==0)
			{
				int entry=tick/8;
				JobMonitorList joblist(ui->GetDispatcher());
				entry%=joblist.size();
				gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ui->progressbar),joblist[entry].name.c_str());
			}
			++tick;
		}
		else
		{
//			gtk_widget_hide(ui->progressbar);
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ui->progressbar),0.0);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ui->progressbar),_("Done"));
			result=ui->timeoutrunning=false;
			tick=0;
		}
	}
	return(result);
}


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


void TestUI::removeimages(GtkWidget *wid,gpointer userdata)
{
	TestUI *ui=(TestUI *)userdata;

	imageselector_remove(IMAGESELECTOR(ui->imgsel));
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

#ifdef WIN32
	char dirname[MAX_PATH]={0};
	SHGetFolderPath(NULL,CSIDL_MYPICTURES,NULL,SHGFP_TYPE(SHGFP_TYPE_CURRENT),dirname);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(sel),dirname);
#else
	char *dirname=substitute_homedir("$HOME");
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(sel),dirname);	
	free(dirname);
#endif

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


#if 0
void TestUI::showconversiondialog(GtkWidget *wid,gpointer userdata)
{
	Debug[TRACE] << "About to display dialog..." << endl;

	TestUI *ui=(TestUI *)userdata;
	CMYKConversionOptions_Dialog(ui->convopts,ui->window);

	Debug[TRACE] << "Updating combo..." << endl;

	SimpleComboOptions opts;
	ui->BuildComboOpts(opts);
	simplecombo_set_opts(SIMPLECOMBO(ui->combo),opts);
}
#endif

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

//			We don't promote here if we can convert using profiles.
//			if(STRIP_ALPHA(is->type==IS_TYPE_GREY))
//				is=new ImageSource_Promote(is,IS_TYPE_RGB);

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
			delete mon;
			if(pb)
				imageselector_add_filename(IMAGESELECTOR(imgsel),filename,pb);
		}
	}
	catch(const char *err)
	{
		ErrorDialogs.AddMessage(err);
	}
}


void ParseOptions(int argc,char *argv[])
{
	static struct option long_options[] =
	{
		{"help",no_argument,NULL,'h'},
		{"version",no_argument,NULL,'v'},
		{"debug",required_argument,NULL,'d'},
		{0, 0, 0, 0}
	};

	while(1)
	{
		int c;
		c = getopt_long(argc,argv,"hvd:",long_options,NULL);
		if(c==-1)
			break;
		switch (c)
		{
			case 'h':
				printf("Usage: %s [options] image1 [image2] ... \n",argv[0]);
				printf("\t -h --help\t\tdisplay this message\n");
				printf("\t -v --version\t\tdisplay version\n");
				printf("\t -d --debug\t\tset debugging level - 0 for silent, 4 for verbose");
				throw 0;
				break;
			case 'v':
				printf("%s\n",PACKAGE_STRING);
				throw 0;
				break;
			case 'd':
				Debug.SetLevel(DebugLevel(atoi(optarg)));
				break;
		}
	}
}


int main(int argc,char *argv[])
{
	gtk_init(&argc,&argv);

	ParseOptions(argc,argv);
#ifdef WIN32
	char *logname=substitute_homedir("$HOME" SEARCHPATH_SEPARATOR_S ".cmyktool_errorlog");
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

		for(int i=optind;i<argc;++i)
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

