#include <iostream>

#include "tiffsaver.h"

#include <stdlib.h>
#include <string.h>

#include "lcmswrapper.h"
#include "imagesource_promote.h"
#include "imagesource_cms.h"
#include "imagesource_deflatten.h"
#include "imagesource_mask.h"
#include "imagesource_montage.h"
#include "imagesource_flatten.h"
#include "imagesource_util.h"

#include "support/util.h"
#include "support/jobqueue.h"
#include "profilemanager/profilemanager.h"

#include "conversionopts.h"


#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif


bool ParseOptions(int argc,char *argv[],CMYKConversionOptions &options)
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
				printf("\t -o --outputprofile\t\tOutput (CMYK) profile\n");
				printf("\t -b --preserveblack\t\tPreserve pure black pixels\n");
				printf("\t -f --overprintblack\t\tFill in background behind black pixels\n");
				printf("\t -g --preservegrey\t\tRestrict grey pixels to the black plate\n");
//				printf("\t -M --mask\t\tApply a mask between normal separation and preserve mode\n");
				throw 0;
				break;
			case 'v':
				printf("deflatten 0.1\n");
				throw 0;
				break;
			case 'b':
				options.SetMode(CMYKCONVERSIONMODE_HOLDBLACK);
				break;
			case 'f':
				options.SetMode(CMYKCONVERSIONMODE_OVERPRINT);
				break;
			case 'g':
				options.SetMode(CMYKCONVERSIONMODE_HOLDGREY);
				break;
			case 'i':
				options.SetInRGBProfile(optarg);
				break;
			case 'o':
				options.SetOutProfile(optarg);
				break;
//			case 'm':
//				options->mask=true;
//				break;
		}
	}
	return(batchmode);
}


class ConversionWorker : public Worker
{
	public:
	ConversionWorker(JobQueue &queue,ProfileManager &pm) : Worker(queue), profilemanager(pm)
	{
		factory=profilemanager.GetTransformFactory();
	}
	virtual ~ConversionWorker()
	{
		WaitCompletion();
		if(factory)
			delete factory;
	}
	ProfileManager &profilemanager;
	CMTransformFactory *factory;
};


class ConversionJob : public Job
{
	public:
	ConversionJob(CMYKConversionOptions &opts,const char *filename) : Job(), opts(opts), filename(filename)
	{
	}
	virtual ~ConversionJob()
	{
	}
	virtual void Run(Worker *w)
	{
		ConversionWorker *tw=(ConversionWorker *)w;

		try
		{
			ImageSource *src(ISLoadImage(filename));
			src=opts.Apply(src,NULL,tw->factory);

			char *outfilename=BuildFilename(filename,"-CMYK","tif");

			cerr << "Saving " << outfilename << endl;
			{
				Progress p;
				TIFFSaver ts(outfilename,ImageSource_rp(src));
				ts.SetProgress(&p);
				ts.Save();
			}
			free(outfilename);
		}
		catch(const char *err)
		{
			cerr << "Error: " << err << endl;
		}
	}
	protected:
	CMYKConversionOptions opts;
	const char *filename;
};


int main(int argc, char **argv)
{
	try
	{
		ConfigFile file;
		ProfileManager profilemanager(&file,"[Colour Management]");

		CMYKConversionOptions opts(profilemanager);
		ParseOptions(argc,argv,opts);

		JobDispatcher jd(0);

		jd.AddWorker(new ConversionWorker(jd,profilemanager));
		jd.AddWorker(new ConversionWorker(jd,profilemanager));
		jd.AddWorker(new ConversionWorker(jd,profilemanager));
		
		for(int i=optind;i<argc;++i)
		{
			jd.AddJob(new ConversionJob(opts,argv[i]));
		}

		jd.WaitCompletion();
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
