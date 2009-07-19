#include <iostream>
#include <cstdio>

#include <gtk/gtk.h>

#include "imagesource/imagesource_util.h"
#include "imagesource/pixbuf_from_imagesource.h"
#include "miscwidgets/imageselector.h"
#include "miscwidgets/pixbufview.h"
#include "miscwidgets/generaldialogs.h"
#include "support/progressbar.h"
#include "support/progresstext.h"
#include "psripui.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;


int main(int argc,char *argv[])
{
	gtk_init(&argc,&argv);

	if(argc>1)
	{
		try
		{
//			PSRipOptions opts;
//			PSRip ripper;
//			ripper.Rip(argv[1],opts);


			PSRipUI ripper;
			ripper.Rip(argv[1]);

			gtk_main();
		}
		catch(const char *err)
		{
			cerr << "Error: " << err << endl;
		}
	}
	return(0);
}

