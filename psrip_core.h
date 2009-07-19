#ifndef PSRIP_CORE_H
#define PSRIP_CORE_H

#include "imagesource/imagesource.h"
#include "support/tempfile.h"
#include "support/configdb.h"
#include "support/searchpath.h"
#include "support/progress.h"
#include "support/threadevent.h"
#include "support/threadutil.h"


#define PSRIPOPTIONS_ARGC_MAX 10
class PSRipOptions : public SearchPathHandler
{
	public:
	PSRipOptions(IS_TYPE type=IS_TYPE_RGB,int resolution=300,int firstpage=0,int lastpage=0,bool textantialias=false,bool gfxantialias=true);
	~PSRipOptions();
	char * const *GetArgV(const char *filename,int &argc);
	void ToDB(ConfigDB &db);
	void FromDB(ConfigDB &db);
	IS_TYPE type;
	int resolution;
	int firstpage;
	int lastpage;
	bool textantialias;
	bool gfxantialias;
	protected:
	int argc;
	char *argv[PSRIPOPTIONS_ARGC_MAX+1];
};

class PSRip_TempFile;
class Thread_PSRipFileMonitor;
class Thread_PSRipProcess;
class PSRip : public TempFileTracker, public ThreadEventHandler
{
	public:
	PSRip();
	~PSRip();
	void Rip(const char *filename,PSRipOptions &options);
	void Stop();
	bool TestFinished();
	char *GetRippedFilename(int page);
	ThreadEvent Event;
	protected:
	char *tempname;
	Thread_PSRipProcess *ripthread;
	Thread_PSRipFileMonitor *monitorthread;
	friend class Thread_PSRipFileMonitor;
	friend class Thread_PSRipProcess;
};

#endif

