#include <iostream>
#include <cstdio>
#include <cstring>

#include <gtk/gtk.h>

#include "imagesource/imagesource_util.h"
#include "imagesource/pixbuf_from_imagesource.h"
#include "miscwidgets/imageselector.h"
#include "miscwidgets/pixbufview.h"
#include "miscwidgets/generaldialogs.h"
#include "support/progresstext.h"
#include "support/util.h"
#include "psripui.h"
#include "debug.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

using namespace std;


int main(int argc,char *argv[])
{
	gtk_init(&argc,&argv);

	Debug.SetLevel(TRACE);

	try
	{
//			PSRipOptions opts;
//			PSRip ripper;
//			ripper.Rip(argv[1],opts);

		PSRipUI ripper;

		if(argc>1)
		{
			char *filename;

#ifdef WIN32
			if(argv[1][1]!=':' && argv[1][0]!='/')
				filename=BuildAbsoluteFilename(argv[1]);
			else
				filename=strdup(argv[1]);
#else
			if(argv[1][0]!='/')
				filename=BuildAbsoluteFilename(argv[1]);
			else
				filename=strdup(argv[1]);
#endif
			ripper.Rip(filename);
			free(filename);
		}

		gtk_main();
	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	return(0);
}

