#include <iostream>
#include <cstdio>

#include "imagesource/imagesource.h"
#include "support/tempfile.h"
#include "support/searchpath.h"
#include "support/util.h"
#include "support/progresstext.h"
#include "support/thread.h"

using namespace std;


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


class PSRip_Thread : public ThreadFunction, public Thread
{
	public:
	PSRip_Thread(const char *cmd) : ThreadFunction(), Thread(this), cmd(cmd)
	{
		Start();
		WaitSync();
	}
	virtual ~PSRip_Thread()
	{
	}
	virtual int Entry(Thread &t)
	{
		SendSync();
		try
		{
			if(system(cmd))
				throw "Command failed";
		}
		catch(const char *err)
		{
			cerr << "Subthread error: " << err << endl;
			returncode=-1;
		}
		return(returncode);
	}
	protected:
	const char *cmd;
};


class PSRip : public TempFileTracker
{
	public:
	PSRip(SearchPathHandler &searchpath,const char *filename,IS_TYPE type,int resolution=600,int firstpage=0,int lastpage=0)
		: TempFileTracker(), searchpath(searchpath)
	{

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
		char *tempname=tempnam(NULL,"PSRIP");

		// Now build the actual command.
		const char *gsfmtstring="%s -sDEVICE=%s -sOutputFile=%s_%%d.tif -r%dx%d %s -dBATCH -dNOPAUSE %s";
		int cmdlen=strlen(gsfmtstring)+10+strlen(gspath)+strlen(gsdev)+strlen(pages)+strlen(filename);
		char *gscmd=(char *)malloc(cmdlen);
		snprintf(gscmd,cmdlen,gsfmtstring,gspath,gsdev,tempname,resolution,resolution,pages,filename);
		free(pages);
		free(gspath);

		cerr << "Built command: " << gscmd << endl;

		// Execute the command...

		PSRip_Thread thread(gscmd);
		ProgressText prog;
		while(!thread.TestFinished())
		{
			prog.DoProgress(0,0);
#ifdef WIN32
			sleep(100);
#else
			usleep(100000);
#endif
		}

		// Now scan the output files and add them to the TempFileTracker

		int page=1;
		char *buf=(char *)malloc(strlen(tempname)+10);
		while(page)
		{
			snprintf(buf,strlen(tempname)+10,"%s_%d.tif",tempname,page);
			cerr << "Checking whether " << buf << " exists..." << endl;
			if(CheckFileExists(buf))
			{
				cerr << "Yes - adding..." << endl;
				new PSRip_TempFile(this,buf);
				++page;
			}
			else
				page=0;			
		}

		free(gscmd);
		free(tempname);
	}
	~PSRip()
	{
	}
	protected:
	SearchPathHandler &searchpath;
};


int main(int argc,char *argv[])
{
	if(argc>1)
	{
		try
		{
			SearchPathHandler searchpath;
#ifdef WIN32
			searchpath.AddPath("c:/gs/bin/;c:/Program Files/gs/bin");
#else
			searchpath.AddPath("/usr/bin");
#endif

			cerr << "Processing file " << argv[1] << endl;
			PSRip rip(searchpath,argv[1],IS_TYPE_CMYK);


		}
		catch(const char *err)
		{
			cerr << "Error: " << err << endl;
		}
	}
	return(0);
}

