#include <iostream>

#include "debug.h"
#include "util.h"
#include "md5.h"
#include "devicelink.h"

ConfigTemplate DeviceLink::configtemplate[]=
{
	ConfigTemplate("Description",""),
	ConfigTemplate("SourceProfile",""),
	ConfigTemplate("DestProfile",""),
	ConfigTemplate("RenderingIntent",int(0)),
	ConfigTemplate("SourceViewingConditions",""),
	ConfigTemplate("DestViewingConditions",""),
	ConfigTemplate("BlackGeneration",""),
	ConfigTemplate()
};


DeviceLink::DeviceLink(const char *filename) : ConfigFile(), ConfigDB(configtemplate)
{
	new ConfigDBHandler(this,"[DeviceLink]",this);

	if(filename)
	{
		fn=filename;
		char *fn2=BuildFilename(filename,NULL,"dlm");
		metadata_fn=fn2;
		ParseConfigFile(fn2);
		free(fn2);
	}
	else
	{
		char *path=substitute_xdgconfighome(DEVICELINK_CACHE_PATH);
		CreateDirIfNeeded(path);
		// Generate new presetid here
		// Yeah, using an MD5 digest of a random buffer is over-engineering, but it should work...
		std::string presetname;
		char buf[32];
		MD5Digest dig;
		do
		{
			for(int i=0;i<32;++i)
			{
				buf[i]=char(RandomSeeded(255));
				dig.Update(buf,32);
			}
			fn=path + std::string("/") + dig.GetPrintableDigest() + ".icc";
		} while(CheckFileExists(fn.c_str()));

		char *fn2=BuildFilename(fn.c_str(),NULL,"dlm");
		metadata_fn=fn2;
		free(fn2);

		free(path);
	}
}


DeviceLink::~DeviceLink()
{

}


void DeviceLink::Save(const char *filename)
{
	if(!filename)
		filename=metadata_fn.c_str();
	SaveConfigFile(filename);
}


void DeviceLink::CreateDeviceLink(ProfileManager &pm)
{

}
