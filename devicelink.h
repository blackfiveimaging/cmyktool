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

enum DeviceLink_Quality {DEVICELINK_QUALITY_LOW=0,DEVICELINK_QUALITY_MEDIUM,DEVICELINK_QUALITY_HIGH};


class DeviceLink : public ConfigFile, public ConfigDB
{
	public:
	DeviceLink(const char *filename=NULL);
	~DeviceLink();
	void Save(const char *filename=NULL);
	void Delete();	// Removes the files from disk, but doesn't delete the object itself.
	Argyll_BlackGenerationCurve blackgen;
	void CreateDeviceLink(ProfileManager &pm);
	CMSTransform *GetTransform();
	private:
	void makefilename();
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
	DeviceLinkList(CMSProfile *src=NULL,CMSProfile *dst=NULL,LCMSWrapper_Intent intent=LCMSWRAPPER_INTENT_NONE)
	{
		if(intent==LCMSWRAPPER_INTENT_DEFAULT)
			intent=LCMSWRAPPER_INTENT_PERCEPTUAL;

		Debug[TRACE] << "Creating devicelink list..." << std::endl;
		char *configdir=substitute_xdgconfighome(DEVICELINK_CACHE_PATH);
		DirTreeWalker dtw(configdir);
		const char *fn;

		const char *srcd="";
		const char *dstd="";
		if(src)
			srcd=src->GetMD5()->GetPrintableDigest();
		if(dst)
			dstd=dst->GetMD5()->GetPrintableDigest();

		while((fn=dtw.NextFile()))
		{
			if(strcmp(".icc",fn+strlen(fn)-4)==0 || strcmp(".icm",fn+strlen(fn)-4)==0)
			{
				Debug[TRACE] << "Adding file " << fn << std::endl;
				DeviceLink dl(fn);

				bool match=true;
				if(src && dst)
				{
					Debug[TRACE] << "Comparing " << srcd << " against " << dl.FindString("SourceProfileHash") << std::endl;
					if(strcmp(srcd,dl.FindString("SourceProfileHash"))!=0)
						match=false;
					Debug[TRACE] << "Match: " << match << " - Comparing " << dstd << " against " << dl.FindString("DestProfileHash") << std::endl;
					if(strcmp(dstd,dl.FindString("DestProfileHash"))!=0)
						match=false;
					Debug[TRACE] << "Match: " << match << " - Comparing " << intent << " against " << dl.FindInt("RenderingIntent") << std::endl;

					int dlintent=dl.FindInt("RenderingIntent");
					if(dlintent==LCMSWRAPPER_INTENT_DEFAULT)
						dlintent=LCMSWRAPPER_INTENT_PERCEPTUAL;

					if(dlintent!=intent && dlintent!=LCMSWRAPPER_INTENT_NONE)
						match=false;
					Debug[TRACE] << "Match: " << match << std::endl;
				}
				if(match)
				{
					const char *dn=dl.FindString("Description");
					if(!(dn && strlen(dn)>0))
						dn=_("<unknown>");

					DeviceLinkList_Entry e(fn,dn);
					push_back(e);
				}
			}
		}
		free(configdir);
	}
};


#endif

