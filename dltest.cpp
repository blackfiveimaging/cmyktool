#include <iostream>

#include "configdb.h"
#include "profilemanager.h"
#include "devicelink.h"
#include "debug.h"
#include "binaryblob.h"

int main(int argc,char **argv)
{
	Debug.SetLevel(TRACE);
	try
	{
		if(argc>1)
		{
			BinaryBlob blob(argv[1]);
		}
#if 0
		ConfigFile conf;
		ProfileManager pm(&conf,"[Colour Management]");
		pm.SetInt("DefaultCMYKProfileActive",1);

		CMSProfile *src=pm.GetProfile(CM_COLOURDEVICE_DEFAULTRGB);
		CMSProfile *dst=pm.GetProfile(CM_COLOURDEVICE_DEFAULTCMYK);

		if(src)
			Debug[TRACE] << "Using source profile: " << src->GetMD5()->GetPrintableDigest() << std::endl;
		if(dst)
			Debug[TRACE] << "Using dest profile: " << dst->GetMD5()->GetPrintableDigest() << std::endl;

		DeviceLinkList devicelinks;
		Debug[TRACE] << "Got devicelink list with " << devicelinks.size() << " entries" << std::endl;

		DeviceLinkList devicelinksfiltered(src,dst);
		Debug[TRACE] << "Got filtered devicelink list with " << devicelinksfiltered.size() << " entries" << std::endl;
		
		DeviceLink dltest;
		dltest.SetString("Description","Test devicelink");
		dltest.SetString("SourceProfile",pm.FindString("DefaultRGBProfile"));
		dltest.SetString("DestProfile",pm.FindString("DefaultCMYKProfile"));

		if(argc>1)
			dltest.CreateDeviceLink("/usr/local/bin/:/usr/bin",pm);

		if(src)
			delete src;
		if(dst)
			delete dst;
#endif
	}
	catch (const char *err)
	{
		Debug[ERROR] << "Error: " << err << std::endl;
	}
	return(0);
}
