#include <iostream>
#include <vector>

#include "tiffsaver.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

#include "lcmswrapper.h"
#include "imagesource_tee.h"
#include "imagesource_cms.h"
#include "imagesource_extractchannel.h"

#include "imagesource_util.h"

#include "debug.h"

#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif


int main(int argc, char **argv)
{
	Debug.SetLevel(TRACE);
	try
	{
		for(int i=optind;i<argc;++i)
		{
			ImageSource_rp src=ImageSource_rp(ISLoadImage(argv[i]));
			Progress p;
			std::vector<RefCountPtr<TIFFSaver> > savers;

			switch(STRIP_ALPHA(src->type))
			{
				case IS_TYPE_RGB:
					{
						Debug[TRACE] << "RGB" << std::endl;
						char *filename;
						const char *channelnames[]={"-R","-G","-B","-A"};
						for(int channel=0;channel<src->samplesperpixel;++channel)
						{
							ImageSource *s=new ImageSource_Tee(&*src);
							s=new ImageSource_ExtractChannel(s,channel);
							filename=BuildFilename(argv[i],channelnames[channel],"tif");
							RefCountPtr<TIFFSaver> ts(new TIFFSaver(filename,ImageSource_rp(s)));
							savers.push_back(ts);
							free(filename);
						}
					}
					break;
				case IS_TYPE_CMYK:
					{
						Debug[TRACE] << "CMYK" << std::endl;
						char *filename;
						const char *channelnames[]={"-C","-M","-Y","-K","-A"};
						for(int channel=0;channel<src->samplesperpixel;++channel)
						{
							ImageSource *s=new ImageSource_Tee(&*src);
							s=new ImageSource_ExtractChannel(s,channel);
							filename=BuildFilename(argv[i],channelnames[channel],"tif");
							RefCountPtr<TIFFSaver> ts(new TIFFSaver(filename,ImageSource_rp(s)));
							savers.push_back(ts);
							free(filename);
						}
					}
					break;
				default:
					throw "Only RGB and CMYK images are currently supported";
			}
			for(int row=0;row<src->height;++row)
			{
				for(unsigned int i=0;i<savers.size();++i)
				{
					savers[i]->ProcessRow(row);
				}
				p.DoProgress(row,src->height);
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
