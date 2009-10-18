#include <iostream>

#include <gtk/gtk.h>

#include "uitab.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;


bool UITab::style_applied=false;
void UITab::apply_style()
{
	if(style_applied)
		return;
	gtk_rc_parse_string("style \"mystyle\"\n"
						"{\n"
						"	GtkWidget::focus-padding = 0\n"
						"	GtkWidget::focus-line-width = 0\n"
						"	xthickness = 0\n"
						"	ythickness = 0\n"
						"}\n"
						"widget \"*.tab-close-button\" style \"mystyle\"\n");
	style_applied=true;
}


UITab::UITab(GtkWidget *notebook,const char *tabname) : notebook(notebook)
{
	apply_style();

	label=gtk_hbox_new(FALSE,0);
	GtkWidget *tmp=gtk_label_new(tabname);
	gtk_box_pack_start(GTK_BOX(label),tmp,TRUE,TRUE,0);

	GtkWidget *closeimg=gtk_image_new_from_stock(GTK_STOCK_CLOSE,GTK_ICON_SIZE_MENU);
	tmp=gtk_button_new();
	gtk_widget_set_name(tmp,"tab-close-button");
	gtk_button_set_image(GTK_BUTTON(tmp),closeimg);
	gtk_button_set_relief(GTK_BUTTON(tmp),GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(tmp),"clicked",G_CALLBACK(deleteclicked),this);
	g_signal_connect(G_OBJECT(tmp),"style-set",G_CALLBACK(setclosebuttonsize),this);
	gtk_box_pack_start(GTK_BOX(label),tmp,FALSE,FALSE,4);
	gtk_widget_show_all(label);
	
	hbox=gtk_hbox_new(FALSE,0);
	gtk_widget_show(GTK_WIDGET(hbox));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),GTK_WIDGET(hbox),label);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),-1);
    gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK(notebook),hbox, TRUE, TRUE,GTK_PACK_START);
}


UITab::~UITab()
{
	int page=gtk_notebook_page_num(GTK_NOTEBOOK(notebook),hbox);
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook),page);
}


GtkWidget *UITab::GetBox()
{
	return(hbox);
}


void UITab::deleteclicked(GtkWidget *wid,gpointer userdata)
{
	UITab *ui=(UITab *)userdata;
	// Disable the close button here in case it gets clicked again while we're
	// waiting for thread termination.
	gtk_widget_set_sensitive(wid,FALSE);
	delete ui;
}


void UITab::setclosebuttonsize(GtkWidget *wid,GtkStyle *style,gpointer userdata)
{
	GtkSettings *settings=gtk_widget_get_settings(wid);
	int x,y;
	gtk_icon_size_lookup_for_settings(settings,GTK_ICON_SIZE_MENU,&x,&y);
	gtk_widget_set_size_request(wid,x+2,y+2);
}


