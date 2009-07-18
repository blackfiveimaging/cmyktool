#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "imagesource_combine.h"

using namespace std;

ImageSource_Combine::~ImageSource_Combine()
{
	if(prev)
		delete prev;

	if(source)
		delete source;
}


ISDataType *ImageSource_Combine::GetRow(int row)
{
	int i;

	float cc,cm,cy,ck;
	float sc,sm,sy,sk;
	float c,m,y,k;
	ISDataType *rb;
	ISDataType *dst;
	ISDataType *src=source->GetRow(row);

	if(prev)
	{
		dst=rb=prev->GetRow(row);
	}
	else
	{
		dst=rb=rowbuffer;
		for(i=0;i<width;++i)
		{
		  dst[i*4]=0.0;
		  dst[i*4+1]=0.0;
		  dst[i*4+2]=0.0;
		  dst[i*4+3]=0.0;
		}
	}

	cc=colour[0]; cc/=IS_SAMPLEMAX;
	cm=colour[1]; cm/=IS_SAMPLEMAX;
	cy=colour[2]; cy/=IS_SAMPLEMAX;
	ck=colour[3]; ck/=IS_SAMPLEMAX;

	switch(source->type)
	{
		case IS_TYPE_GREY:
			for(i=0;i<width;++i)
			{
				sc=*src++;

				c=sc*cc;
				m=sc*cm;
				y=sc*cy;
				k=sc*ck;

				(*dst++)+=c;
				(*dst++)+=m;
				(*dst++)+=y;
				(*dst++)+=k;
			}
			break;
		case IS_TYPE_CMYK:
			for(i=0;i<width;++i)
			{
				sc=*src++;
				sm=*src++;
				sy=*src++;
				sk=*src++;

				c=sc*cc;
				m=sm*cm;
				y=sy*cy;
				k=sk*ck;

				(*dst++)+=c;
				(*dst++)+=m;
				(*dst++)+=y;
				(*dst++)+=k;
			}
			break;
		default:
			break;
	}
	return(rb);
}


ImageSource_Combine::ImageSource_Combine(struct ImageSource *source,ISDeviceNValue &colour,struct ImageSource *prev,IS_TYPE type,int channels) :
	ImageSource(source), source(source), colour(colour), prev(prev)
{
	this->type=type;
	switch(type)
	{
		case IS_TYPE_RGB:
			samplesperpixel=3;
			break;
		case IS_TYPE_RGBA:
			samplesperpixel=4;
			break;
		case IS_TYPE_CMYK:
			samplesperpixel=4;
			break;
		case IS_TYPE_CMYKA:
			samplesperpixel=5;
			break;
		case IS_TYPE_DEVICEN:
			samplesperpixel=channels;
			break;
		default:
			throw "ImageSource_Combine: Unsupported type";
			break;
	}

	MakeRowBuffer();
}

