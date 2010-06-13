#include <iostream>

#include "configdb.h"
#include "profilemanager.h"
#include "devicelink.h"
#include "debug.h"

int main(int argc,char **argv)
{
	ConfigFile conf;
	ProfileManager pm(&conf,"[Colour Management]");

	Debug.SetLevel(TRACE);
	DeviceLinkList devicelinks;

	DeviceLink dltest;

	dltest.CreateDeviceLink(pm);
//	dltest.Save();
	return(0);
}
