/*
 * imagesource_extractchannel.cpp
 *
 * Supports Greyscale, RGB and CMYK data
 * Supports random access
 *
 * Copyright (c) 2004 by Alastair M. Robinson
 * Distributed under the terms of the GNU General Public License -
 * see the file named "COPYING" for more details.
 *
 */

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "imagesource_extractchannel.h"

using namespace std;

ImageSource_ExtractChannel::~ImageSource_ExtractChannel()
{
	if(source)
		delete source;
}


ISDataType *ImageSource_ExtractChannel::GetRow(int row)
{
	int i;

	if(row==currentrow)
		return(rowbuffer);

	ISDataType *srcdata=source->GetRow(row);

	for(i=0;i<width;++i)
	{
		rowbuffer[i]=srcdata[i*source->samplesperpixel+channel];
	}

	currentrow=row;

	return(rowbuffer);
}


ImageSource_ExtractChannel::ImageSource_ExtractChannel(struct ImageSource *source,int channel)
	: ImageSource(source), source(source), channel(channel)
{
	samplesperpixel=1;
	type=IS_TYPE_GREY;
	MakeRowBuffer();
}
