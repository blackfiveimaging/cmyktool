#ifndef CONVERSIONOPTS_H
#define CONVERSIONOPTS_H

#include <string>
#include <cstring>

#include "support/configdb.h"
#include "support/pathsupport.h"
#include "imagesource/imagesource.h"
#include "profilemanager/profilemanager.h"
#include "support/md5.h"
#include "support/util.h"

#define CMYKCONVERSIONOPTS_PRESET_PATH "$XDG_CONFIG_HOME/cmyktool/presets"

#define PRESET_PREVIOUS_ESCAPE "previous"
#define PRESET_NEW_ESCAPE ""

enum CMYKConversionMode
{
	CMYKCONVERSIONMODE_NORMAL,
	CMYKCONVERSIONMODE_HOLDBLACK,
	CMYKCONVERSIONMODE_HOLDGREY,
	CMYKCONVERSIONMODE_OVERPRINT,
	CMYKCONVERSIONMODE_MAX
};


class CMYKConversionOptions
{
	public:
	CMYKConversionOptions(ProfileManager &pm,const char *presetname=NULL);
	CMYKConversionOptions(const CMYKConversionOptions &other);
	~CMYKConversionOptions();
	CMYKConversionOptions &operator=(const CMYKConversionOptions &other);

	LCMSWrapper_Intent GetIntent();
	CMYKConversionMode GetMode();
	const char *GetInProfile();
	const char *GetOutProfile();
	IS_TYPE GetOutputType();

	void SetIntent(LCMSWrapper_Intent intent);
	void SetMode(CMYKConversionMode mode);
	void SetInProfile(const char *in);
	void SetOutProfile(const char *out);

	void Save(const char *presetname);

	ImageSource *Apply(ImageSource *src,ImageSource *mask=NULL,CMTransformFactory *factory=NULL);

	ProfileManager &profilemanager;
	protected:
	std::string inprofile;
	std::string outprofile;
	LCMSWrapper_Intent intent;
	CMYKConversionMode mode;
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
		SetString("InProfile",opts.GetInProfile());
		SetInt("Intent",opts.GetIntent());
		SetInt("Mode",opts.GetMode());
	}
	void Retrieve(CMYKConversionOptions &opts)
	{
		opts.SetOutProfile(FindString("OutProfile"));	
		opts.SetInProfile(FindString("InProfile"));	
		opts.SetIntent(LCMSWrapper_Intent(FindInt("Intent")));	
		opts.SetMode(CMYKConversionMode(FindInt("Mode")));	
	}
	protected:
	static ConfigTemplate Template[];
};

#endif
