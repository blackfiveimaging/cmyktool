#ifndef EXTERNALGHOSTSCRIPT_H
#define EXTERNALGHOSTSCRIPT_H

#include "dirtreewalker.h"
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
		// FIXME - Need to scan user's homedir if Ghostscript's not found in Program Files

		static char programfiles[MAX_PATH]={0};
		SHGetFolderPath(NULL,CSIDL_PROGRAM_FILES,NULL,SHGFP_TYPE(SHGFP_TYPE_CURRENT),programfile);

		// Hunt for a Ghostscript Installation...
		DirTreeWalker dtw(programfiles);
		DirTreeWalker *w;
		while((w=dtw.NextDirectory()))
		{
			Debug[TRACE] << "Checking path: " << w->c_str() << std::endl;
			const char *p=w->c_str();
			if(strncasecmp(p,"GS",2)==0)
			{
				std::string path=p+std::string("/bin/");
				Debug[TRACE] << "Adding " << path << " to search path" << std::endl;
				prog.AddPath(path.c_str());
			}
		}
		args[0]="gswin32c";
		Debug[TRACE] << "args[0] is now: " << args[0] << std::endl;
#else
		AddPath("/usr/bin:/usr/local/bin");
		args[0]="gs";
		Debug[TRACE] << "args[0] is now: " << args[0] << std::endl;
#endif
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

