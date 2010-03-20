#include <iostream>
#include <cstdlib>

#include "support/debug.h"
#include "support/util.h"
#include "support/pathsupport.h"
#include "support/configdb.h"
#include "conversionopts.h"
#include "imagesource_promote.h"
#include "imagesource_cms.h"
#include "imagesource_deflatten.h"
#include "imagesource_mask.h"
#include "imagesource_montage.h"
#include "imagesource_flatten.h"
#include "naivecmyktransform.h"

using namespace std;



ConfigTemplate CMYKConversionPreset::Template[]=
{
	ConfigTemplate("PresetID",PRESET_PREVIOUS_ESCAPE),
	ConfigTemplate("DisplayName","unnamed preset"),
	ConfigTemplate("InRGBProfile",BUILTINSRGB_ESCAPESTRING),
	ConfigTemplate("InCMYKProfile","USWebCoatedSWOP.icc"),
	ConfigTemplate("OutProfile","USWebCoatedSWOP.icc"),
	ConfigTemplate("IgnoreEmbedded",0),
	ConfigTemplate("Intent",0),
	ConfigTemplate("Mode",1),
	ConfigTemplate("Width",10),
	ConfigTemplate()
};


CMYKConversionOptions::CMYKConversionOptions(ProfileManager &pm)
	: profilemanager(pm), inrgbprofile("sRGB Color Space Profile.icm"), incmykprofile("USWebCoatedSWOP.icc"),
	outprofile("USWebCoatedSWOP.icc"), intent(LCMSWRAPPER_INTENT_DEFAULT), mode(CMYKCONVERSIONMODE_NORMAL), ignoreembedded(false), width(0)
{
}


CMYKConversionOptions::CMYKConversionOptions(const CMYKConversionOptions &other)
	: profilemanager(other.profilemanager), inrgbprofile(other.inrgbprofile), incmykprofile(other.incmykprofile),
	outprofile(other.outprofile), intent(other.intent), mode(other.mode), ignoreembedded(false)
{
}


CMYKConversionOptions::~CMYKConversionOptions()
{
}


CMYKConversionOptions &CMYKConversionOptions::operator=(const CMYKConversionOptions &other)
{
	mode=other.mode;
	intent=other.intent;
	inrgbprofile=other.inrgbprofile;
	incmykprofile=other.incmykprofile;
	outprofile=other.outprofile;
	ignoreembedded=other.ignoreembedded;
	return(*this);
}


LCMSWrapper_Intent CMYKConversionOptions::GetIntent()
{
	return(intent);
}


CMYKConversionMode CMYKConversionOptions::GetMode()
{
	return(mode);
}


bool CMYKConversionOptions::GetIgnoreEmbedded()
{
	return(ignoreembedded);
}


const char *CMYKConversionOptions::GetInRGBProfile()
{
	return(inrgbprofile.c_str());
}


const char *CMYKConversionOptions::GetInCMYKProfile()
{
	return(incmykprofile.c_str());
}


const char *CMYKConversionOptions::GetOutProfile()
{
	return(outprofile.c_str());
}


int CMYKConversionOptions::GetWidth()
{
	return(width);
}


void CMYKConversionOptions::SetIntent(LCMSWrapper_Intent intent)
{
	this->intent=intent;
}


void CMYKConversionOptions::SetMode(CMYKConversionMode mode)
{
	this->mode=mode;
}


void CMYKConversionOptions::SetIgnoreEmbedded(bool ignore)
{
	ignoreembedded=ignore;
}


void CMYKConversionOptions::SetInRGBProfile(const char *in)
{
	if(in)
		inrgbprofile=string(in);
}


void CMYKConversionOptions::SetInCMYKProfile(const char *in)
{
	if(in)
		incmykprofile=string(in);
}


void CMYKConversionOptions::SetOutProfile(const char *out)
{
	if(out)
		outprofile=string(out);
}


void CMYKConversionOptions::SetWidth(int w)
{
	width=w;
}


void CMYKConversionOptions::Save(const char *presetname)
{
}


IS_TYPE CMYKConversionOptions::GetOutputType(IS_TYPE type)
{
	if(GetMode()==CMYKCONVERSIONMODE_NONE)
		return(type);

	// If the user's chosen no output profile we want to use a naive RGB -> CMYK conversion...
	if(outprofile==NOPROFILE_ESCAPESTRING)
		return(IS_TYPE_CMYK);

	CMSProfile *p=profilemanager.GetProfile(outprofile.c_str());
	if(p)
	{
		if(p->IsDeviceLink())
			type=p->GetDeviceLinkOutputSpace();
		else
			type=p->GetColourSpace();
		delete p;
	}
	return(type);
}


ImageSource *CMYKConversionOptions::Apply(ImageSource *src,ImageSource *mask,CMTransformFactory *factory)
{
	if(mode==CMYKCONVERSIONMODE_NONE)
		return(src);

	if(STRIP_ALPHA(src->type)==IS_TYPE_GREY)
		src=new ImageSource_Promote(src,IS_TYPE_RGB);

	bool freeinprof=false;
	CMSProfile *inprof=NULL;
	if(!ignoreembedded)
		inprof=src->GetEmbeddedProfile();
	if(!inprof)
	{
		freeinprof=true;
		switch(STRIP_ALPHA(src->type))
		{
			case IS_TYPE_RGB:
				inprof=profilemanager.GetProfile(inrgbprofile.c_str());
				break;
			case IS_TYPE_CMYK:
				inprof=profilemanager.GetProfile(incmykprofile.c_str());
				break;
			default:
				throw "Image type not (yet) supported";
				break;
		}
		if(!inprof)
			throw "Can't open input profile";
	}

	Debug[TRACE] << "Opening profile: " << outprofile << endl;
	CMSProfile *outprof=NULL;
	if(outprofile!=NOPROFILE_ESCAPESTRING)
		outprof=profilemanager.GetProfile(outprofile.c_str());
	if(outprof)
	{
		if(outprof->IsDeviceLink())
		{
			Debug[TRACE] << "Using DeviceLink profile" << endl;
			if(factory)
			{
				Debug[TRACE] << "Getting transform from factory" << endl;
				CMSTransform *trans=factory->GetTransform(outprof,NULL,intent);
				if(trans)
					src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,
						mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);
			}
			else
				src=new ImageSource_Deflatten(src,outprof,NULL,mode==CMYKCONVERSIONMODE_HOLDBLACK,
					mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);

			CMSProfile *embprof=NULL;

			switch(STRIP_ALPHA(src->type))
			{
				case IS_TYPE_RGB:
					embprof=profilemanager.GetProfile(inrgbprofile.c_str());
					Debug[TRACE] << "Using default RGB profile to embed, since output profile is a DeviceLink" << endl;
					break;
				case IS_TYPE_CMYK:
					embprof=profilemanager.GetProfile(incmykprofile.c_str());
					Debug[TRACE] << "Using default CMYK profile to embed, since output profile is a DeviceLink" << endl;
					break;
				default:
					throw "Image type not (yet) supported";
					break;
			}
			src->SetEmbeddedProfile(embprof,true);
			delete outprof;
		}
		else
		{
			if(factory)
			{
				CMSTransform *trans=factory->GetTransform(outprof,inprof,intent);
				if(trans)
					src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,
						mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);
			}
			else
				src=new ImageSource_Deflatten(src,inprof,outprof,mode==CMYKCONVERSIONMODE_HOLDBLACK,
					mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);
			src->SetEmbeddedProfile(outprof,true);
		}
	}
	else
	{
		// No output profile - use a naive conversion instead.
		CMSTransform *trans=new NaiveCMYKTransform(src);
		src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,
			mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);
	}
	if(freeinprof)
		delete inprof;

	return(src);
}

