#include <iostream>

#include "support/progresstext.h"
#include "imageutils/tiffsave.h"
#include "imagesource/imagesource_util.h"

#include "cachedimage.h"

using namespace std;


int main(int argc,char **argv)
{
	try
	{
		if(argc==3)
		{
			ImageSource *is=ISLoadImage(argv[1]);
			CachedImage cache(is);

			is=cache.GetImageSource();
			ProgressText prog;
			TIFFSaver t(argv[2],is);
			t.SetProgress(&prog);
			t.Save();
			delete is;

			is=cache.GetImageSource();
			TIFFSaver t2(argv[2],is);
			t2.SetProgress(&prog);
			t2.Save();
			delete is;

		}
		else
			cerr << "Usage: " << argv[0] << " in.tif out.tif" << endl;
	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	return(0);
}

