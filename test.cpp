#include <deque>
#include <iostream>

#include <unistd.h>

#include <gtk/gtk.h>

#include "debug.h"
#include "externalghostscript.h"


#define TARGET_URI_LIST 1


static GtkTargetEntry dnd_file_drop_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
static gint dnd_file_drop_types_count = 1;


class DnDTray
{
	public:
	DnDTray() : dropzone(false)
	{
		window=gtk_window_new(GTK_WINDOW_POPUP);
		gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_MOUSE);
		gtk_signal_connect (GTK_OBJECT (window), "delete_event", (GtkSignalFunc) gtk_main_quit, NULL);
		gtk_widget_show(window);


		GtkWidget *hbox=gtk_hbox_new(FALSE,0);
		gtk_container_add(GTK_CONTAINER(window),hbox);
		gtk_widget_show(hbox);
		
		droptarget=gtk_image_new_from_stock(GTK_STOCK_OK,GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_box_pack_start(GTK_BOX(hbox),droptarget,FALSE,FALSE,0);
		gtk_widget_show(droptarget);

		droptarget2=gtk_image_new_from_stock(GTK_STOCK_OK,GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_box_pack_start(GTK_BOX(hbox),droptarget2,FALSE,FALSE,0);

		gtk_drag_dest_set(GTK_WIDGET(droptarget),
				  GtkDestDefaults(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP),
				  dnd_file_drop_types, dnd_file_drop_types_count, GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
		g_signal_connect(G_OBJECT(droptarget), "drag_data_received",G_CALLBACK(get_dnd_data), this);
		g_signal_connect(G_OBJECT(droptarget), "drag-motion",G_CALLBACK(drag_motion), this);
		g_signal_connect(G_OBJECT(droptarget), "drag-leave",G_CALLBACK(drag_leave), this);
	}
	~DnDTray()
	{

	}
	protected:
	static void get_dnd_data(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
				     GtkSelectionData *selection_data, guint info, guint time, gpointer data)
	{
		gchar *savedptr;
		gchar *urilist=savedptr=g_strdup((const gchar *)selection_data->data);
		DnDTray *tray=(DnDTray *)data;

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
				}
			}
		}
		g_free(savedptr);
	}
	static gboolean drag_motion(GtkWidget *widget,GdkDragContext *drag_context,
		gint x,gint y,guint time,gpointer data)
	{
		DnDTray *tray=(DnDTray *)data;
		if(!tray->dropzone)
		{
			gtk_widget_show(tray->droptarget2);
		}
		return(TRUE);
	}
	static void drag_leave(GtkWidget *widget,GdkDragContext *drag_context,
		guint time,gpointer data)
	{
		DnDTray *tray=(DnDTray *)data;
		gtk_widget_hide(tray->window);
	}
	GtkWidget *window;
	GtkWidget *droptarget;
	GtkWidget *droptarget2;
	bool dropzone;
};


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

