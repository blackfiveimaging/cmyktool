#ifndef CMYKTOOL_CORE_H
#define CMYKTOOL_CORE_H

#include <string>

#include "support/jobqueue.h"
#include "support/configdb.h"

#include "profilemanager/profilemanager.h"
#include "cmtransformworker.h"
#include "threadevent.h"
#include "conversionopts.h"


class Callback
{
	public:
	Callback()
	{
	}
	virtual ~Callback()
	{
	}
	virtual void Call()
	{
	}
};


class CMYKToolDispatcher : public JobDispatcher
{
	public:
	CMYKToolDispatcher(ProfileManager &pm,int threads=3) : JobDispatcher(0), addjobcallback(0)
	{
		for(int i=0;i<threads;++i)
			AddWorker(new CMTransformWorker(*this,pm));
	}
	void SetAddJobCallback(Callback *cb)
	{
//		Debug[TRACE] << "***Setting callback..." << std::endl;
		addjobcallback=cb;
	}
	virtual void AddJob(Job *job)
	{
//		Debug[TRACE] << "*** In CMYKToolDispatcher::AddJob" << std::endl;
		JobQueue::AddJob(job);
		if(addjobcallback)
		{
//			Debug[TRACE] << "***Calling callback..." << std::endl;
			addjobcallback->Call();
		}
	}
	protected:
	Callback *addjobcallback;
};


class CMYKTool_Core : public ConfigFile, public ConfigDB, public ThreadEventHandler
{
	public:
	CMYKTool_Core() : ConfigFile(), ConfigDB(Template), profilemanager(this,"[ColourManagement]"),
		dispatcher(profilemanager),factory(profilemanager), convopts(profilemanager), UpdateUI(*this,"UpdateUI")
	{
		new ConfigDBHandler(this,"[CMYKTool]",this);
		profilemanager.SetInt("DefaultCMYKProfileActive",1);

		char *fn=substitute_xdgconfighome("$XDG_CONFIG_HOME/cmyktool/cmyktool.conf");
		ParseConfigFile(fn);
		confname=fn;
		free(fn);
	}
	virtual ~CMYKTool_Core()
	{
	}
	virtual void ProcessImage(const char *filename)
	{
		std::cerr << "*** Processing image" << std::endl;
		throw "CMYKTool_Core::ProcessImage Not yet implemented";
	}
	virtual void SaveConfig()
	{
		SaveConfigFile(confname.c_str());
	}
	ProfileManager &GetProfileManager()
	{
		return(profilemanager);
	}
	CMYKToolDispatcher &GetDispatcher()
	{
		return(dispatcher);
	}
	CMYKConversionOptions &GetOptions()
	{
		return(convopts);
	};
	protected:
	ProfileManager profilemanager;
	CMYKToolDispatcher dispatcher;
	CMTransformFactory factory;
	CMYKConversionOptions convopts;
	std::string confname;
	ThreadEvent UpdateUI;
	static ConfigTemplate Template[];
};

#endif

