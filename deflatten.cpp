#include <iostream>

#include "tiffsave.h"

#include <stdlib.h>
#include <string.h>

#include "lcmswrapper.h"
#include "imagesource_cms.h"
#include "imagesource_deflatten.h"
#include "imagesource_mask.h"
#include "imagesource_montage.h"
#include "imagesource_flatten.h"

#include "imagesource_util.h"


#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif

struct DFOptions
{
	bool preserveblack;
	bool overprintblack;
	bool preservegrey;
	bool mask;
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
		{"mask",no_argument,NULL,'m'},
		{0, 0, 0, 0}
	};

	while(1)
	{
		int c;
		c = getopt_long(argc,argv,"hvi:o:bfgm",long_options,NULL);
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
				printf("\t -M --mask\t\tApply a mask between normal separation and preserve mode\n");
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
			case 'm':
				options->mask=true;
				break;
		}
	}
	return(batchmode);
}


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

		DFOptions opts;
		opts.overprintblack=false;
		opts.preserveblack=false;
		opts.preservegrey=false;
		opts.mask=false;
		opts.sourceprofile="sRGB Color Space Profile.icm";
		opts.destprofile="USWebCoatedSWOP.icc";

		ParseOptions(argc,argv,&opts);
		
		cerr << "PreserveBlack: " << opts.preserveblack << endl;
		cerr << "Overprint: " << opts.overprintblack << endl;
		cerr << "PreserveGrey: " << opts.preservegrey << endl;

		CMSTransform *transform=NULL;
		CMSProfile in(opts.sourceprofile);
		if(in.IsDeviceLink())
		{
			cerr << "Using DeviceLink profile" << endl;
			transform=new CMSTransform(&in);
			if(!transform)
				throw "Can't create transform";
		}
		else
		{
			CMSProfile out(opts.destprofile);
			transform=new CMSTransform(&in,&out);
			if(!transform)
				throw "Can't create transform";
		}

		for(int i=optind;i<argc;++i)
		{
			src=ISLoadImage(argv[i]);
			src=new ImageSource_Deflatten(src,transform,opts.preserveblack,opts.overprintblack,opts.preservegrey);

			char *filename=buildfilename(argv[i],"CMYK","tif");

			if(opts.mask && ((i+1)<argc))
			{
				ImageSource *src2=ISLoadImage(argv[i]);
				ImageSource *mask=ISLoadImage(argv[i+1]);
				ImageSource *normal=new ImageSource_CMS(src2,transform);
				normal=new ImageSource_Mask(normal,mask);
				ImageSource_Montage *comp=new ImageSource_Montage(src->type,int(src->xres));
				comp->Add(normal,0,0);
				comp->Add(src,0,0);
				src=comp;
				++i;
			}

			cerr << "Saving " << filename << endl;
			{
				Progress p;
				TIFFSaver ts(filename,src);
				ts.SetProgress(&p);
				ts.Save();
			}
			delete src;
			free(filename);
		}
		if(transform)
			delete transform;
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
