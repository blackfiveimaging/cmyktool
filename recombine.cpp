#include <iostream>

#include "tiffsave.h"

#include <stdlib.h>
#include <string.h>

#include "lcmswrapper.h"
#include "imagesource_cms.h"
#include "imagesource_extractchannel.h"
#include "imagesource_plane.h"
#include "imagesource_util.h"


#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif

#if 0
struct DFOptions
{
	bool preserveblack;
	bool overprintblack;
	bool preservegrey;
	char *sourceprofile;
	char *destprofile;
};


bool ParseOptions(int argc,char *argv[],DFOptions *options)
{
	int batchmode=false;
	static struct option long_options[] =
	{
		{"help",no_argument,NULL,'h'},
		{"version",no_argument,NULL,'v'},
		{"inputprofile",required_argument,NULL,'i'},
		{"outputprofile",required_argument,NULL,'o'},
		{"preserveblack",no_argument,NULL,'p'},
		{"overprintblack",no_argument,NULL,'f'},
		{"preservegrey",no_argument,NULL,'g'},
		{0, 0, 0, 0}
	};

	while(1)
	{
		int c;
		c = getopt_long(argc,argv,"hvi:o:bfg",long_options,NULL);
		if(c==-1)
			break;
		switch (c)
		{
			case 'h':
				printf("Usage: %s [options] image1 [image2] ... \n",argv[0]);
				printf("\t -h --help\t\tdisplay this message\n");
				printf("\t -v --version\t\tdisplay version\n");
				printf("\t -i --inputprofile\t\tSource (RGB) profile\n");
				printf("\t -o --outputprofile\t\tSource (CMYK) profile\n");
				printf("\t -b --preserveblack\t\tPreserve pure black pixels\n");
				printf("\t -f --overprintblack\t\tFill in background behind black pixels\n");
				printf("\t -g --preservegrey\t\tRestrict grey pixels to the black plate\n");
				throw 0;
				break;
			case 'v':
				printf("deflatten 0.1\n");
				throw 0;
				break;
			case 'b':
				options->preserveblack=true;
				break;
			case 'f':
				options->overprintblack=true;
				options->preserveblack=true;
				break;
			case 'g':
				options->preservegrey=true;
				break;
			case 'i':
				options->sourceprofile=optarg;
				break;
			case 'o':
				options->destprofile=optarg;
				break;
		}
	}
	return(batchmode);
}
#endif


void GetColourFromFilename(CMYKValue *c,const char *fn)
{
	int sl=strlen(fn)-1;
	c->c=0;
	c->m=0;
	c->y=0;
	c->k=0;
	while(sl>=0)
	{
		if(fn[sl]=='.' && sl>0)
		{
			switch(fn[sl-1]&~32)
			{
				case 'C':
					c->c=255;
					break;
				case 'M':
					c->m=255;
					break;
				case 'Y':
					c->y=255;
					break;
				case 'K':
					c->k=255;
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

//		ParseOptions(argc,argv,&opts);

		for(int i=optind;i<argc;++i)
		{
			CMYKValue col;
			cerr << "Getting colour from " << argv[i] << endl;
			GetColourFromFilename(&col,argv[i]);
			col.Dump();
			ImageSource *img=ISLoadImage(argv[i]);
			src=new ImageSource_Plane(img,&col,src);
		}
		if(src)
		{
			char *fn=GetRootFilename(argv[optind]);
			cerr << "Filename " << fn << endl;
			TIFFSaver ts(fn,src);
			Progress p;
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
