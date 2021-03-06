#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>

#ifndef WIN32
#include <sys/wait.h>
#endif

#include "imagesource.h"
#include "util.h"
#include "thread.h"
#include "threadutil.h"
#include "psrip_core.h"

using namespace std;


// Slight variation on the usual TempFile - because GhostScript uses a format string for output filenames
// the TempFiles don't have freedom to pick their own names.

class PSRip_TempFile : public TempFile
{
	public:
	PSRip_TempFile(TempFileTracker &header,const char *fname) : TempFile(&header)
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


#define GS_ARGC 9
class Thread_PSRipProcess : public ThreadFunction, public Thread
{
	public:
	Thread_PSRipProcess(PSRip &rip,const char *filename,PSRipOptions &opt)
		: ThreadFunction(), Thread(this), rip(rip), opt(opt), filename(filename)
	{
		Start();
		WaitSync();
	}
	virtual ~Thread_PSRipProcess()
	{
	}
	virtual int Entry(Thread &t)
	{
		SendSync();
		Debug[TRACE] << "PSRipProcess: Sent sync to main thread" << endl;
		opt.RunProgram(filename);
		return(0);
	}
	virtual void Stop()
	{
		opt.StopProgram();
		Thread::Stop();
	}
	protected:
	PSRip &rip;
	PSRipOptions &opt;
	std::string filename;
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
		try
		{
			Debug[TRACE] << "*** Monitor thread active" << std::endl;

			// scan the output files and add them to the TempFileTracker

			int page=1;
			char *rfn=NULL;
			while(!rip.ripthread->TestFinished())
			{
	//			cerr << "Thread not yet finished - waiting for page " << page+1 << endl;
				if(rip.TestPage(page+1))
				{
					Debug[TRACE] << "File monitor thread found page " << page+1 << std::endl;
					if((rfn=rip.GetRippedFilename(page)))
					{
						Debug[TRACE] << "So now safe to add: " << rfn << std::endl;
						new PSRip_TempFile(rip,rfn);
						++page;
						free(rfn);
						cerr << "Triggering event" << endl;
						rip.Event.Trigger();
					}
				}
				else
				{
	//				Debug[TRACE] << "File not found, so sleeping..." << std::endl;
	#ifdef WIN32
					Sleep(100);
	#else
					usleep(100000);
	#endif
	//				Debug[TRACE] << "Woken from sleep - trying again..." << std::endl;
				}
			}
			while(rip.TestPage(page))
			{
				rfn=rip.GetRippedFilename(page);
				cerr << "Thread finished -- adding file: " << rfn << endl;
				new PSRip_TempFile(rip,rfn);
				++page;
				free(rfn);
			}
			// FIXME - need to consider concurrency here...
		}
		catch(const char *err)
		{
			Debug[ERROR] << "Error: " << err << endl;
		}
		rip.Event.Trigger();
		return(0);
	}
	protected:
	PSRip &rip;
};



PSRip::PSRip()
	: TempFileTracker(), ThreadEventHandler(), Event(*this,"FileReady"),
	tempname(), ripthread(NULL), monitorthread(NULL)
{
}


void PSRip::Rip(const char *filename,PSRipOptions &opts)
{
	if(monitorthread)
		delete monitorthread;
	monitorthread=NULL;

	if(ripthread)
		delete ripthread;
	ripthread=NULL;

	// Create temporary output filename
	tempname=BuildFilename(filename,"","");

	ripthread=new Thread_PSRipProcess(*this,filename,opts);

//  Disable monitorthread until we can determine whether it's causing Ghostscript locks.
//  Don't think it is - think the problem was actually with having the *input* file open from
//  another thread, so re-enabling for now...
	monitorthread=new Thread_PSRipFileMonitor(*this);
}


void PSRip::Stop()
{
	if(ripthread)
		ripthread->Stop();
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


void PSRip::WaitFinished()
{
	if(monitorthread)
		monitorthread->WaitFinished();
	if(ripthread)
		ripthread->WaitFinished();
}


PSRip::~PSRip()
{
	if(monitorthread)
		delete monitorthread;
	if(ripthread)
		delete ripthread;
	cerr << "Disposing of tempname" << endl;
	if(tempname)
		free(tempname);
}


char *PSRip::GetRippedFilename(int page,const char *basename)
{
	if(!basename)
		basename=tempname;
	if(!basename)
		throw "PSRip: Don't have a tempname yet!";
	char *buf=(char *)malloc(strlen(basename)+10);
	snprintf(buf,strlen(basename)+10,"%s_%03d.tif",basename,page);
	return(buf);
}


bool PSRip::TestPage(int page)
{
	char *fn=GetRippedFilename(page);
	bool result=CheckFileExists(fn);
	free(fn);
	return(result);
}


// PSRipOptions


PSRipOptions::PSRipOptions(IS_TYPE type,int resolution,int firstpage,int lastpage,bool textantialias,bool gfxantialias)
	: ExternalGhostScript(), type(type),resolution(resolution),firstpage(firstpage),lastpage(lastpage),
	textantialias(textantialias),gfxantialias(gfxantialias)
{
}


PSRipOptions::~PSRipOptions()
{
}


// Build argv array ready for execv call.  Returns a pointer to the argv array.
void PSRipOptions::RunProgram(std::string &filename)
{
	// Build a GhostScript command according to the following format:
	// %s -sDEVICE=%s -sOutputFile=%s_%%d.tif -r%dx%d %s -dBATCH -dNOPAUSE %s

	ClearArgs();

	// Select an appropriate GS device for output...

	switch(STRIP_ALPHA(type))
	{
		case IS_TYPE_BW:
			AddArg("-sDEVICE=tifflzw");
			break;
		case IS_TYPE_GREY:
			AddArg("-sDEVICE=tiffgray");
			break;
		case IS_TYPE_RGB:
			AddArg("-sDEVICE=tiff24nc");
			break;
		case IS_TYPE_CMYK:
			AddArg("-sDEVICE=tiff32nc");
			break;
		case IS_TYPE_DEVICEN:
			// FIXME: add support for DeviceN using the tiffsep device.
		default:
			throw "PSRip: type not yet supported";
			break;
	}

	// Enable interpolation
	AddArg("-dDOINTERPOLATE");

	// Create anti-aliasing parameters
	if(textantialias)
		AddArg("-dTextAlphaBits=4");

	if(gfxantialias)
		AddArg("-dGraphicsAlphaBits=4");

	// Create FirstPage and LastPage paramters, if needed...	
	if(firstpage)
	{
		std::stringstream tmp;
		tmp << "-dFirstPage=" << firstpage;
		AddArg(tmp.str());
	}

	if(lastpage)
	{
		std::stringstream tmp;
		tmp << "-dLastPage=" << firstpage;
		AddArg(tmp.str());
	}

	// FIXME - need to escape this for the shell!
	// Build output name;
	Debug[TRACE] << "Building filename from " << filename.c_str() << ", " << "_%03d" << " and " << "tif" << endl;
	char *tmp=BuildFilename(filename.c_str(),"_%03d","tif");
	char *tmp2=BuildFilename("-sOutputFile=",tmp,"");
	Debug[TRACE] << "Got " << tmp2 << " by way of " << tmp << endl;
	AddArg(tmp2);
	free(tmp2);
	free(tmp);

	// Build resolution;
	std::stringstream tmpss;
	tmpss << "-r" << resolution << "x" << resolution << endl;
	AddArg(tmpss.str());

	AddArg("-dBATCH");
	AddArg("-dNOPAUSE");

	AddArg(filename);

	ExternalGhostScript::RunProgram();
}


// Save the options into a ConfigDB.
void PSRipOptions::ToDB(ConfigDB &db)
{
}

// Retrieve the options from a ConfigDB
void PSRipOptions::FromDB(ConfigDB &db)
{
}

