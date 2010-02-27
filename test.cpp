#include <deque>
#include <iostream>

#include <unistd.h>

#include "debug.h"
#include "externalghostscript.h"



int main(int argc,char **argv)
{
	Debug.SetLevel(TRACE);
	try
	{
		ExternalGhostScript prog;
		Debug[TRACE] << "Able to locate GhostScript executable? " << prog.CheckPath() << std::endl;
		prog.AddArg("test.ps");
		prog.RunProgram();
	}
	catch(const char *err)
	{
		Debug[ERROR] << "Error: " << err << std::endl;
	}
	return(0);
}

