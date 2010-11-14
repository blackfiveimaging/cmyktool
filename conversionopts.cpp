#include <iostream>
#include <cstdlib>
#include <cstring>

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
//	ConfigTemplate("PresetID",PRESET_PREVIOUS_ESCAPE),
	ConfigTemplate("DisplayName","unnamed preset"),
	ConfigTemplate("InRGBProfile",BUILTINSRGB_ESCAPESTRING),
	ConfigTemplate("InCMYKProfile","USWebCoatedSWOP.icc"),
	ConfigTemplate("OutProfile","USWebCoatedSWOP.icc"),
	ConfigTemplate("IgnoreEmbedded",int(0)),
	ConfigTemplate("UseDevicelink",int(0)),
	ConfigTemplate("Devicelink",""),
	ConfigTemplate("Intent",int(0)),
	ConfigTemplate("Mode",int(1)),
	ConfigTemplate("Width",int(10)),
	ConfigTemplate()
};


CMYKConversionOptions::CMYKConversionOptions(ProfileManager &pm)
	: profilemanager(pm), inrgbprofile("sRGB Color Space Profile.icm"), incmykprofile("USWebCoatedSWOP.icc"),
	outprofile("USWebCoatedSWOP.icc"), intent(LCMSWRAPPER_INTENT_DEFAULT), mode(CMYKCONVERSIONMODE_NORMAL),
	ignoreembedded(false), usedevicelink(false), width(0)
{
}


CMYKConversionOptions::CMYKConversionOptions(const CMYKConversionOptions &other)
	: profilemanager(other.profilemanager), inrgbprofile(other.inrgbprofile), incmykprofile(other.incmykprofile),
	outprofile(other.outprofile), devicelink(other.devicelink), intent(other.intent), mode(other.mode),
	ignoreembedded(other.ignoreembedded), usedevicelink(other.usedevicelink)
{
	Debug[TRACE] << "*** Copy constructor sees usedevicelink flag as " << usedevicelink << std::endl;
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
	devicelink=other.devicelink;
	ignoreembedded=other.ignoreembedded;
	usedevicelink=other.usedevicelink;
	Debug[TRACE] << "*** Assignment operator sees usedevicelink flag as " << usedevicelink << std::endl;
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


bool CMYKConversionOptions::GetUseDeviceLink()
{
	Debug[TRACE] << std::endl << "Getting usedl flag : " << usedevicelink << endl;
	return(usedevicelink);
}


const char *CMYKConversionOptions::GetDeviceLink()
{
	Debug[TRACE] << std::endl << "Getting devicelink name " << devicelink << endl;
	return(devicelink.c_str());
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


void CMYKConversionOptions::SetUseDeviceLink(bool usedl)
{
	Debug[TRACE] << "Setting usedl flag to " << usedl << std::endl;
	usedevicelink=usedl;
}


void CMYKConversionOptions::SetDeviceLink(const char *dl)
{
	if(dl)
		Debug[TRACE] << "Setting dl to " << dl << std::endl;
	else
		Debug[WARN] << "WARNING: Setting dl but no filename!" << std::endl;
	devicelink=dl;
}


//void CMYKConversionOptions::Save(const char *presetname)
//{
//}


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

	bool freeinprof=false;
	CMSProfile *inprof=NULL;
	if(!ignoreembedded)
		inprof=src->GetEmbeddedProfile();
	if(!inprof)
	{
		// Promote to RGB only if there's no embedded grey profile.
		// FIXME - support for default grey profile.

		if(STRIP_ALPHA(src->type)==IS_TYPE_GREY)
			src=new ImageSource_Promote(src,IS_TYPE_RGB);

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
	CMSProfile *outprof=NULL;
	CMSProfile *dlprof=NULL;

	Debug[TRACE] << "Opening profile: " << outprofile << endl;
	if(outprofile!=NOPROFILE_ESCAPESTRING)
		outprof=profilemanager.GetProfile(outprofile.c_str());
	if(GetUseDeviceLink())
		dlprof=profilemanager.GetProfile(GetDeviceLink());

	if(outprof && outprof->IsDeviceLink() && !dlprof)
	{
		dlprof=outprof;
		outprof=NULL;
	}

	// If we have a devicelink profile, we embed the output profile.
	if(dlprof)
	{
		Debug[TRACE] << "Using DeviceLink profile" << endl;
		if(factory)
		{
			Debug[TRACE] << "Getting transform from factory" << endl;
			CMSTransform *trans=factory->GetTransform(dlprof,NULL,intent);
			if(trans)
				src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,
					mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);
		}
		else
			src=new ImageSource_Deflatten(src,dlprof,NULL,mode==CMYKCONVERSIONMODE_HOLDBLACK,
				mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);

		src->SetEmbeddedProfile(outprof,true);
	}
	else if(outprof)
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
	else
	{
		// No output profile - use a naive conversion instead.
		CMSTransform *trans=new NaiveCMYKTransform(src);
		src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,
			mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY,width);
	}
	if(freeinprof)
		delete inprof;
	// We don't free the output profile - the ImageSource assumes ownership of it.
	if(dlprof)
		delete dlprof;

	return(src);
}

