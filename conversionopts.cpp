#include <iostream>
#include <cstdlib>

#include "support/util.h"
#include "conversionopts.h"

using namespace std;

CMYKConversionOptions::CMYKConversionOptions(const char *presetname)
	: inprofile(NULL), outprofile(NULL), intent(LCMSWRAPPER_INTENT_DEFAULT), mode(CMYKCONVERSIONMODE_NORMAL)
{

}


CMYKConversionOptions::CMYKConversionOptions(const CMYKConversionOptions &other)
	: inprofile(NULL), outprofile(NULL), intent(other.intent), mode(other.mode)
{
	if(other.inprofile)
		inprofile=strdup(other.inprofile);
	if(other.outprofile)
		outprofile=strdup(other.outprofile);
}


CMYKConversionOptions::~CMYKConversionOptions()
{
	if(inprofile)
		free(inprofile);
	if(outprofile)
		free(outprofile);
}


CMYKConversionOptions &CMYKConversionOptions::operator=(const CMYKConversionOptions &other)
{
	mode=other.mode;
	intent=other.intent;

	if(inprofile)
		free(inprofile);
	inprofile=NULL;
	if(outprofile)
		free(outprofile);
	outprofile=NULL;

	if(other.inprofile)
		inprofile=strdup(other.inprofile);
	if(other.outprofile)
		outprofile=strdup(other.outprofile);
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
	return(inprofile);
}


const char *CMYKConversionOptions::GetOutProfile()
{
	return(outprofile);
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
	if(inprofile)
		free(inprofile);
	inprofile=NULL;

	if(in)
		inprofile=strdup(in);
}


void CMYKConversionOptions::SetOutProfile(const char *out)
{
	if(outprofile)
		free(outprofile);
	outprofile=NULL;

	if(out)
		outprofile=strdup(out);
}


void CMYKConversionOptions::Save(const char *presetname)
{
}


ImageSource *CMYKConversionOptions::Apply(ImageSource *src,ImageSource *mask)
{
}

