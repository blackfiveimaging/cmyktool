#ifndef CONVERSIONOPTS_H
#define CONVERSIONOPTS_H

#include <string>

#include "support/configdb.h"
#include "support/pathsupport.h"
#include "imagesource/imagesource.h"
#include "profilemanager/profilemanager.h"

#define CMYKCONVERSIONOPTS_PRESET_PATH "$XDG_CONFIG_HOME/cmyktool"

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
		presetname=filename;
		ParseConfigFile(presetname.c_str());
	}
	void Save(const char *filename=NULL)
	{
		if(filename)
		{
			char *path=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);
			presetname=path + std::string("/") + filename;
		}
		SaveConfigFile(presetname.c_str());
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
	std::string presetname;
};

#endif
