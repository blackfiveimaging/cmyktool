#ifndef CONVERSIONOPTS_H
#define CONVERSIONOPTS_H

#include "imagesource/imagesource.h"
#include "profilemanager/lcmswrapper.h"


enum CMYKConversionMode
{
	CMYKCONVERSIONMODE_NORMAL,
	CMYKCONVERSIONMODE_HOLDBLACK,
	CMYKCONVERSIONMODE_HOLDGREY,
	CMYKCONVERSIONMODE_OVERPRINT,
};

class CMYKConversionOptions
{
	public:
	CMYKConversionOptions(const char *presetname=NULL);
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

	ImageSource *Apply(ImageSource *src,ImageSource *mask=NULL);

	protected:
	char *inprofile;
	char *outprofile;
	LCMSWrapper_Intent intent;
	CMYKConversionMode mode;
};

#endif
