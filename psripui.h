#ifndef PSRIPUI_H

#include <gtk/gtk.h>
#include "psrip_core.h"

class PSRipUIThread;
class PSRipUI : public SearchPathHandler
{
	public:
	PSRipUI();
	~PSRipUI();
	void Rip(const char *filename);
	static void image_changed(GtkWidget *widget,gpointer userdata);
	protected:
	GtkWidget *window;
	GtkWidget *imgsel;
	GtkWidget *pbview;
	GdkPixbuf *currentimage;
	PSRipUIThread *monitorthread;
	friend class PSRipUIThread;
};

#endif

