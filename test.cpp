#include <deque>
#include <iostream>

#include <unistd.h>

#include "searchpath.h"
#include "debug.h"
#include "externalprog.h"

class ExternalGhostScript : public ExternalProgram
{
	public:
	ExternalGhostScript() : ExternalProgram()
	{
		SetDefaultPath();
	}
	virtual ~ExternalGhostScript()
	{
	}
	virtual void SetDefaultPath()
	{
#ifdef WIN32
		// FIXME - need to find the path of Program Files, then scan for GhostScript.
		// Need to scan user's homedir if not found.
		AddPath("c:\Program Files\gs\bin");
		args[0]="gswin32c";
		Debug[TRACE] << "args[0] is now: " << args[0] << std::endl;
#else
		AddPath("/usr/bin:/usr/local/bin");
		args[0]="gs";
		Debug[TRACE] << "args[0] is now: " << args[0] << std::endl;
#endif
	}
	protected:
};


int main(int argc,char **argv)
{
	Debug.SetLevel(TRACE);
	try
	{
		ExternalGhostScript prog;
		prog.AddArg("test.ps");
		prog.RunProgram();
	}
	catch(const char *err)
	{
		Debug[ERROR] << "Error: " << err << std::endl;
	}
	return(0);
}

