/*
 * imagesource_extractchannel.h
 *
 * Supports RGB and CMYK data
 * Supports random access
 *
 * Copyright (c) 2004 by Alastair M. Robinson
 * Distributed under the terms of the GNU General Public License -
 * see the file named "COPYING" for more details.
 *
 */

#ifndef IMAGESOURCE_EXTRACTCHANNEL_H
#define IMAGESOURCE_EXTRACTCHANNEL_H

#include "imagesource.h"

class ImageSource_ExtractChannel : public ImageSource
{
	public:
	ImageSource_ExtractChannel(ImageSource *source,int channel);
	~ImageSource_ExtractChannel();
	ISDataType *GetRow(int row);
	private:
	ImageSource *source;
	int channel;
};

#endif
