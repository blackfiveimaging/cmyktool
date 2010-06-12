#ifndef DEVICELINK_H

// Devicelink caching:
// Store devicelinks and metadata in $XDG_CONFIG_HOME/cmyktool/devicelinks
DEVICELINK_CACHE_PATH=$XDG_CONFIG_HOME/cmyktool/devicelinks


class DeviceLink : public ConfigFile, public ConfigDB
{
	public:
	DeviceLink(const char *filename=NULL);
	~DeviceLink();
	void Save(const char *filename=NULL);
	Argyll_BlackGenerationCurve blackgen;
	void CreateDeviceLink();
	CMTransform *GetTransform();
	private:
	string fn;
	static ConfigTemplate configtemplate[];
};

ConfigTemplate DeviceLink::configtemplate[]=
{
	ConfigTemplate("Description",""),
	ConfigTemplate("SrcProfile",""),
	ConfigTemplate("DestProfile",""),
	ConfigTemplate("SourceViewingConditions",""),
	ConfigTemplate("DestViewingConditions",""),
	ConfigTemplate("BlackGeneration",""),
	ConfigTemplate();
}


class DeviceLinkList_Entry
{
	public:
	DeviceLinkList_Entry(const char *fn,const char *dn) : filename(fn), displayname(dn)
	{
	}
	std::string filename;
	std::string displayname;
};


class DeviceLinkList : public std::deque<DeviceLinkList_Entry>
{
	public:
	DeviceLinkList() : previdx(-1)
	{
		Debug[TRACE] << "Creating preset list..." << std::endl;
		char *configdir=substitute_xdgconfighome(CMYKCONVERSIONOPTS_PRESET_PATH);
		DirTreeWalker dtw(configdir);
		const char *fn;

		while((fn=dtw.NextFile()))
		{
			Debug[TRACE] << "Adding file " << fn << std::endl;
			DeviceLink dl(fn);

			const char *dn=dl.FindString("Description");
			if(!(dn && strlen(dn)>0))
				dn=_("<unknown>");

			DeviceLinkList_Entry e(fn,dn);
			push_back(e);
		}
		free(configdir);
		Debug[TRACE] << "Done - index of previous file is " << previdx << std::endl;
	}
};


#endif

