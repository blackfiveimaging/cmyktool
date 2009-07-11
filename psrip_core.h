#ifndef PSRIP_CORE_H
#define PSRIP_CORE_H

#include "imagesource/imagesource.h"
#include "support/tempfile.h"
#include "support/searchpath.h"
#include "support/progress.h"
#include "support/threadevent.h"
#include "support/threadutil.h"


class PSRip_TempFile;
class Thread_PSRipFileMonitor;
class Thread_PSRipProcess;
class PSRip : public TempFileTracker, public ThreadEventHandler
{
	public:
	PSRip(SearchPathHandler &searchpath);
	~PSRip();
	void Rip(const char *filename,IS_TYPE type=IS_TYPE_RGB,int resolution=300,int firstpage=0,int lastpage=0);
	bool TestFinished();
	char *GetRippedFilename(int page);
	ThreadEvent Event;
	protected:
	SearchPathHandler &searchpath;
	char *tempname;
	Thread_PSRipProcess *ripthread;
	Thread_PSRipFileMonitor *monitorthread;
	friend class Thread_PSRipFileMonitor;
	friend class Thread_PSRipProcess;
};

#endif

