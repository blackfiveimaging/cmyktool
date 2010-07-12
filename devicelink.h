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

class DeviceLinkList_Entry;

class DeviceLink : public ConfigFile, public ConfigDB
{
	public:
	DeviceLink(const char *filename=NULL);
	~DeviceLink();
	void Save(const char *filename=NULL);
	void Save(DeviceLinkList_Entry &entry);	// Take filenames from an existing DeviceLink entry.
	void Delete();	// Removes the files from disk, but doesn't delete the object itself.
	void CreateDeviceLink(std::string agyllpath,ProfileManager &pm);
	void SetBlackGen(Argyll_BlackGenerationCurve blackgen);
	Argyll_BlackGenerationCurve &GetBlackGen();
	CMSTransform *GetTransform();
	private:
	Argyll_BlackGenerationCurve blackgen;
	void makefilename();
	std::string fn;
	std::string metadata_fn;
	static ConfigTemplate configtemplate[];
};


class DeviceLinkList_Entry
{
	public:
	DeviceLinkList_Entry(const char *fn,const char *dn,int pending) : filename(fn), displayname(dn), pending(pending)
	{
	}
	std::string filename;
	std::string displayname;
	bool pending;
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
			if(strcasecmp(".icc",fn+strlen(fn)-4)==0 || strcasecmp(".icm",fn+strlen(fn)-4)==0 || strcasecmp(".dlm",fn+strlen(fn)-4)==0)
			{
				Debug[TRACE] << "Adding file " << fn << std::endl;
				DeviceLink dl(fn);

				bool match=true;

				// FIXME: Yuk - find a better way to do this...
				if(strcasecmp(".dlm",fn+strlen(fn)-4)==0)
				{
					if(dl.FindInt("Pending")==0)
						match=false;
				}

				if(match && src && dst)
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

					if(!Find(dn))	// Only create a new entry if there isn't already one by this name...
					{
						DeviceLinkList_Entry e(fn,dn,dl.FindInt("Pending"));
						push_back(e);
					}
				}
			}
		}
		free(configdir);
	}
	DeviceLinkList_Entry *Find(std::string displayname)
	{
		for(unsigned int idx=0;idx<size();++idx)
		{
			if(displayname==(*this)[idx].displayname)
				return(&(*this)[idx]);
		}
		return(NULL);
	}
};


#endif

