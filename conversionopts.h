#ifndef CONVERSIONOPTS_H
#define CONVERSIONOPTS_H

#include <string>
#include <cstdlib>
#include <cstring>
#include <deque>

#include "debug.h"
#include "support/configdb.h"
#include "support/pathsupport.h"
#include "imagesource/imagesource.h"
#include "profilemanager/profilemanager.h"
#include "support/md5.h"
#include "support/util.h"
#include "support/dirtreewalker.h"

#include "config.h"
#include "gettext.h"

#define _(x) gettext(x)

#define CMYKCONVERSIONOPTS_PRESET_PATH "$XDG_CONFIG_HOME/cmyktool/presets"

#define PRESET_NONE_ESCAPE "none"
#define PRESET_PREVIOUS_ESCAPE "previous"
#define PRESET_NEW_ESCAPE ""

enum CMYKConversionMode
{
	CMYKCONVERSIONMODE_NORMAL,
	CMYKCONVERSIONMODE_HOLDBLACK,
	CMYKCONVERSIONMODE_HOLDGREY,
	CMYKCONVERSIONMODE_OVERPRINT,
	CMYKCONVERSIONMODE_NONE,
	CMYKCONVERSIONMODE_MAX
};


class CMYKConversionOptions
{
	public:
	CMYKConversionOptions(ProfileManager &pm);
	CMYKConversionOptions(const CMYKConversionOptions &other);
	~CMYKConversionOptions();
	CMYKConversionOptions &operator=(const CMYKConversionOptions &other);

	LCMSWrapper_Intent GetIntent();
	CMYKConversionMode GetMode();
	const char *GetInRGBProfile();
	const char *GetInCMYKProfile();
	const char *GetOutProfile();
	IS_TYPE GetOutputType(IS_TYPE deflt=IS_TYPE_RGB);
	bool GetIgnoreEmbedded();
	int GetWidth();

	void SetIntent(LCMSWrapper_Intent intent);
	void SetMode(CMYKConversionMode mode);
	void SetInRGBProfile(const char *in);
	void SetInCMYKProfile(const char *in);
	void SetOutProfile(const char *out);
	void SetIgnoreEmbedded(bool ignore);
	void SetWidth(int w);

	void Save(const char *presetname);

	ImageSource *Apply(ImageSource *src,ImageSource *mask=NULL,CMTransformFactory *factory=NULL);

	ProfileManager &profilemanager;
	protected:
	std::string inrgbprofile;
	std::string incmykprofile;
	std::string outprofile;
	LCMSWrapper_Intent intent;
	CMYKConversionMode mode;
	bool ignoreembedded;
	int width;
};


class CMYKConversionPreset : public ConfigFile, public ConfigDB
{
	public:
	CMYKConversionPreset() : ConfigFile(), ConfigDB(Template)
	{
		new ConfigDBHandler(this,"[CMYKConversionOpts]",this);
	}
	void Load(const char *filename)
	{
		if(strcmp(filename,PRESET_NONE_ESCAPE)==0)
			SetInt("Mode",CMYKCONVERSIONMODE_NONE);
		else
			ParseConfigFile(filename);
	}
	void Save(const char *presetid=NULL)
	{
		// If a presetid is provided we store it, otherwise retrieve the existing one
		if(presetid)
			SetString("PresetID",presetid);
		else
			presetid=FindString("PresetID");

		char *path=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);

		// If the presetid is an empty string (as opposed to a NULL pointer) we create a new ID
		if(presetid && strlen(presetid)<1)
		{
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
				presetname=path + std::string("/") + dig.GetPrintableDigest();
			} while(CheckFileExists(presetname.c_str()));
			SetString("PresetID",dig.GetPrintableDigest());
			presetid=FindString("PresetID");
		}
		if(presetid)
		{
			std::string presetname=path + std::string("/") + presetid;
			SaveConfigFile(presetname.c_str());
		}

		free(path);
	}
	void Store(CMYKConversionOptions &opts)
	{
		SetString("OutProfile",opts.GetOutProfile());
		SetString("InRGBProfile",opts.GetInRGBProfile());
		SetString("InCMYKProfile",opts.GetInCMYKProfile());
		SetInt("Intent",opts.GetIntent());
		SetInt("Mode",opts.GetMode());
		SetInt("Width",opts.GetWidth());
		SetInt("IgnoreEmbedded",opts.GetIgnoreEmbedded());
	}
	void Retrieve(CMYKConversionOptions &opts)
	{
		opts.SetOutProfile(FindString("OutProfile"));	
		opts.SetInRGBProfile(FindString("InRGBProfile"));	
		opts.SetInCMYKProfile(FindString("InCMYKProfile"));	
		opts.SetIntent(LCMSWrapper_Intent(FindInt("Intent")));	
		opts.SetMode(CMYKConversionMode(FindInt("Mode")));	
		opts.SetWidth(FindInt("Width"));
		opts.SetIgnoreEmbedded(FindInt("IgnoreEmbedded"));
	}
	protected:
	static ConfigTemplate Template[];
};


class PresetList_Entry
{
	public:
	PresetList_Entry(const char *fn,const char *dn) : filename(fn), displayname(dn)
	{
	}
	std::string filename;
	std::string displayname;
};


class PresetList : public std::deque<PresetList_Entry>
{
	public:
	PresetList() : previdx(-1)
	{
		Debug[TRACE] << "Creating preset list..." << std::endl;
		char *configdir=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);
		DirTreeWalker dtw(configdir);
		const char *fn;

		while((fn=dtw.NextFile()))
		{
			Debug[TRACE] << "Adding file " << fn << std::endl;
			CMYKConversionPreset p;
			p.Load(fn);
			const char *dn=p.FindString("DisplayName");
			if(!(dn && strlen(dn)>0))
				dn=_("<unknown>");

			const char *id=p.FindString("PresetID");

			if(strcmp(id,PRESET_PREVIOUS_ESCAPE)==0)
				previdx=size();

			PresetList_Entry e(fn,dn);
			push_back(e);
		}
		free(configdir);
		Debug[TRACE] << "Done - index of previous file is " << previdx << std::endl;
	}
	int GetPreviousIndex()
	{
		return(previdx);
	}
	int previdx;
};

#endif
