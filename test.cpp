#include <deque>
#include <iostream>
#include <deque>

#include <unistd.h>

#include <gtk/gtk.h>

#include "debug.h"
#include "externalghostscript.h"


#define TARGET_URI_LIST 1


static GtkTargetEntry dnd_file_drop_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
static gint dnd_file_drop_types_count = 1;


class DnDTray;

class DropTarget
{
	public:
	DropTarget(DnDTray *parent);
	virtual ~DropTarget();
	virtual void MakeWidget();
	virtual GtkWidget *GetWidget();
	protected:
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
				     GtkSelectionData *selection_data, guint info, guint time, gpointer data);
	static gboolean drag_motion(GtkWidget *widget,GdkDragContext *drag_context,
		gint x,gint y,guint time,gpointer data);
	static void drag_leave(GtkWidget *widget,GdkDragContext *drag_context,
		guint time,gpointer data);
	DnDTray *parent;
	GtkWidget *widget;
};

class DnDTray
{
	public:
	DnDTray() : shown(false), timeoutid(0)
	{
		window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_MOUSE);
		gtk_window_set_default_size(GTK_WINDOW(window),0,0);
		gtk_signal_connect (GTK_OBJECT (window), "delete_event", (GtkSignalFunc) gtk_main_quit, NULL);
		gtk_widget_show(window);

		droptarget=gtk_image_new_from_stock(GTK_STOCK_OK,GTK_ICON_SIZE_DND);
		gtk_container_add(GTK_CONTAINER(window),droptarget);
		gtk_widget_show(droptarget);


		tray=gtk_window_new(GTK_WINDOW_POPUP);
		gtk_window_set_position(GTK_WINDOW(tray),GTK_WIN_POS_MOUSE);
		gtk_window_set_default_size(GTK_WINDOW(tray),0,0);

		GtkWidget *hbox=gtk_hbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(tray),hbox);
		gtk_widget_show(hbox);


		DropTarget *t=new DropTarget(this);
		targets.push_back(t);
		GtkWidget *tmp;
		tmp=t->GetWidget();
		gtk_box_pack_start(GTK_BOX(hbox),tmp,FALSE,FALSE,0);
		gtk_widget_show(tmp);

		gtk_drag_dest_set(GTK_WIDGET(droptarget),
				  GtkDestDefaults(GTK_DEST_DEFAULT_MOTION),
				  dnd_file_drop_types, dnd_file_drop_types_count, GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
		g_signal_connect(G_OBJECT(droptarget), "drag-motion",G_CALLBACK(drag_motion), this);
		g_signal_connect(G_OBJECT(droptarget), "drag-leave",G_CALLBACK(drag_leave), this);
	}
	~DnDTray()
	{
		while(targets.size())
		{
			delete targets[0];
			targets.pop_front();
		}
	}
	void MarkEnter()
	{
		if(timeoutid)
			g_source_remove(timeoutid);
		timeoutid=0;
	}
	void MarkLeave()
	{
		if(timeoutid)
			g_source_remove(timeoutid);
		timeoutid=g_timeout_add_seconds(1,hide_tray,this);
	}
	protected:
	static gboolean drag_motion(GtkWidget *widget,GdkDragContext *drag_context,
		gint x,gint y,guint time,gpointer data)
	{
		DnDTray *tray=(DnDTray *)data;
		tray->MarkEnter();
		if(!tray->shown)
		{
			gtk_widget_show(tray->tray);
			tray->shown=true;
		}
		return(TRUE);
	}
	static void drag_leave(GtkWidget *widget,GdkDragContext *drag_context,
		guint time,gpointer data)
	{
		DnDTray *tray=(DnDTray *)data;
		tray->MarkLeave();
//		gtk_widget_hide(tray->window);
	}
	static gboolean hide_tray(gpointer userdata)
	{
		DnDTray *tray=(DnDTray *)userdata;
		gtk_widget_hide(tray->tray);
		
		if(tray->timeoutid)
			tray->timeoutid=0;

		tray->shown=false;
		
		return(FALSE);	
	}
	GtkWidget *window;
	GtkWidget *droptarget;
	GtkWidget *tray;
	bool shown;
	std::deque<DropTarget *> targets;
	guint timeoutid;
};



DropTarget::DropTarget(DnDTray *parent) : parent(parent), widget(NULL)
{
}

DropTarget::~DropTarget()
{
//	if(widget)
//		gtk_widget_destroy(GTK_WIDGET(widget));
}

void DropTarget::MakeWidget()
{
	Debug[TRACE] << "Creating widget" << std::endl;
	widget=gtk_image_new_from_stock(GTK_STOCK_OK,GTK_ICON_SIZE_DND);
	gtk_drag_dest_set(GTK_WIDGET(widget),
			  GtkDestDefaults(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP),
			  dnd_file_drop_types, dnd_file_drop_types_count, GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
	g_signal_connect(G_OBJECT(widget), "drag_data_received",G_CALLBACK(get_dnd_data), this);
	g_signal_connect(G_OBJECT(widget), "drag-motion",G_CALLBACK(drag_motion), this);
	g_signal_connect(G_OBJECT(widget), "drag-leave",G_CALLBACK(drag_leave), this);
}

GtkWidget *DropTarget::GetWidget()
{
	Debug[TRACE] << "Checking widget" << std::endl;
	if(!widget)
		MakeWidget();
	Debug[TRACE] << "Returning widget" << std::endl;
	return(widget);
}


void DropTarget::get_dnd_data(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
			     GtkSelectionData *selection_data, guint info, guint time, gpointer data)
{
	gchar *savedptr;
	gchar *urilist=savedptr=g_strdup((const gchar *)selection_data->data);
	DropTarget *target=(DropTarget *)data;

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
				// Process file here...
				Debug[TRACE] << "Got filename: " << filename << std::endl;
			}
		}
	}
	g_free(savedptr);
}


gboolean DropTarget::drag_motion(GtkWidget *widget,GdkDragContext *drag_context,
	gint x,gint y,guint time,gpointer data)
{
	DropTarget *target=(DropTarget *)data;
	target->parent->MarkEnter();
	return(TRUE);
}


void DropTarget::drag_leave(GtkWidget *widget,GdkDragContext *drag_context,
	guint time,gpointer data)
{
	DropTarget *target=(DropTarget *)data;
	target->parent->MarkLeave();
}


int main(int argc,char **argv)
{
	Debug.SetLevel(TRACE);
	gtk_init(&argc,&argv);
	try
	{
		DnDTray dndtray;
		gtk_main();
	}
	catch(const char *err)
	{
		Debug[ERROR] << "Error: " << err << std::endl;
	}
	return(0);
}

