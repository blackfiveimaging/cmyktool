#ifndef CMYKTOOL_CORE_H
#define CMYKTOOL_CORE_H

#include <string>

#include "support/jobqueue.h"
#include "support/configdb.h"

#include "profilemanager/profilemanager.h"
#include "cmtransformworker.h"
#include "threadevent.h"
#include "conversionopts.h"


class CMYKTool_Core : public ConfigFile, public ConfigDB, public ThreadEventHandler
{
	public:
	CMYKTool_Core() : ConfigFile(), ConfigDB(Template), profilemanager(this,"[ColourManagement]"),
		dispatcher(0),factory(profilemanager), convopts(profilemanager), UpdateUI(*this,"UpdateUI")
	{
		new ConfigDBHandler(this,"[CMYKTool]",this);
		profilemanager.SetInt("DefaultCMYKProfileActive",1);

		char *fn=substitute_xdgconfighome("$XDG_CONFIG_HOME/cmyktool/cmyktool.conf");
		ParseConfigFile(fn);
		confname=fn;
		free(fn);

		dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
		dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
		dispatcher.AddWorker(new CMTransformWorker(dispatcher,profilemanager));
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
	JobDispatcher &GetDispatcher()
	{
		return(dispatcher);
	}
	CMYKConversionOptions &GetOptions()
	{
		return(convopts);
	};
	protected:
	ProfileManager profilemanager;
	JobDispatcher dispatcher;
	CMTransformFactory factory;
	CMYKConversionOptions convopts;
	std::string confname;
	ThreadEvent UpdateUI;
	static ConfigTemplate Template[];
};

#endif

