#include <iostream>

#include <stdlib.h>
#include <string.h>

#include "imageutils/tiffsave.h"
#include "support/progresstext.h"

#include "lcmswrapper.h"
#include "imagesource_cms.h"
#include "imagesource_extractchannel.h"
#include "imagesource_combine.h"
#include "imagesource_util.h"


#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif


void GetColourFromFilename(ISDeviceNValue &c,const char *fn)
{
	int sl=strlen(fn)-1;
	c[0]=0;
	c[1]=0;
	c[2]=0;
	c[3]=0;
	while(sl>=0)
	{
		if(fn[sl]=='.' && sl>0)
		{
			switch(fn[sl-1]&~32)
			{
				case 'C':
					c[0]=IS_SAMPLEMAX;
					break;
				case 'M':
					c[1]=IS_SAMPLEMAX;
					break;
				case 'Y':
					c[2]=IS_SAMPLEMAX;
					break;
				case 'K':
					c[3]=IS_SAMPLEMAX;
					break;
			}
			return;
		}
		--sl;
	}
}


char *GetRootFilename(const char *fn)
{
	int sl=strlen(fn)-1;
	while(sl>=0)
	{
		if(fn[sl]=='.' && sl>1 && fn[sl-2]=='-')
		{
			char *result=strdup(fn);
			while(result[sl])
			{
				result[sl-2]=result[sl];
				++sl;
			}
			result[sl-2]=0;
			return(result);
		}
		--sl;
	}
	return(NULL);
}


int main(int argc, char **argv)
{
	try
	{
		ImageSource *src=NULL;

		for(int i=1;i<argc;++i)
		{
			cerr << "Index " << i << endl;
			ISDeviceNValue col(4);
			cerr << "Getting colour from " << argv[i] << endl;
			GetColourFromFilename(col,argv[i]);
			ImageSource *img=ISLoadImage(argv[i]);
			src=new ImageSource_Combine(img,col,src);
		}
		cerr << "Checking combined image and saving..." << endl;
		if(src)
		{
			char *fn=GetRootFilename(argv[optind]);
			cerr << "Filename " << fn << endl;
			TIFFSaver ts(fn,src);
			ProgressText p;
			ts.SetProgress(&p);
			ts.Save();
	
			delete src;
			free(fn);
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
