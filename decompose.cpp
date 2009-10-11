#include <iostream>

#include "tiffsave.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

#include "lcmswrapper.h"
#include "imagesource_cms.h"
#include "imagesource_extractchannel.h"

#include "imagesource_util.h"


#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif


int main(int argc, char **argv)
{
	try
	{
		ImageSource *src;

		for(int i=optind;i<argc;++i)
		{
			src=ISLoadImage(argv[i]);
			switch(STRIP_ALPHA(src->type))
			{
				case IS_TYPE_RGB:
					cerr << "RGB" << endl;
					for(int channel=0;channel<3;++channel)
					{
						const char *channelnames[]={"R","G","B"};
						if(!src)
							src=ISLoadImage(argv[i]);
						src=new ImageSource_ExtractChannel(src,channel);
						Progress p;
						char *filename=BuildFilename(argv[i],channelnames[channel],"tif");
						cerr << "Saving " << filename << endl;
						{
							TIFFSaver ts(filename,src);
							ts.SetProgress(&p);
							ts.Save();
						}
						delete src;
						src=NULL;
						free(filename);
					}
					break;
				case IS_TYPE_CMYK:
					cerr << "CMYK" << endl;
					for(int channel=0;channel<4;++channel)
					{
						const char *channelnames[]={"C","M","Y","K"};
						if(!src)
							src=ISLoadImage(argv[i]);
						src=new ImageSource_ExtractChannel(src,channel);
						Progress p;
						char *filename=BuildFilename(argv[i],channelnames[channel],"tif");
						cerr << "Saving " << filename << endl;
						{
							TIFFSaver ts(filename,src);
							ts.SetProgress(&p);
							ts.Save();
						}
						delete src;
						src=NULL;
						free(filename);
					}
					break;
				default:
					throw "Only RGB and CMYK images are currently supported";
			}
		}
	}
	catch(const char *err)
	{
		cerr << "Error: " << err << endl;
	}
	catch(int i)
	{
	}
	return(0);
}
