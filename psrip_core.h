#ifndef PSRIP_CORE_H
#define PSRIP_CORE_H

#include "imagesource.h"
#include "tempfile.h"
#include "configdb.h"
#include "searchpath.h"
#include "progress.h"
#include "threadevent.h"
#include "threadutil.h"
#include "externalghostscript.h"

class PSRipOptions : public ExternalGhostScript
{
	public:
	PSRipOptions(IS_TYPE type=IS_TYPE_RGB,int resolution=300,int firstpage=0,int lastpage=0,bool textantialias=false,bool gfxantialias=true);
	virtual ~PSRipOptions();
	virtual void RunProgram(std::string &filename);
	void ToDB(ConfigDB &db);
	void FromDB(ConfigDB &db);
	IS_TYPE type;
	int resolution;
	int firstpage;
	int lastpage;
	bool textantialias;
	bool gfxantialias;
	protected:
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
	void WaitFinished();
	bool TestPage(int page);
	void WaitPage(int page);
	char *GetRippedFilename(int page,const char *basename=NULL);
	ThreadEvent Event;
	protected:
	char *tempname;
	Thread_PSRipProcess *ripthread;
	Thread_PSRipFileMonitor *monitorthread;
	friend class Thread_PSRipFileMonitor;
	friend class Thread_PSRipProcess;
};

#endif

