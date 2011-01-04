/*
 * imagesource_deflatten.h
 * ImageSource Colour Management filter.
 *
 * Copyright (c) 2005 by Alastair M. Robinson
 * Distributed under the terms of the GNU General Public License -
 * see the file named "COPYING" for more details.
 *
 * TODO: Clean up handling of image types other than RGB
 */

#ifndef IMAGESOURCE_DEFLATTEN_H
#define IMAGESOURCE_DEFLATTEN_H

#include "imagesource.h"
#include "lcmswrapper.h"

#include <lcms.h>


class ImageSource_Deflatten : public ImageSource
{
	public:
	ImageSource_Deflatten(ImageSource *source,CMSProfile *inp,CMSProfile *outp,
		bool preserveblack=true,bool overprintblack=false,bool preservegrey=false,int effectwidth=0);
	ImageSource_Deflatten(ImageSource *source,RefCountPtr<CMSTransform> transform,
		bool preserveblack=true,bool overprintblack=false,bool preservegrey=false,int effectwidth=0);
	~ImageSource_Deflatten();
	ISDataType *GetRow(int row);
	private:
	void Init();
	ImageSource *source;
	RefCountPtr<CMSTransform> transform;
	bool preserveblack;
	bool overprintblack;
	bool preservegrey;
	int effectwidth;
	int tmpsourcespp;
	int tmpdestspp;
	unsigned short *tmp1;
	unsigned short *tmp2;
};

#endif
