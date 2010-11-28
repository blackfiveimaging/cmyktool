#ifndef LIVEDISPLAYCHECK_H
#define LIVEDISPLAYCHECK_H

#ifdef WIN32
class LiveDisplayCheck
{
	public:
	bool HaveDisplay()
	{
		return(true);
	}
};
#else
#include <X11/Xlib.h>
class LiveDisplayCheck
{
	public:
	LiveDisplayCheck()
	{
		xdisplay = XOpenDisplay(NULL);
	}
	~LiveDisplayCheck()
	{
		if(xdisplay)
			XCloseDisplay(xdisplay);
	}
	bool HaveDisplay()
	{
		return(xdisplay!=0);
	}
	protected:
	Display *xdisplay;
};
#endif

extern LiveDisplayCheck LiveDisplay;

#endif

