#include <iostream>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>

#include "imagesource/imagesource.h"
#include "support/util.h"
#include "support/thread.h"
#include "support/threadutil.h"
#include "psrip_core.h"

using namespace std;


// Slight variation on the usual TempFile - because GhostScript uses a format string for output filenames
// the TempFiles don't have freedom to pick their own names.

class PSRip_TempFile : public TempFile
{
	public:
	PSRip_TempFile(TempFileTracker *header,const char *fname) : TempFile(header)
	{
		filename=strdup(fname);
	}
	virtual ~PSRip_TempFile()
	{
	}
	virtual const char *Filename()
	{
		return(filename);
	}
	protected:
};


// Subthread to RIP the file using GS.
// Uses fork()/exec() to make the process cancellable.


#define GS_ARGC 8
class Thread_PSRipProcess : public ThreadFunction, public Thread
{
	public:
	Thread_PSRipProcess(PSRip &rip,const char *filename,IS_TYPE type,int resolution, int firstpage, int lastpage)
		: ThreadFunction(), Thread(this), rip(rip), argc(0), forkpid(0)
	{
		for(int i=0;i<(GS_ARGC+1);++i)
			argv[i]=NULL;

		// Build a GhostScript command according to the following format:
		// %s -sDEVICE=%s -sOutputFile=%s_%%d.tif -r%dx%d %s -dBATCH -dNOPAUSE %s

		// Locate GhostScript executable...
#ifdef WIN32
		argv[argc]=rip.searchpath.SearchPaths("gswin32c.exe");
#else
		argv[argc]=rip.searchpath.SearchPaths("gs");
#endif
		if(!argv[argc])
			throw "Can't locate GhostScript executable!";

		++argc;

		// Select an appropriate GS device for output...

		switch(STRIP_ALPHA(type))
		{
			case IS_TYPE_BW:
				argv[argc]=strdup("-sDEVICE=tifflzw");
				break;
			case IS_TYPE_GREY:
				argv[argc]=strdup("-sDEVICE=tiffgray");
				break;
			case IS_TYPE_RGB:
				argv[argc]=strdup("-sDEVICE=tiff24nc");
				break;
			case IS_TYPE_CMYK:
				argv[argc]=strdup("-sDEVICE=tiff32nc");
				break;
			case IS_TYPE_DEVICEN:
				// FIXME: add support for DeviceN using the tiffsep device.
			default:
				throw "PSRip: type not yet supported";
				break;
		}
		cerr << "Using Ghostscript device: " << argv[argc] << endl;
		++argc;

		// Create FirstPage and LastPage paramters, if needed...	
		if(firstpage)
		{
			const char *pagefmt=" -dFirstPage=%d";
			int bl=strlen(pagefmt)+20;
			argv[argc]=(char *)malloc(bl);
			snprintf(argv[argc],bl,pagefmt,firstpage);
			++argc;
		}
		if(lastpage)
		{
			const char *pagefmt=" -dFirstPage=%d";
			int bl=strlen(pagefmt)+20;
			argv[argc]=(char *)malloc(bl);
			snprintf(argv[argc],bl,pagefmt,lastpage);
			++argc;
		}

		// Build output name;
		const char *outfmt="-sOutputFile=%s_%%d.tif";
		int outl=strlen(rip.tempname)+strlen(outfmt)+1;
		argv[argc]=(char *)malloc(outl);
		snprintf(argv[argc],outl,outfmt,rip.tempname);
		++argc;

		// Build resolution;
		const char *resfmt="-r%dx%d";
		int resl=strlen(resfmt)+20;
		argv[argc]=(char *)malloc(resl);
		snprintf(argv[argc],resl,resfmt,resolution,resolution);
		++argc;

		argv[argc++]=strdup("-dBATCH");
		argv[argc++]=strdup("-dNOPAUSE");
		argv[argc++]=strdup(filename);
		argv[argc]=NULL;

		Start();
		WaitSync();
	}
	virtual ~Thread_PSRipProcess()
	{
		for(int i=0;i<(GS_ARGC+1);++i)
		{
			if(argv[i])
				free(argv[i]);
		}
	}
	virtual int Entry(Thread &t)
	{
		for(int i=0;i<argc;++i)
		{
			if(argv[i])
				cerr << "Argument: " << argv[i] << endl;
		}
		SendSync();
		switch((forkpid=fork()))
		{
			case -1:
				throw "Unable to launch subprocess";
				break;
			case 0:
				cerr << "Subprocess running..." << endl;
				execv(argv[0],&argv[0]);
				break;
			default:
				cerr << "Waiting for subprocess to complete..." << endl;
				int status;
				waitpid(forkpid,&status,0);
				cerr << "Subprocess complete." << endl;
				break;
		}		
		return(0);
	}
	protected:
	PSRip &rip;
	int argc;
	char *argv[GS_ARGC+1];
	int forkpid;
};


// SubThread to hang around waiting for files to be rendered by GS.
// FIXME - use notification rather than polling.


class Thread_PSRipFileMonitor : public ThreadFunction, public Thread
{
	public:
	Thread_PSRipFileMonitor(PSRip &rip) : ThreadFunction(), Thread(this), rip(rip)
	{
		Start();
		WaitSync();
	}
	virtual ~Thread_PSRipFileMonitor()
	{
	}
	virtual int Entry(Thread &t)
	{
		SendSync();

		cerr << "Monitor thread active" << endl;

		// scan the output files and add them to the TempFileTracker

		int page=1;
		char *rfn=NULL;
		while(!rip.ripthread->TestFinished())
		{
//			cerr << "Thread not yet finished - waiting for page " << page+1 << endl;
			if((rfn=rip.GetRippedFilename(page+1)))
			{
				cerr << "File monitor thread found file: " << rfn << endl;
				free(rfn);
				if((rfn=rip.GetRippedFilename(page)))
				{
					cerr << "So now safe to add: " << rfn << endl;
					new PSRip_TempFile(&rip,rfn);
					++page;
					free(rfn);
					cerr << "Triggering event" << endl;
					rip.Event.Trigger();
				}
			}
			else
			{
#ifdef WIN32
				sleep(100);
#else
				usleep(100000);
#endif
			}
		}
		while((rfn=rip.GetRippedFilename(page)))
		{
			cerr << "Thread finished -- adding file: " << rfn << endl;
			new PSRip_TempFile(&rip,rfn);
			++page;
			free(rfn);
		}
		rip.Event.Trigger();
		return(0);
	}
	protected:
	PSRip &rip;
};



PSRip::PSRip(SearchPathHandler &searchpath)
	: TempFileTracker(), ThreadEventHandler(), Event(*this,"FileReady"),
	searchpath(searchpath), tempname(), ripthread(NULL), monitorthread(NULL)
{
}


void PSRip::Rip(const char *filename,IS_TYPE type,int resolution,int firstpage,int lastpage)
{
	if(monitorthread)
		delete monitorthread;
	monitorthread=NULL;

	if(ripthread)
		delete ripthread;
	ripthread=NULL;

	// Create temporary output filename
	if(tempname)
		free(tempname);
	tempname=tempnam(NULL,"PSRIP");
	cerr << "Using tempname: " << tempname << endl;

#if 0
	// Locate GhostScript executable...
#ifdef WIN32
	char *gspath=searchpath.SearchPaths("gswin32c.exe");
#else
	char *gspath=searchpath.SearchPaths("gs");
#endif
	if(!gspath)
		throw "Can't locate GhostScript executable!";


	// Select an appropriate GS device for output...

	const char *gsdev=NULL;
	switch(STRIP_ALPHA(type))
	{
		case IS_TYPE_BW:
			gsdev="tifflzw";
			break;
		case IS_TYPE_GREY:
			gsdev="tiffgray";
			break;
		case IS_TYPE_RGB:
			gsdev="tiff24nc";
			break;
		case IS_TYPE_CMYK:
			gsdev="tiff32nc";
			break;
		case IS_TYPE_DEVICEN:
			// FIXME: add support for DeviceN using the tiffsep device.
		default:
			throw "PSRip: type not yet supported";
			break;
	}
	cerr << "Using Ghostscript device: " << gsdev << endl;


	// Create FirstPage and LastPage paramters, if needed...	

	char *fpage=NULL;
	char *lpage=NULL;
	if(firstpage)
	{
		const char *pagefmt=" -dFirstPage=%d";
		int bl=strlen(pagefmt)+20;
		fpage=(char *)malloc(bl);
		snprintf(fpage,bl,pagefmt,firstpage);
	}
	if(lastpage)
	{
		const char *pagefmt=" -dFirstPage=%d";
		int bl=strlen(pagefmt)+20;
		lpage=(char *)malloc(bl);
		snprintf(lpage,bl,pagefmt,lastpage);
	}
	char *pages=SafeStrcat(fpage,lpage);	// Will be a valid empty string if both are NULL.
											// Must be free()d.
	// Having built "pages", don't need these any more.
	if(fpage)
		free(fpage);
	if(lpage)
		free(lpage);

	// Create temporary output filename
	tempname=tempnam(NULL,"PSRIP");

	// Now build the actual command.
	const char *gsfmtstring="%s -sDEVICE=%s -sOutputFile=%s_%%d.tif -r%dx%d %s -dBATCH -dNOPAUSE %s";
	int cmdlen=strlen(gsfmtstring)+10+strlen(gspath)+strlen(gsdev)+strlen(pages)+strlen(filename);
	char *gscmd=(char *)malloc(cmdlen);
	snprintf(gscmd,cmdlen,gsfmtstring,gspath,gsdev,tempname,resolution,resolution,pages,filename);
	free(pages);
	free(gspath);

	cerr << "Built command: " << gscmd << endl;
#endif

	// Execute the command...

	ripthread=new Thread_PSRipProcess(*this,filename,type,resolution,firstpage,lastpage);
	cerr << "Created ripthread: " << long(ripthread) << endl;

	monitorthread=new Thread_PSRipFileMonitor(*this);
}


bool PSRip::TestFinished()
{
	int result=true;
	if(monitorthread)
		result&=monitorthread->TestFinished();
	if(ripthread)
		result&=ripthread->TestFinished();
	return(result);
}


PSRip::~PSRip()
{
	cerr << "Disposing of ripthread - " << long(ripthread) << endl;
	if(monitorthread)
		delete monitorthread;
	if(ripthread)
		delete ripthread;
	cerr << "Disposing of tempname" << endl;
	if(tempname)
		free(tempname);
}


char *PSRip::GetRippedFilename(int page)
{
	char *buf=(char *)malloc(strlen(tempname)+10);
	snprintf(buf,strlen(tempname)+10,"%s_%d.tif",tempname,page);
	if(!CheckFileExists(buf))
	{
		free(buf);
		buf=NULL;
	}
	return(buf);
}	

