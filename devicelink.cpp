#include <iostream>

#include "debug.h"
#include "util.h"
#include "md5.h"
#include "tempfile.h"
#include "externalprog.h"
#include "binaryblob.h"
#include "errordialogqueue.h"

#include "devicelink.h"

ConfigTemplate DeviceLink::configtemplate[]=
{
	ConfigTemplate("Description",""),
	ConfigTemplate("SourceProfile",""),
	ConfigTemplate("SourceProfileHash",""),
	ConfigTemplate("DestProfile",""),
	ConfigTemplate("DestProfileHash",""),
	ConfigTemplate("RenderingIntent",int(LCMSWRAPPER_INTENT_PERCEPTUAL)),
	ConfigTemplate("SourceViewingConditions",""),
	ConfigTemplate("DestViewingConditions",""),
	ConfigTemplate("BlackGeneration",""),
	ConfigTemplate("InkLimit",0),
	ConfigTemplate("Quality",0),
	ConfigTemplate("Pending",1),
	ConfigTemplate()
};


class TempProfile : public TempFile
{
	public:
	TempProfile(CMSProfile *profile) : TempFile(NULL,"cktl")
	{
		if(!profile)
			throw "TempProfile - must provide a CMSProfile!";
		Debug[TRACE] << "Saving profile to " << Filename() << std::endl;
		profile->Save(Filename());
	}
	protected:
};


#ifdef WIN32
// We use this to provide a filename that can be presented to collink on the command line without hitting
// unicode character problems.  Once the devicelink's created we copy it to its destination.

class TempDeviceLink : public TempFile
{
	public:
	TempDeviceLink(std::string finalfilename) : TempFile(NULL,"cktl"), finalfn(finalfilename)
	{
	}
	void Save()	// Since BinaryBlob can throw exceptions, it's not safe to do this from the destructor!
	{
		Debug[TRACE] << "TempDeviceLink: Loading blob from " << Filename() << std::endl;
		BinaryBlob dl(Filename());
		dl.Save(finalfn.c_str());
	}
	~TempDeviceLink()
	{
	}
	protected:
	std::string finalfn;
};
#endif


DeviceLink::DeviceLink(const char *filename) : ConfigFile(), ConfigDB(configtemplate)
{
	new ConfigDBHandler(this,"[DeviceLink]",this);

	if(filename)
	{
		fn=filename;
		char *fn2=BuildFilename(filename,NULL,"dlm");
		metadata_fn=fn2;
		Debug[TRACE] << "Loading metadata from file " << fn2 << std::endl;
		ParseConfigFile(fn2);
		free(fn2);
		if(FindInt("Pending"))
			fn="";
		blackgen.SetCmdLine(FindString("BlackGeneration"));
	}
	else
		makefilename();
}


DeviceLink::~DeviceLink()
{

}


void DeviceLink::SetBlackGen(Argyll_BlackGenerationCurve blackgen)
{
	this->blackgen=blackgen;
	SetString("BlackGeneration",blackgen.GetCmdLine().c_str());
}


Argyll_BlackGenerationCurve &DeviceLink::GetBlackGen()
{
	return(blackgen);
}


void DeviceLink::Save(const char *filename)
{
	if(!filename)
	{
		if(!metadata_fn.size())
			makefilename();
		filename=metadata_fn.c_str();
	}
	SaveConfigFile(filename);
}


void DeviceLink::Save(DeviceLinkList_Entry &entry)
{
	fn=entry.filename;
	char *fn2=BuildFilename(fn.c_str(),NULL,"dlm");
	metadata_fn=fn2;
	free(fn2);
	SaveConfigFile(metadata_fn.c_str());
}


bool DeviceLink::DeviceLinkBuilt()
{
	if(fn.size()==0)
		return(false);
	return(CheckFileExists(fn.c_str()));
}


void DeviceLink::Export(const char *exportfilename)
{
	if(exportfilename)
	{
		if(!DeviceLinkBuilt())
			throw "Devicelink hasn't been built yet - can't export!";
		BinaryBlob blob(fn.c_str());
		blob.Save(exportfilename);
	}
}


void DeviceLink::Delete()
{
	if(fn.size())
	{
		if(CheckFileExists(fn.c_str()))
			remove(fn.c_str());
		else
			Debug[ERROR] << "Error - can't delete " << fn << " - it doesn't exist!" << std::endl;
	}
	if(metadata_fn.size())
	{
		if(CheckFileExists(metadata_fn.c_str()))
			remove(metadata_fn.c_str());
		else
			Debug[ERROR] << "Error - can't delete " << metadata_fn << " - it doesn't exist!" << std::endl;
	}
}


bool DeviceLink::CheckArgyllPath(std::string argyllpath)
{
#ifdef WIN32
	const char *cmd="collink.exe";
#else
	const char *cmd="collink";
#endif
	SearchPathHandler sp;
	sp.AddPath(argyllpath.c_str());
	char *prgname=sp.SearchPaths(cmd);
	if(prgname)
	{
		free(prgname);
		return(true);
	}
	return(false);
}


void DeviceLink::CreateDeviceLink(std::string argyllpath, ProfileManager &pm)
{
	CMSProfile *tmp;
	tmp=pm.GetProfile(FindString("SourceProfile"));
	if(!tmp)
		throw _("Devicelink: Can't open source profile!");
	TempProfile src(tmp);
	SetString("SourceProfileHash",tmp->GetMD5()->GetPrintableDigest());
	delete tmp;

	tmp=pm.GetProfile(FindString("DestProfile"));
	if(!tmp)
		throw _("Devicelink: Can't open destination profile!");
	IS_TYPE type=tmp->GetColourSpace();
	TempProfile dst(tmp);
	SetString("DestProfileHash",tmp->GetMD5()->GetPrintableDigest());
	delete tmp;

	ExternalProgram collink;
	collink.AddPath(argyllpath.c_str());
//	collink.AddPath("/usr/local/bin:/usr/bin/");
#ifdef WIN32
	collink.AddArg("collink.exe");
#else
	collink.AddArg("collink");
#endif

	SetInt("Pending",1);
	Save();

	switch(DeviceLink_Quality(FindInt("Quality")))
	{
		case DEVICELINK_QUALITY_LOW:
			collink.AddArg("-ql");
			collink.AddArg("-g");
			break;
		case DEVICELINK_QUALITY_MEDIUM:
			collink.AddArg("-qm");
			collink.AddArg("-G");
			break;
		case DEVICELINK_QUALITY_HIGH:
			collink.AddArg("-qh");
			collink.AddArg("-G");
			break;
	}

	LCMSWrapper_Intent intent=LCMSWrapper_Intent(FindInt("RenderingIntent"));
	std::string intentarg;
	switch(intent)
	{
		case LCMSWRAPPER_INTENT_DEFAULT:
		case LCMSWRAPPER_INTENT_PERCEPTUAL:
			intentarg="-ip";
			break;
		case LCMSWRAPPER_INTENT_SATURATION:
			intentarg="-is";
			break;
		case LCMSWRAPPER_INTENT_NONE:
		case LCMSWRAPPER_INTENT_RELATIVE_COLORIMETRIC:
		case LCMSWRAPPER_INTENT_RELATIVE_COLORIMETRIC_BPC:
			intentarg="-ir";
			break;
		case LCMSWRAPPER_INTENT_ABSOLUTE_COLORIMETRIC:
			intentarg="-ia";
			break;
	}
	collink.AddArg(intentarg);

	std::string viewcond=FindString("SourceViewingConditions");
	if(viewcond.size()>0)
		collink.AddArg("-c"+viewcond);
	viewcond=FindString("DestViewingConditions");
	if(viewcond.size()>0)
		collink.AddArg("-d"+viewcond);

	std::string blackgen=FindString("BlackGeneration");
	if(blackgen.size()>0)
	{
		// We have to split the black generation argument into individual args, or Argyll can't find them!
		std::string tmp=blackgen;
		int argstart=0;
		int argend=tmp.find(' ',argstart);
		while(argend!=std::string::npos)
		{
			Debug[TRACE] << "Adding argument from " << argstart << " to " << argend << ", " << tmp.substr(argstart,argend-argstart) << std::endl;
			collink.AddArg(tmp.substr(argstart,argend-argstart));
			argstart=argend+1;
			argend=tmp.find(' ',argstart);
		}
	}

	// We only use ink limiting if linking to a CMYK target - we disable it for RGB.
	if(type==IS_TYPE_CMYK)
	{
		int inklimit=FindInt("InkLimit");
		if(inklimit>0)
		{
			char buf[10];
			sprintf(buf,"-l%d",inklimit);
			collink.AddArg(buf);
		}
	}
#if WIN32	// Quote filenames in case they contain spaces...
	{
		// Avoid the command-line-and-wide-char-filenames issue on WIN32...
		TempDeviceLink tempdl(fn);
		try
		{
			collink.AddArg("-D"+ShellQuote(FindString("Description")));
			collink.AddArg(ShellQuote(src.Filename()));
			collink.AddArg(ShellQuote(dst.Filename()));
			collink.AddArg(ShellQuote(tempdl.Filename()));
			collink.RunProgram();
			tempdl.Save();
		}
		catch(const char *err)
		{
			ErrorDialogs.AddMessage(err);
		}
	}
#else
	collink.AddArg("-D"+std::string(FindString("Description")));
	collink.AddArg(src.Filename());
	collink.AddArg(dst.Filename());
	collink.AddArg(fn);
	collink.RunProgram();
#endif
	SetInt("Pending",0);
	Save();		// Save a second time with the pending flag removed.
}


void DeviceLink::makefilename()
{
	char *path=substitute_xdgconfighome(DEVICELINK_CACHE_PATH);
	CreateDirIfNeeded(path);
	// Generate new presetid here
	// Yeah, using an MD5 digest of a random buffer is over-engineering, but it should work...
	std::string presetname;
	char buf[32];
	MD5Digest dig;
	do
	{
		for(int i=0;i<32;++i)
			buf[i]=char(RandomSeeded(255));
		dig.Update(buf,32);
		Debug[TRACE] << "Building filename using random hash" << std::endl;
		fn=path + std::string("/") + dig.GetPrintableDigest() + ".icc";
	} while(CheckFileExists(fn.c_str()));

	char *fn2=BuildFilename(fn.c_str(),NULL,"dlm");
	metadata_fn=fn2;
	free(fn2);

	free(path);
}
