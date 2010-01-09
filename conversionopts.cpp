#include <iostream>
#include <cstdlib>

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

using namespace std;



ConfigTemplate CMYKConversionPreset::Template[]=
{
	ConfigTemplate("PresetID",PRESET_PREVIOUS_ESCAPE),
	ConfigTemplate("DisplayName","unnamed preset"),
	ConfigTemplate("InProfile",BUILTINSRGB_ESCAPESTRING),
	ConfigTemplate("OutProfile","USWebCoatedSWOP.icc"),
	ConfigTemplate("Intent",0),
	ConfigTemplate("Mode",1),
	ConfigTemplate()
};


CMYKConversionOptions::CMYKConversionOptions(ProfileManager &pm)
	: profilemanager(pm), inprofile("sRGB Color Space Profile.icm"), outprofile("USWebCoatedSWOP.icc"), intent(LCMSWRAPPER_INTENT_DEFAULT), mode(CMYKCONVERSIONMODE_NORMAL)
{
}


CMYKConversionOptions::CMYKConversionOptions(const CMYKConversionOptions &other)
	: profilemanager(other.profilemanager), inprofile(other.inprofile), outprofile(other.outprofile), intent(other.intent), mode(other.mode)
{
}


CMYKConversionOptions::~CMYKConversionOptions()
{
}


CMYKConversionOptions &CMYKConversionOptions::operator=(const CMYKConversionOptions &other)
{
	mode=other.mode;
	intent=other.intent;
	inprofile=other.inprofile;
	outprofile=other.outprofile;
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


const char *CMYKConversionOptions::GetInProfile()
{
	return(inprofile.c_str());
}


const char *CMYKConversionOptions::GetOutProfile()
{
	return(outprofile.c_str());
}


void CMYKConversionOptions::SetIntent(LCMSWrapper_Intent intent)
{
	this->intent=intent;
}


void CMYKConversionOptions::SetMode(CMYKConversionMode mode)
{
	this->mode=mode;
}


void CMYKConversionOptions::SetInProfile(const char *in)
{
	if(in)
		inprofile=string(in);
}


void CMYKConversionOptions::SetOutProfile(const char *out)
{
	if(out)
		outprofile=string(out);
}


void CMYKConversionOptions::Save(const char *presetname)
{
}


IS_TYPE CMYKConversionOptions::GetOutputType()
{
	IS_TYPE type=IS_TYPE_RGB;
	if(GetMode()==CMYKCONVERSIONMODE_NONE)
		return(type);
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

	cerr << "Opening profile: " << inprofile << endl;

	bool freeinprof=false;
	CMSProfile *inprof=src->GetEmbeddedProfile();
	if(!inprof)
	{
		freeinprof=true;
		inprof=profilemanager.GetProfile(inprofile.c_str());
		if(!inprof)
			throw "Can't open input profile";
	}

	if(inprof->IsDeviceLink())
	{
		cerr << "Using DeviceLink profile" << endl;
		if(factory)
		{
			CMSTransform *trans=factory->GetTransform(NULL,inprof,intent);
			if(trans)
				src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY);
		}
		else
			src=new ImageSource_Deflatten(src,inprof,NULL,mode==CMYKCONVERSIONMODE_HOLDBLACK,mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY);
	}
	else
	{
		cerr << "Opening profile: " << outprofile << endl;
		CMSProfile *out=profilemanager.GetProfile(outprofile.c_str());
		if(!out)
			throw "Can't open output profile";
		if(factory)
		{
			CMSTransform *trans=factory->GetTransform(out,inprof,intent);
			if(trans)
				src=new ImageSource_Deflatten(src,trans,mode==CMYKCONVERSIONMODE_HOLDBLACK,mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY);
		}
		else
			src=new ImageSource_Deflatten(src,inprof,out,mode==CMYKCONVERSIONMODE_HOLDBLACK,mode==CMYKCONVERSIONMODE_OVERPRINT,mode==CMYKCONVERSIONMODE_HOLDGREY);
		src->SetEmbeddedProfile(out,true);
//		if(out)
//			delete out;
	}

	if(freeinprof)
		delete inprof;

	return(src);
}

