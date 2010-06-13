#ifndef DEVICELINK_H

#include <string>
#include <deque>
#include <cstring>

#include "configdb.h"
#include "dirtreewalker.h"
#include "pathsupport.h"
#include "argyllsupport/argyllbg.h"
#include "profilemanager/profilemanager.h"
#include "profilemanager/lcmswrapper.h"

#include "config.h"
#include "gettext.h"
#define _(x) gettext(x)

// Devicelink caching:
// Store devicelinks and metadata in $XDG_CONFIG_HOME/cmyktool/devicelinks
#define DEVICELINK_CACHE_PATH "$XDG_CONFIG_HOME/cmyktool/devicelinks"


class DeviceLink : public ConfigFile, public ConfigDB
{
	public:
	DeviceLink(const char *filename=NULL);
	~DeviceLink();
	void Save(const char *filename=NULL);
	Argyll_BlackGenerationCurve blackgen;
	void CreateDeviceLink(ProfileManager &pm);
	CMSTransform *GetTransform();
	private:
	std::string fn;
	std::string metadata_fn;
	static ConfigTemplate configtemplate[];
};


class DeviceLinkList_Entry
{
	public:
	DeviceLinkList_Entry(const char *fn,const char *dn) : filename(fn), displayname(dn)
	{
	}
	std::string filename;
	std::string displayname;
};


class DeviceLinkList : public std::deque<DeviceLinkList_Entry>
{
	public:
	DeviceLinkList()
	{
		Debug[TRACE] << "Creating devicelink list..." << std::endl;
		char *configdir=substitute_xdgconfighome(DEVICELINK_CACHE_PATH);
		DirTreeWalker dtw(configdir);
		const char *fn;

		while((fn=dtw.NextFile()))
		{
			if(strcmp(".dlm",fn+strlen(fn)-4)==0)
			{
				Debug[TRACE] << "Adding file " << fn << std::endl;
				DeviceLink dl(fn);

				const char *dn=dl.FindString("Description");
				if(!(dn && strlen(dn)>0))
					dn=_("<unknown>");

				DeviceLinkList_Entry e(fn,dn);
				push_back(e);
			}
		}
		free(configdir);
	}
};


#endif

