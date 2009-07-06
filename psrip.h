#ifndef PSRIP_H
#define PSRIP_H

#include <iostream>
#include <cstdio>
#include <cstring>

#include "imagesource/imagesource.h"
#include "support/tempfile.h"
#include "support/searchpath.h"
#include "support/util.h"
#include "support/thread.h"
#include "support/threadutil.h"
#include "support/progress.h"

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


class PSRip : public TempFileTracker
{
	public:
	PSRip(SearchPathHandler &searchpath,const char *filename,Progress *prog=NULL,IS_TYPE type=IS_TYPE_RGB,int resolution=600,int firstpage=0,int lastpage=0)
		: TempFileTracker(), searchpath(searchpath), tempname()
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
		tempname=tempnam(NULL,"PSRIP");

		// Now build the actual command.
		const char *gsfmtstring="%s -sDEVICE=%s -sOutputFile=%s_%%d.tif -r%dx%d %s -dBATCH -dNOPAUSE %s";
		int cmdlen=strlen(gsfmtstring)+10+strlen(gspath)+strlen(gsdev)+strlen(pages)+strlen(filename);
		char *gscmd=(char *)malloc(cmdlen);
		snprintf(gscmd,cmdlen,gsfmtstring,gspath,gsdev,tempname,resolution,resolution,pages,filename);
		free(pages);
		free(gspath);

		cerr << "Built command: " << gscmd << endl;

		// Execute the command...

		Thread_SystemCommand thread(gscmd);
		while(!thread.TestFinished())
		{
			if(prog)
				prog->DoProgress(0,0);
#ifdef WIN32
			sleep(100);
#else
			usleep(100000);
#endif
		}

		// Now scan the output files and add them to the TempFileTracker

		int page=1;
		char *rfn=NULL;
		while((rfn=GetRippedFilename(page)))
		{
			new PSRip_TempFile(this,rfn);
			++page;
			free(rfn);
		}

		free(gscmd);
	}
	~PSRip()
	{
		if(tempname)
			free(tempname);
	}
	char *GetRippedFilename(int page)
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
	protected:
	SearchPathHandler &searchpath;
	char *tempname;
};


#endif

