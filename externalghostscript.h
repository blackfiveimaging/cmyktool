#ifndef EXTERNALGHOSTSCRIPT_H
#define EXTERNALGHOSTSCRIPT_H

#include "dirtreewalker.h"
#include "searchpath.h"
#include "debug.h"
#include "externalprog.h"
#include "util.h"

#ifdef WIN32
#include <w32api.h>
#define _WIN32_IE IE5
#define _WIN32_WINNT Windows2000
#include <shlobj.h>
#endif

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
		// FIXME - Need to scan user's homedir if Ghostscript's not found in Program Files

		static char programfiles[MAX_PATH]={0};
		SHGetFolderPath(NULL,CSIDL_PROGRAM_FILES,NULL,SHGFP_TYPE(SHGFP_TYPE_CURRENT),programfiles);

		Debug[TRACE] << "Program Files path: " << programfiles << std::endl;

		// Hunt for a Ghostscript Installation...
		DirTreeWalker dtw(programfiles);
		DirTreeWalker *w;
		while((w=dtw.NextDirectory()))
		{
			if(MatchBaseName("gs",w->Directory())==0)
			{
				std::string gspath=FindParent(w->Directory(),"gswin32");
				if(gspath.size())
				{
					Debug[TRACE] << "Adding " << gspath << " to search path" << std::endl;
					AddPath(gspath.c_str());
				}
			}
		}
		args[0]="gswin32.exe";
		Debug[TRACE] << "args[0] is now: " << args[0] << std::endl;
#else
		AddPath("/usr/bin:/usr/local/bin");
		args[0]="gs";
		Debug[TRACE] << "args[0] is now: " << args[0] << std::endl;
#endif
		Debug[TRACE] << GetPaths() << std::endl;
	}
	bool CheckPath()
	{
		char *path=SearchPaths(args[0].c_str());
		if(path)
		{
			free(path);
			return(true);
		}
		return(false);
	}
	protected:
};

#endif

