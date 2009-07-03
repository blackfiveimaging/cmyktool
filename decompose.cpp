#include <iostream>

#include "tiffsave.h"

#include <stdlib.h>
#include <string.h>

#include "lcmswrapper.h"
#include "imagesource_cms.h"
#include "imagesource_extractchannel.h"

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

char *buildfilename(char *root,char *suffix,char *fileext)
{
	/* Build a filename like <imagename>-<channel>.<extension> */
	char *extension;

	char *filename=(char *)malloc(strlen(root)+strlen(suffix)+4);

	root=strdup(root);
	extension = root + strlen (root) - 1;
	while (extension >= root)
	{
		if (*extension == '.') break;
		extension--;
	}
	if (extension >= root)
	{
		*(extension++) = '\0';
		sprintf(filename,"%s-%s.%s", root, suffix, fileext);
	}
	else
		sprintf(filename,"%s-%s", root, suffix);
	free(root);

	return(filename);
}


int main(int argc, char **argv)
{
	try
	{
		ImageSource *src;

//		ParseOptions(argc,argv,&opts);

		for(int i=optind;i<argc;++i)
		{
			src=ISLoadImage(argv[i]);
			switch(STRIP_ALPHA(src->type))
			{
				case IS_TYPE_RGB:
					cerr << "RGB" << endl;
					for(int channel=0;channel<3;++channel)
					{
						char *channelnames[]={"R","G","B"};
						if(!src)
							src=ISLoadImage(argv[i]);
						src=new ImageSource_ExtractChannel(src,channel);
						Progress p;
						char *filename=buildfilename(argv[i],channelnames[channel],"tif");
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
						char *channelnames[]={"C","M","Y","K"};
						if(!src)
							src=ISLoadImage(argv[i]);
						src=new ImageSource_ExtractChannel(src,channel);
						Progress p;
						char *filename=buildfilename(argv[i],channelnames[channel],"tif");
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
